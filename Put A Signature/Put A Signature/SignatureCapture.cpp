#include "pch.h"
#include "SignatureCapture.h"

#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Security.Cryptography.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Security::Cryptography;

IAsyncOperation<SoftwareBitmap> SignatureCapture::CaptureElementAsync(Microsoft::UI::Xaml::UIElement const& element)
{
    RenderTargetBitmap rtb;
    co_await rtb.RenderAsync(element);

    uint32_t width = rtb.PixelWidth();
    uint32_t height = rtb.PixelHeight();
    if (width == 0 || height == 0)
    {
        co_return SoftwareBitmap(BitmapPixelFormat::Bgra8, 1, 1, BitmapAlphaMode::Premultiplied);
    }

    auto buffer = co_await rtb.GetPixelsAsync();

    com_array<uint8_t> bytes;
    CryptographicBuffer::CopyToByteArray(buffer, bytes);

    auto ibuf = CryptographicBuffer::CreateFromByteArray(bytes);
    SoftwareBitmap sb = SoftwareBitmap::CreateCopyFromBuffer(
        ibuf,
        BitmapPixelFormat::Bgra8,
        static_cast<int32_t>(width),
        static_cast<int32_t>(height),
        BitmapAlphaMode::Premultiplied);

    co_return sb;
}

