#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>

// Utilities for capturing the signature drawn on an InkCanvas into a SoftwareBitmap.
// This starter uses RenderTargetBitmap to "snapshot" the InkCanvas visuals.
//
// Notes:
// - RenderTargetBitmap captures at UI pixel resolution.
// - The bitmap will include whatever background the InkCanvas renders with.
//   Keep InkCanvas Background="Transparent" to preserve alpha when possible.
struct SignatureCapture
{
    static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Graphics::Imaging::SoftwareBitmap>
        CaptureInkCanvasAsync(winrt::Microsoft::UI::Xaml::Controls::InkCanvas const& inkCanvas);
};

