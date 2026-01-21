#include "pch.h"
#include "PdfDocumentHandler.h"

#include <mutex>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <limits>

#include <windows.h>
#include <winrt/Windows.Security.Cryptography.h>

using namespace winrt;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Security::Cryptography;

#if __has_include("fpdfview.h") && __has_include("fpdf_edit.h") && __has_include("fpdf_save.h")
  #include "fpdfview.h"
  #include "fpdf_edit.h"
  #include "fpdf_save.h"
  #define PUT_A_SIGNATURE_HAS_PDFIUM 1
#else
  #define PUT_A_SIGNATURE_HAS_PDFIUM 0
#endif

namespace
{
    void ThrowIf(bool condition, char const* message)
    {
        if (condition) throw std::runtime_error(message);
    }

    std::string WideToUtf8(std::wstring const& w)
    {
        if (w.empty()) return {};
        int required = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        ThrowIf(required <= 0, "WideCharToMultiByte failed");
        std::string out(static_cast<size_t>(required), '\0');
        int written = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), required, nullptr, nullptr);
        ThrowIf(written != required, "WideCharToMultiByte wrote unexpected length");
        return out;
    }

    // Minimal COM interface to access BitmapBuffer bytes.
    struct __declspec(uuid("5B0D3235-4DBA-4D44-8659-BC8A2776DCC1")) IMemoryBufferByteAccess : IUnknown
    {
        virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
    };

    std::vector<uint8_t> CopySoftwareBitmapToBgra(SoftwareBitmap const& bmp, uint32_t& outWidth, uint32_t& outHeight, int32_t& outStride)
    {
        ThrowIf(bmp.BitmapPixelFormat() != BitmapPixelFormat::Bgra8, "Expected BGRA8 SoftwareBitmap");

        outWidth = static_cast<uint32_t>(bmp.PixelWidth());
        outHeight = static_cast<uint32_t>(bmp.PixelHeight());

        BitmapBuffer buffer = bmp.LockBuffer(BitmapBufferAccessMode::Read);
        auto desc = buffer.GetPlaneDescription(0);
        outStride = desc.Stride;

        auto reference = buffer.CreateReference();
        uint8_t* data = nullptr;
        uint32_t capacity = 0;
        check_hresult(reference.as<IMemoryBufferByteAccess>()->GetBuffer(&data, &capacity));

        std::vector<uint8_t> out(static_cast<size_t>(desc.Stride) * desc.Height);
        std::memcpy(out.data(), data + desc.StartIndex, out.size());
        return out;
    }
}

struct PdfDocumentHandler::Impl
{
    std::wstring path{};
#if PUT_A_SIGNATURE_HAS_PDFIUM
    // Keep backing bytes alive when loading via FPDF_LoadMemDocument.
    std::vector<uint8_t> docBytes{};
#endif

#if PUT_A_SIGNATURE_HAS_PDFIUM
    FPDF_DOCUMENT doc{ nullptr };
#endif

    ~Impl()
    {
#if PUT_A_SIGNATURE_HAS_PDFIUM
        if (doc)
        {
            FPDF_CloseDocument(doc);
            doc = nullptr;
        }
#endif
    }
};

namespace
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    std::once_flag g_pdfiumInitOnce;
#endif
}

PdfDocumentHandler::PdfDocumentHandler()
    : m(new Impl{})
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    std::call_once(g_pdfiumInitOnce, []()
    {
        FPDF_InitLibrary();
    });
#endif
}

PdfDocumentHandler::~PdfDocumentHandler()
{
    Close();
    delete m;
    m = nullptr;
}

bool PdfDocumentHandler::IsLoaded() const noexcept
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    return m && m->doc != nullptr;
#else
    return false;
#endif
}

void PdfDocumentHandler::Close()
{
    if (!m) return;

#if PUT_A_SIGNATURE_HAS_PDFIUM
    if (m->doc)
    {
        FPDF_CloseDocument(m->doc);
        m->doc = nullptr;
    }
#endif

    m->path.clear();
#if PUT_A_SIGNATURE_HAS_PDFIUM
    m->docBytes.clear();
    m->docBytes.shrink_to_fit();
#endif
}

void PdfDocumentHandler::LoadFromPath(std::wstring const& path)
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    ThrowIf(!m, "PdfDocumentHandler not initialized");

    Close();
    m->path = path;

    std::string utf8Path = WideToUtf8(path);
    m->doc = FPDF_LoadDocument(utf8Path.c_str(), nullptr);
    if (!m->doc)
    {
        throw std::runtime_error("FPDF_LoadDocument failed (bad path, password needed, or PDFium load error)");
    }
#else
    (void)path;
    throw std::runtime_error("PDFium not integrated: add PDFium headers/libs so fpdfview.h/fpdf_edit.h/fpdf_save.h are available.");
#endif
}

void PdfDocumentHandler::LoadFromBytes(std::vector<uint8_t> bytes)
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    ThrowIf(!m, "PdfDocumentHandler not initialized");

    Close();

    // Keep the bytes alive for as long as the document is open.
    m->docBytes = std::move(bytes);
    if (m->docBytes.empty())
    {
        throw std::runtime_error("PDF buffer is empty");
    }

#if defined(FPDF_LoadMemDocument64)
    m->doc = FPDF_LoadMemDocument64(m->docBytes.data(), static_cast<size_t>(m->docBytes.size()), nullptr);
#else
    if (m->docBytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("PDF too large for FPDF_LoadMemDocument");
    }
    m->doc = FPDF_LoadMemDocument(m->docBytes.data(), static_cast<int>(m->docBytes.size()), nullptr);
#endif

    if (!m->doc)
    {
        throw std::runtime_error("FPDF_LoadMemDocument failed (corrupt PDF, password needed, or PDFium load error)");
    }
#else
    (void)bytes;
    throw std::runtime_error("PDFium not integrated: add PDFium headers/libs so fpdfview.h/fpdf_edit.h/fpdf_save.h are available.");
#endif
}

SoftwareBitmap PdfDocumentHandler::RenderPageToSoftwareBitmap(int32_t pageIndex, float scale)
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    ThrowIf(!IsLoaded(), "No document loaded");

    FPDF_PAGE page = FPDF_LoadPage(m->doc, pageIndex);
    ThrowIf(!page, "Failed to load page");

    const double pageWidthPts = FPDF_GetPageWidth(page);
    const double pageHeightPts = FPDF_GetPageHeight(page);

    const int widthPx = static_cast<int>(pageWidthPts * (96.0 / 72.0) * scale);
    const int heightPx = static_cast<int>(pageHeightPts * (96.0 / 72.0) * scale);
    ThrowIf(widthPx <= 0 || heightPx <= 0, "Invalid page size");

    FPDF_BITMAP bitmap = FPDFBitmap_Create(widthPx, heightPx, 1);
    ThrowIf(!bitmap, "Failed to create bitmap");

    FPDFBitmap_FillRect(bitmap, 0, 0, widthPx, heightPx, 0xFFFFFFFF);

    int flags = FPDF_ANNOT;
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, widthPx, heightPx, 0, flags);

    uint8_t* buffer = static_cast<uint8_t*>(FPDFBitmap_GetBuffer(bitmap));
    const int stride = FPDFBitmap_GetStride(bitmap);

    // Avoid relying on IMemoryBufferByteAccess (can fail depending on toolchain/runtime).
    // Build a tightly-packed BGRA buffer (stride = width * 4) and create SoftwareBitmap from it.
    const int dstStride = widthPx * 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(dstStride) * static_cast<size_t>(heightPx));

    const size_t rowCopy = static_cast<size_t>((std::min)(dstStride, stride));
    for (int y = 0; y < heightPx; ++y)
    {
        std::memcpy(pixels.data() + static_cast<size_t>(y) * dstStride, buffer + static_cast<size_t>(y) * stride, rowCopy);
    }

    winrt::com_array<uint8_t> bytes(static_cast<uint32_t>(pixels.size()));
    std::memcpy(bytes.data(), pixels.data(), pixels.size());
    auto ibuf = CryptographicBuffer::CreateFromByteArray(bytes);

    SoftwareBitmap sb = SoftwareBitmap::CreateCopyFromBuffer(
        ibuf,
        BitmapPixelFormat::Bgra8,
        widthPx,
        heightPx,
        BitmapAlphaMode::Premultiplied);

    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);

    return sb;
#else
    (void)pageIndex;
    (void)scale;
    throw std::runtime_error("PDFium not integrated: cannot render.");
#endif
}

void PdfDocumentHandler::StampSignatureBitmap(int32_t pageIndex, SoftwareBitmap const& signatureBitmap, PdfRect const& rectInPdfPoints)
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    ThrowIf(!IsLoaded(), "No document loaded");

    FPDF_PAGE page = FPDF_LoadPage(m->doc, pageIndex);
    ThrowIf(!page, "Failed to load page");

    uint32_t sigW = 0, sigH = 0;
    int32_t sigStride = 0;
    std::vector<uint8_t> sigBgra = CopySoftwareBitmapToBgra(signatureBitmap, sigW, sigH, sigStride);
    ThrowIf(sigW == 0 || sigH == 0, "Invalid signature bitmap");

    FPDF_BITMAP sigBmp = FPDFBitmap_CreateEx(
        static_cast<int>(sigW),
        static_cast<int>(sigH),
        FPDFBitmap_BGRA,
        sigBgra.data(),
        sigStride);
    ThrowIf(!sigBmp, "Failed to create signature bitmap");

    FPDF_PAGEOBJECT imageObj = FPDFPageObj_NewImageObj(m->doc);
    ThrowIf(!imageObj, "Failed to create image object");

    if (!FPDFImageObj_SetBitmap(&page, 0, imageObj, sigBmp))
    {
        FPDFBitmap_Destroy(sigBmp);
        FPDFPageObj_Destroy(imageObj);
        FPDF_ClosePage(page);
        throw std::runtime_error("FPDFImageObj_SetBitmap failed");
    }

    if (!FPDFImageObj_SetMatrix(
        imageObj,
        static_cast<float>(rectInPdfPoints.width),
        0.0f,
        0.0f,
        static_cast<float>(rectInPdfPoints.height),
        static_cast<float>(rectInPdfPoints.x),
        static_cast<float>(rectInPdfPoints.y)))
    {
        FPDFBitmap_Destroy(sigBmp);
        FPDFPageObj_Destroy(imageObj);
        FPDF_ClosePage(page);
        throw std::runtime_error("FPDFImageObj_SetMatrix failed");
    }

    FPDFPage_InsertObject(page, imageObj);
    FPDFPage_GenerateContent(page);

    FPDFBitmap_Destroy(sigBmp);
    FPDF_ClosePage(page);
#else
    (void)pageIndex;
    (void)signatureBitmap;
    (void)rectInPdfPoints;
    throw std::runtime_error("PDFium not integrated: cannot stamp.");
#endif
}

void PdfDocumentHandler::SaveAs(std::wstring const& outputPath)
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    ThrowIf(!IsLoaded(), "No document loaded");

    // Simple synchronous file writer for FPDF_SaveAsCopy.
    struct FileWriter
    {
        FPDF_FILEWRITE iface{};
        HANDLE hFile{ INVALID_HANDLE_VALUE };

        static int WriteBlock(FPDF_FILEWRITE* pThis, const void* data, unsigned long size)
        {
            auto self = reinterpret_cast<FileWriter*>(pThis);
            DWORD written = 0;
            BOOL ok = ::WriteFile(self->hFile, data, size, &written, nullptr);
            return (ok && written == size) ? 1 : 0;
        }
    };

    FileWriter writer{};
    writer.iface.version = 1;
    writer.iface.WriteBlock = &FileWriter::WriteBlock;

    writer.hFile = ::CreateFileW(
        outputPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    ThrowIf(writer.hFile == INVALID_HANDLE_VALUE, "Failed to open output file for writing");

    if (!FPDF_SaveAsCopy(m->doc, &writer.iface, 0))
    {
        ::CloseHandle(writer.hFile);
        throw std::runtime_error("FPDF_SaveAsCopy failed");
    }

    ::CloseHandle(writer.hFile);
#else
    (void)outputPath;
    throw std::runtime_error("PDFium not integrated: cannot save.");
#endif
}

int32_t PdfDocumentHandler::PageCount() const noexcept
{
#if PUT_A_SIGNATURE_HAS_PDFIUM
    if (!m || !m->doc) return 0;
    return static_cast<int32_t>(FPDF_GetPageCount(m->doc));
#else
    return 0;
#endif
}
