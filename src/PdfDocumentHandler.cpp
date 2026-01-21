#include "pch.h"
#include "PdfDocumentHandler.h"

// PDFium
// Assumes you have something like:
//   #include "fpdfview.h"
//   #include "fpdf_edit.h"
//   #include "fpdf_save.h"
//
// Included here as placeholders; you will include the actual PDFium headers in your project.
#include "fpdfview.h"
#include "fpdf_edit.h"
#include "fpdf_save.h"

#include <mutex>
#include <stdexcept>
#include <cstring>

#include <windows.h>

using namespace winrt;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace
{
    std::once_flag g_pdfiumInitOnce;

    // Minimal COM interface to access BitmapBuffer bytes.
    struct __declspec(uuid("5B0D3235-4DBA-4D44-8659-BC8A2776DCC1")) IMemoryBufferByteAccess : IUnknown
    {
        virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
    };

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

    // Convert SoftwareBitmap (BGRA8) to a contiguous BGRA byte vector.
    // Note: SoftwareBitmap can have stride/padding; use BitmapBuffer for safe access.
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
    FPDF_DOCUMENT doc{ nullptr };

    ~Impl()
    {
        if (doc)
        {
            FPDF_CloseDocument(doc);
            doc = nullptr;
        }
    }
};

PdfDocumentHandler::PdfDocumentHandler()
    : m(new Impl{})
{
    EnsurePdfiumInitialized();
}

PdfDocumentHandler::~PdfDocumentHandler()
{
    Close();
    delete m;
    m = nullptr;
}

void PdfDocumentHandler::EnsurePdfiumInitialized()
{
    std::call_once(g_pdfiumInitOnce, []()
    {
        FPDF_InitLibrary();
        // If you need PDFium edit module init, do it here.
    });
}

bool PdfDocumentHandler::IsLoaded() const noexcept
{
    return m && m->doc != nullptr;
}

void PdfDocumentHandler::Close()
{
    if (!m) return;
    if (m->doc)
    {
        FPDF_CloseDocument(m->doc);
        m->doc = nullptr;
    }
    m->path.clear();
}

void PdfDocumentHandler::LoadFromPath(std::wstring const& path)
{
    ThrowIf(!m, "PdfDocumentHandler not initialized");

    Close();
    m->path = path;

    // Password support: pass a password string instead of nullptr.
    std::string utf8Path = WideToUtf8(path);
    m->doc = FPDF_LoadDocument(utf8Path.c_str(), nullptr);
    if (!m->doc)
    {
        // Optional: inspect FPDF_GetLastError() for more detail.
        throw std::runtime_error("FPDF_LoadDocument failed (bad path, password needed, or PDFium load error)");
    }
}

SoftwareBitmap PdfDocumentHandler::RenderPageToSoftwareBitmap(int32_t pageIndex, float scale)
{
    ThrowIf(!IsLoaded(), "No document loaded");

    FPDF_PAGE page = FPDF_LoadPage(m->doc, pageIndex);
    ThrowIf(!page, "Failed to load page");

    const double pageWidthPts = FPDF_GetPageWidth(page);
    const double pageHeightPts = FPDF_GetPageHeight(page);

    // Minimal mapping: treat PDF points as 1/72 inch, render at 96 DPI baseline then apply scale.
    // pixels = points * (96 / 72) * scale
    const int widthPx = static_cast<int>(pageWidthPts * (96.0 / 72.0) * scale);
    const int heightPx = static_cast<int>(pageHeightPts * (96.0 / 72.0) * scale);
    ThrowIf(widthPx <= 0 || heightPx <= 0, "Invalid page size");

    // PDFium uses BGRA. Use alpha=1 (opaque background) for now.
    FPDF_BITMAP bitmap = FPDFBitmap_Create(widthPx, heightPx, 1);
    ThrowIf(!bitmap, "Failed to create bitmap");

    // White background
    FPDFBitmap_FillRect(bitmap, 0, 0, widthPx, heightPx, 0xFFFFFFFF);

    // Render flags: you can tweak for quality/perf.
    int flags = FPDF_ANNOT; // include annotations
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, widthPx, heightPx, 0 /*rotate*/, flags);

    uint8_t* buffer = static_cast<uint8_t*>(FPDFBitmap_GetBuffer(bitmap));
    const int stride = FPDFBitmap_GetStride(bitmap);

    // Copy into a SoftwareBitmap.
    // Note: We copy because PDFium owns the bitmap buffer.
    SoftwareBitmap sb(BitmapPixelFormat::Bgra8, widthPx, heightPx, BitmapAlphaMode::Premultiplied);
    BitmapBuffer sbuf = sb.LockBuffer(BitmapBufferAccessMode::Write);
    auto desc = sbuf.GetPlaneDescription(0);

    auto reference = sbuf.CreateReference();
    uint8_t* dest = nullptr;
    uint32_t capacity = 0;
    check_hresult(reference.as<IMemoryBufferByteAccess>()->GetBuffer(&dest, &capacity));

    for (int y = 0; y < heightPx; ++y)
    {
        std::memcpy(
            dest + desc.StartIndex + y * desc.Stride,
            buffer + y * stride,
            static_cast<size_t>(std::min(desc.Stride, stride)));
    }

    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);

    return sb;
}

void PdfDocumentHandler::StampSignatureBitmap(int32_t pageIndex, SoftwareBitmap const& signatureBitmap, PdfRect const& rectInPdfPoints)
{
    ThrowIf(!IsLoaded(), "No document loaded");

    // Load page for editing.
    FPDF_PAGE page = FPDF_LoadPage(m->doc, pageIndex);
    ThrowIf(!page, "Failed to load page");

    uint32_t sigW = 0, sigH = 0;
    int32_t sigStride = 0;
    std::vector<uint8_t> sigBgra = CopySoftwareBitmapToBgra(signatureBitmap, sigW, sigH, sigStride);
    ThrowIf(sigW == 0 || sigH == 0, "Invalid signature bitmap");

    // Create PDFium bitmap from signature pixels.
    FPDF_BITMAP sigBmp = FPDFBitmap_CreateEx(
        static_cast<int>(sigW),
        static_cast<int>(sigH),
        FPDFBitmap_BGRA,
        sigBgra.data(),
        sigStride);
    ThrowIf(!sigBmp, "Failed to create signature bitmap");

    // Create an image object and attach bitmap.
    FPDF_PAGEOBJECT imageObj = FPDFPageObj_NewImageObj(m->doc);
    ThrowIf(!imageObj, "Failed to create image object");

    // Attach bitmap to image object.
    // NOTE: PDFium will read from the bitmap; you may need to ensure lifetime until content generation.
    if (!FPDFImageObj_SetBitmap(&page, 0, imageObj, sigBmp))
    {
        FPDFBitmap_Destroy(sigBmp);
        FPDFPageObj_Destroy(imageObj);
        FPDF_ClosePage(page);
        throw std::runtime_error("FPDFImageObj_SetBitmap failed");
    }

    // Place & scale in PDF user space (points).
    // Coordinate conversion placeholder:
    // - PDF origin is bottom-left
    // - UI origin is top-left
    // You will likely map UI pixel coordinates to PDF points with page size + zoom + scroll offsets.
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

    // Cleanup.
    // imageObj is now owned by page content.
    FPDFBitmap_Destroy(sigBmp);
    FPDF_ClosePage(page);
}

void PdfDocumentHandler::SaveAs(std::wstring const& outputPath)
{
    ThrowIf(!IsLoaded(), "No document loaded");

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

    // Save flags can include incremental saving, etc.
    const int flags = 0;
    if (!FPDF_SaveAsCopy(m->doc, &writer.iface, flags))
    {
        ::CloseHandle(writer.hFile);
        throw std::runtime_error("FPDF_SaveAsCopy failed");
    }

    ::CloseHandle(writer.hFile);
}

