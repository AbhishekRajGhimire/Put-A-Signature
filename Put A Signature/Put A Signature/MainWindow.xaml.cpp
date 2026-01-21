#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "SignatureCapture.h"

#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Security.Cryptography.h>

#include <shobjidl.h> // IInitializeWithWindow
#include <microsoft.ui.xaml.window.h> // IWindowNative

#include <cwchar> // swprintf_s

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::Put_A_Signature::implementation
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Media::Imaging;

    namespace
    {
        template <typename TPicker>
        void InitializePickerWithWindow(TPicker const& picker, HWND hwnd)
        {
            auto initializeWithWindow = picker.as<IInitializeWithWindow>();
            check_hresult(initializeWithWindow->Initialize(hwnd));
        }

        HWND GetWindowHwnd(Microsoft::UI::Xaml::Window const& window)
        {
            HWND hwnd{};
            check_hresult(window.as<IWindowNative>()->get_WindowHandle(&hwnd));
            return hwnd;
        }

        // Your toolchain doesn't provide winrt::resume_foreground for Microsoft::UI::Dispatching::DispatcherQueue.
        // This is a small awaitable that switches execution back to the UI thread via DispatcherQueue::TryEnqueue.
        [[nodiscard]] inline auto ResumeForeground(
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
            Microsoft::UI::Dispatching::DispatcherQueuePriority const priority = Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal) noexcept
        {
            struct awaitable
            {
                awaitable(Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
                    Microsoft::UI::Dispatching::DispatcherQueuePriority const priority) noexcept :
                    m_dispatcher(dispatcher),
                    m_priority(priority)
                {
                }

                bool await_ready() const noexcept
                {
                    return false;
                }

                bool await_resume() const noexcept
                {
                    return m_queued;
                }

                bool await_suspend(winrt::impl::coroutine_handle<> handle)
                {
                    return m_dispatcher.TryEnqueue(m_priority, [handle, this]
                    {
                        m_queued = true;
                        handle();
                    });
                }

            private:
                Microsoft::UI::Dispatching::DispatcherQueue const& m_dispatcher;
                Microsoft::UI::Dispatching::DispatcherQueuePriority const m_priority;
                bool m_queued{};
            };

            return awaitable{ dispatcher, priority };
        }
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Enable Windows 11 Mica backdrop (best-effort).
        try
        {
            this->SystemBackdrop(Microsoft::UI::Xaml::Media::MicaBackdrop{});
        }
        catch (...)
        {
            // Older OS/build or missing support; ignore.
        }

        // Initial UI state.
        m_pageCount = 0;
        m_currentPageIndex = 0;
        SetEmptyStateVisible(true);
        UpdateNavigationUi();

        StatusText().Text(L"Ready");
    }

    void MainWindow::SetEmptyStateVisible(bool visible)
    {
        EmptyStatePanel().Visibility(visible ? Visibility::Visible : Visibility::Collapsed);
    }

    void MainWindow::UpdateNavigationUi()
    {
        const bool loaded = m_pdf.IsLoaded() && m_pageCount > 0;

        PrevPageButton().IsEnabled(loaded && m_currentPageIndex > 0);
        NextPageButton().IsEnabled(loaded && (m_currentPageIndex + 1) < m_pageCount);

        PlaceSignatureButton().IsEnabled(loaded);
        SaveSignedPdfButton().IsEnabled(loaded);

        if (loaded)
        {
            PageNumberBox().Text(to_hstring(m_currentPageIndex + 1));
            PageCountText().Text(to_hstring(m_pageCount));
        }
        else
        {
            PageNumberBox().Text(L"1");
            PageCountText().Text(L"-");
        }
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::LoadPdfFromFileAsync(Windows::Storage::StorageFile const& file)
    {
        if (!file) co_return;

        StatusText().Text(L"Loading PDF...");

        bool loaded = false;
        std::wstring errorMessage{};

        winrt::Windows::Storage::Streams::IBuffer fileBuffer{ nullptr };

        try
        {
            // Packaged-app friendly: read via StorageFile and load PDF from memory.
            fileBuffer = co_await winrt::Windows::Storage::FileIO::ReadBufferAsync(file);
            winrt::com_array<uint8_t> bytes;
            winrt::Windows::Security::Cryptography::CryptographicBuffer::CopyToByteArray(fileBuffer, bytes);
            m_pdf.LoadFromBytes(std::vector<uint8_t>(bytes.begin(), bytes.end()));
            m_pageCount = m_pdf.PageCount();
            m_currentPageIndex = 0;
            loaded = true;
        }
        catch (std::exception const& ex)
        {
            errorMessage = winrt::to_hstring(ex.what());
        }
        catch (...)
        {
            errorMessage = L"Failed to load PDF";
        }

        if (!loaded)
        {
            DocInfoText().Text(L"(no file loaded)");
            SetEmptyStateVisible(true);
            UpdateNavigationUi();
            StatusText().Text(errorMessage.empty() ? L"Failed to load PDF" : winrt::hstring(errorMessage));
            co_return;
        }

        DocInfoText().Text(file.Name());
        SetEmptyStateVisible(false);
        UpdateNavigationUi();
        co_await RenderCurrentPageAsync();
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::RenderCurrentPageAsync()
    {
        if (!m_pdf.IsLoaded() || m_pageCount <= 0) co_return;

        StatusText().Text(L"Rendering...");

        const int32_t pageIndex = m_currentPageIndex;

        Windows::Graphics::Imaging::SoftwareBitmap pageBitmap{ nullptr };

        // 1) Render via PDFium
        try
        {
            pageBitmap = m_pdf.RenderPageToSoftwareBitmap(pageIndex, 2.0f /*scale*/);
        }
        catch (std::exception const& ex)
        {
            std::wstring msg = L"PDF render failed: ";
            msg += winrt::to_hstring(ex.what()).c_str();
            StatusText().Text(winrt::hstring(msg));
            co_return;
        }
        catch (...)
        {
            StatusText().Text(L"PDF render failed (unknown error)");
            co_return;
        }

        // 2) Display in WinUI (this can fail with HRESULTs if the bitmap is incompatible)
        try
        {
            Microsoft::UI::Xaml::Media::Imaging::SoftwareBitmapSource source;
            co_await source.SetBitmapAsync(pageBitmap);
            PdfPageImage().Source(source);
            StatusText().Text(L"Ready");
        }
        catch (winrt::hresult_error const& e)
        {
            wchar_t hex[11]{};
            swprintf_s(hex, L"%08X", static_cast<uint32_t>(e.code().value));

            std::wstring msg = L"Display failed: 0x";
            msg += hex;
            msg += L" ";
            msg += e.message().c_str();
            StatusText().Text(winrt::hstring(msg));
        }
        catch (std::exception const& ex)
        {
            std::wstring msg = L"Display failed: ";
            msg += winrt::to_hstring(ex.what()).c_str();
            StatusText().Text(winrt::hstring(msg));
        }
        catch (...)
        {
            StatusText().Text(L"Display failed (unknown error)");
        }
    }

    winrt::fire_and_forget MainWindow::OpenPdfButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        StatusText().Text(L"Picking PDF...");

        Windows::Storage::Pickers::FileOpenPicker picker;
        picker.FileTypeFilter().Append(L".pdf");

        HWND hwnd = GetWindowHwnd(*this);
        InitializePickerWithWindow(picker, hwnd);

        Windows::Storage::StorageFile file = co_await picker.PickSingleFileAsync();
        if (!file)
        {
            StatusText().Text(L"Open canceled");
            co_return;
        }

        co_await LoadPdfFromFileAsync(file);
    }

    winrt::fire_and_forget MainWindow::PlaceSignatureButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (!m_pdf.IsLoaded())
        {
            StatusText().Text(L"Load a PDF first");
            co_return;
        }

        StatusText().Text(L"Capturing signature...");

        Windows::Graphics::Imaging::SoftwareBitmap signatureBitmap =
            co_await SignatureCapture::CaptureElementAsync(SignatureCanvas());

        // Minimal placeholder placement: stamp near bottom-left-ish.
        // NOTE: PDF coordinate system origin is bottom-left; UI is top-left.
        // You will likely replace this with proper UI->PDF coordinate conversion.
        PdfRect rectInPdfPoints{};
        rectInPdfPoints.x = 72;        // 1 inch from left
        rectInPdfPoints.y = 72;        // 1 inch from bottom
        rectInPdfPoints.width = 200;   // points
        rectInPdfPoints.height = 80;   // points

        StatusText().Text(L"Stamping signature...");
        m_pdf.StampSignatureBitmap(m_currentPageIndex, signatureBitmap, rectInPdfPoints);

        // Re-render the page so the user sees the result.
        co_await RenderCurrentPageAsync();

        StatusText().Text(L"Signature placed");
    }

    winrt::fire_and_forget MainWindow::SaveSignedPdfButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (!m_pdf.IsLoaded())
        {
            StatusText().Text(L"Load a PDF first");
            co_return;
        }

        StatusText().Text(L"Picking save location...");

        Windows::Storage::Pickers::FileSavePicker picker;
        picker.SuggestedFileName(L"signed");
        picker.FileTypeChoices().Insert(L"PDF Document", single_threaded_vector<hstring>({ L".pdf" }));

        HWND hwnd = GetWindowHwnd(*this);
        InitializePickerWithWindow(picker, hwnd);

        Windows::Storage::StorageFile outFile = co_await picker.PickSaveFileAsync();
        if (!outFile)
        {
            StatusText().Text(L"Save canceled");
            co_return;
        }

        StatusText().Text(L"Saving...");
        m_pdf.SaveAs(outFile.Path().c_str());

        StatusText().Text(L"Saved");
    }

    void MainWindow::ClearSignatureButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        SignatureCanvas().Children().Clear();
        StatusText().Text(L"Signature cleared");
    }

    void MainWindow::PrevPageButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        if (!m_pdf.IsLoaded() || m_pageCount <= 0) return;
        if (m_currentPageIndex <= 0) return;
        m_currentPageIndex--;
        UpdateNavigationUi();
        RenderCurrentPageAsync();
    }

    void MainWindow::NextPageButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        if (!m_pdf.IsLoaded() || m_pageCount <= 0) return;
        if ((m_currentPageIndex + 1) >= m_pageCount) return;
        m_currentPageIndex++;
        UpdateNavigationUi();
        RenderCurrentPageAsync();
    }

    winrt::fire_and_forget MainWindow::PageNumberBox_KeyDown(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        auto lifetime = get_strong();
        if (args.Key() != Windows::System::VirtualKey::Enter) co_return;
        if (!m_pdf.IsLoaded() || m_pageCount <= 0) co_return;

        try
        {
            int32_t requested = std::stoi(std::wstring(PageNumberBox().Text()));
            if (requested < 1) requested = 1;
            if (requested > m_pageCount) requested = m_pageCount;

            m_currentPageIndex = requested - 1;
            UpdateNavigationUi();
            co_await RenderCurrentPageAsync();
        }
        catch (...)
        {
            // Reset textbox to current value.
            UpdateNavigationUi();
        }
    }

    void MainWindow::PdfDropZone_DragOver(Windows::Foundation::IInspectable const&, DragEventArgs const& e)
    {
        auto def = e.GetDeferral();
        e.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
        e.DragUIOverride().Caption(L"Open PDF");
        e.DragUIOverride().IsCaptionVisible(true);
        e.DragUIOverride().IsGlyphVisible(true);
        e.Handled(true);
        def.Complete();
    }

    winrt::fire_and_forget MainWindow::PdfDropZone_Drop(Windows::Foundation::IInspectable const&, DragEventArgs const& e)
    {
        auto lifetime = get_strong();
        auto def = e.GetDeferral();

        try
        {
            auto items = co_await e.DataView().GetStorageItemsAsync();
            if (items.Size() == 0)
            {
                def.Complete();
                co_return;
            }

            auto file = items.GetAt(0).try_as<Windows::Storage::StorageFile>();
            if (!file)
            {
                def.Complete();
                co_return;
            }

            // Basic filter: only PDFs.
            if (file.FileType() != L".pdf")
            {
                StatusText().Text(L"Please drop a .pdf file");
                def.Complete();
                co_return;
            }

            co_await LoadPdfFromFileAsync(file);
        }
        catch (...)
        {
            StatusText().Text(L"Drop failed");
        }

        def.Complete();
    }

    void MainWindow::SignatureCanvas_PointerPressed(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        // Start a new stroke.
        auto pt = args.GetCurrentPoint(SignatureCanvas());
        m_activePointerId = pt.PointerId();
        m_isDrawing = true;

        SignatureCanvas().CapturePointer(args.Pointer());

        Microsoft::UI::Xaml::Shapes::Polyline line;
        line.StrokeThickness(3.0);
        line.StrokeLineJoin(Microsoft::UI::Xaml::Media::PenLineJoin::Round);
        line.StrokeStartLineCap(Microsoft::UI::Xaml::Media::PenLineCap::Round);
        line.StrokeEndLineCap(Microsoft::UI::Xaml::Media::PenLineCap::Round);
        line.Stroke(Microsoft::UI::Xaml::Media::SolidColorBrush(Microsoft::UI::Colors::Black()));

        auto points = line.Points();
        points.Append(pt.Position());

        SignatureCanvas().Children().Append(line);
        m_activeStroke = line;

        args.Handled(true);
    }

    void MainWindow::SignatureCanvas_PointerMoved(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!m_isDrawing || !m_activeStroke) return;

        auto pt = args.GetCurrentPoint(SignatureCanvas());
        if (pt.PointerId() != m_activePointerId) return;

        auto points = m_activeStroke.Points();
        points.Append(pt.Position());

        args.Handled(true);
    }

    void MainWindow::SignatureCanvas_PointerReleased(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        auto pt = args.GetCurrentPoint(SignatureCanvas());
        if (pt.PointerId() == m_activePointerId)
        {
            m_isDrawing = false;
            m_activePointerId = 0;
            m_activeStroke = nullptr;
            SignatureCanvas().ReleasePointerCapture(args.Pointer());
        }
        args.Handled(true);
    }

    void MainWindow::SignatureCanvas_PointerCanceled(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        m_isDrawing = false;
        m_activePointerId = 0;
        m_activeStroke = nullptr;
        SignatureCanvas().ReleasePointerCapture(args.Pointer());
        args.Handled(true);
    }
}
