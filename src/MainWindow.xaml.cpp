#include "pch.h"
#include "MainWindow.xaml.h"

#include "SignatureCapture.h"

#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>

#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>

#include <shobjidl.h> // IInitializeWithWindow
#include <microsoft.ui.xaml.window.h> // IWindowNative

using namespace winrt;
using namespace Microsoft::UI;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Microsoft::UI::Composition::SystemBackdrops;

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
}

namespace winrt::PutASignature::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Enable Windows 11 Mica backdrop (best-effort).
        try
        {
            this->SystemBackdrop(MicaBackdrop{});
        }
        catch (...)
        {
            // Older OS/build or missing support; ignore.
        }

        // Configure InkCanvas for pen + mouse.
        auto presenter = SignatureInkCanvas().InkPresenter();
        presenter.InputDeviceTypes(
            Windows::UI::Core::CoreInputDeviceTypes::Mouse |
            Windows::UI::Core::CoreInputDeviceTypes::Pen);

        StatusText().Text(L"Ready");
    }

    winrt::fire_and_forget MainWindow::OpenPdfButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        StatusText().Text(L"Picking PDF...");

        Windows::Storage::Pickers::FileOpenPicker picker;
        picker.FileTypeFilter().Append(L".pdf");

        // WinUI 3 requires initializing pickers with the HWND.
        HWND hwnd = GetWindowHwnd(*this);
        InitializePickerWithWindow(picker, hwnd);

        Windows::Storage::StorageFile file = co_await picker.PickSingleFileAsync();
        if (!file)
        {
            StatusText().Text(L"Open canceled");
            co_return;
        }

        DocInfoText().Text(file.Name());
        StatusText().Text(L"Loading PDF...");

        m_currentPageIndex = 0;
        m_pdf.LoadFromPath(file.Path().c_str());

        StatusText().Text(L"Rendering page 1...");
        Windows::Graphics::Imaging::SoftwareBitmap pageBitmap = m_pdf.RenderPageToSoftwareBitmap(m_currentPageIndex, 2.0f /*scale*/);

        SoftwareBitmapSource source;
        co_await source.SetBitmapAsync(pageBitmap);
        PdfPageImage().Source(source);

        StatusText().Text(L"Loaded");
    }

    winrt::fire_and_forget MainWindow::PlaceSignatureButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (!m_pdf.IsLoaded())
        {
            StatusText().Text(L"Load a PDF first");
            co_return;
        }

        StatusText().Text(L"Capturing signature...");

        // Capture the InkCanvas as a bitmap (BGRA8).
        Windows::Graphics::Imaging::SoftwareBitmap signatureBitmap =
            co_await SignatureCapture::CaptureInkCanvasAsync(SignatureInkCanvas());

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
        Windows::Graphics::Imaging::SoftwareBitmap pageBitmap = m_pdf.RenderPageToSoftwareBitmap(m_currentPageIndex, 2.0f /*scale*/);
        SoftwareBitmapSource source;
        co_await source.SetBitmapAsync(pageBitmap);
        PdfPageImage().Source(source);

        StatusText().Text(L"Signature placed");
    }

    winrt::fire_and_forget MainWindow::SaveSignedPdfButton_Click(IInspectable const&, RoutedEventArgs const&)
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

    void MainWindow::ClearSignatureButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        SignatureInkCanvas().InkPresenter().StrokeContainer().Clear();
        StatusText().Text(L"Signature cleared");
    }
}

