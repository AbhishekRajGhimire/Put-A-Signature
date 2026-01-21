#pragma once

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>

// Utilities for capturing the signature UI element into a SoftwareBitmap.
// This starter uses RenderTargetBitmap to "snapshot" the visuals.
struct SignatureCapture
{
    static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Graphics::Imaging::SoftwareBitmap>
        CaptureElementAsync(winrt::Microsoft::UI::Xaml::UIElement const& element);
};

