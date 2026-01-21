#include "pch.h"
#include "SignatureCapture.h"

#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Security.Cryptography.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Security::Cryptography;

IAsyncOperation<SoftwareBitmap> SignatureCapture::CaptureInkCanvasAsync(InkCanvas const& inkCanvas)
{
    // RenderTargetBitmap can render any UIElement (InkCanvas is a Control).
    RenderTargetBitmap rtb;
    co_await rtb.RenderAsync(inkCanvas);

    uint32_t width = rtb.PixelWidth();
    uint32_t height = rtb.PixelHeight();
    if (width == 0 || height == 0)
    {
        // Return a 1x1 transparent bitmap if nothing is rendered.
        co_return SoftwareBitmap(BitmapPixelFormat::Bgra8, 1, 1, BitmapAlphaMode::Premultiplied);
    }

    // Pixels are returned as BGRA8 premultiplied in an IBuffer.
    auto buffer = co_await rtb.GetPixelsAsync();

    // Copy to byte array for SoftwareBitmap creation.
    com_array<uint8_t> bytes;
    CryptographicBuffer::CopyToByteArray(buffer, bytes);

    // Create SoftwareBitmap from buffer (BGRA8).
    auto ibuf = CryptographicBuffer::CreateFromByteArray(bytes);
    SoftwareBitmap sb = SoftwareBitmap::CreateCopyFromBuffer(
        ibuf,
        BitmapPixelFormat::Bgra8,
        static_cast<int32_t>(width),
        static_cast<int32_t>(height),
        BitmapAlphaMode::Premultiplied);

    co_return sb;
}

