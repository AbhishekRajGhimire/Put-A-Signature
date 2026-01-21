#pragma once

#include "MainWindow.g.h"

#include "PdfDocumentHandler.h"

namespace winrt::Put_A_Signature::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        winrt::fire_and_forget OpenPdfButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget PlaceSignatureButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget SaveSignedPdfButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ClearSignatureButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void PrevPageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void NextPageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget PageNumberBox_KeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

        void PdfDropZone_DragOver(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::DragEventArgs const& e);
        winrt::fire_and_forget PdfDropZone_Drop(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::DragEventArgs const& e);

        void SignatureCanvas_PointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void SignatureCanvas_PointerMoved(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void SignatureCanvas_PointerReleased(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void SignatureCanvas_PointerCanceled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

    private:
        winrt::Windows::Foundation::IAsyncAction LoadPdfFromFileAsync(winrt::Windows::Storage::StorageFile const& file);
        winrt::Windows::Foundation::IAsyncAction RenderCurrentPageAsync();
        void UpdateNavigationUi();
        void SetEmptyStateVisible(bool visible);

        PdfDocumentHandler m_pdf{};
        int32_t m_currentPageIndex{ 0 };
        int32_t m_pageCount{ 0 };

        bool m_isDrawing{ false };
        uint32_t m_activePointerId{ 0 };
        winrt::Microsoft::UI::Xaml::Shapes::Polyline m_activeStroke{ nullptr };
    };
}

namespace winrt::Put_A_Signature::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
