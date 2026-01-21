#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <winrt/Windows.Graphics.Imaging.h>

struct PdfRect
{
    double x{};
    double y{};
    double width{};
    double height{};
};

// Minimal PDFium wrapper focused on:
// - Load document
// - Render page -> SoftwareBitmap (BGRA8)
// - Stamp a signature bitmap onto a page
// - Save as a new file
//
// Assumptions:
// - PDFium headers are included and linked.
// - Threading model: called from UI thread for this starter. (You can move to a worker thread later.)
class PdfDocumentHandler
{
public:
    PdfDocumentHandler();
    ~PdfDocumentHandler();

    PdfDocumentHandler(PdfDocumentHandler const&) = delete;
    PdfDocumentHandler& operator=(PdfDocumentHandler const&) = delete;

    bool IsLoaded() const noexcept;

    void LoadFromPath(std::wstring const& path);

    // Render a page to a BGRA8 SoftwareBitmap (premultiplied alpha).
    // scale: 1.0 = native pixel size based on page points -> pixels at 96 DPI (placeholder).
    winrt::Windows::Graphics::Imaging::SoftwareBitmap RenderPageToSoftwareBitmap(int32_t pageIndex, float scale);

    // Stamp a signature bitmap (BGRA8) onto a page at rectInPdfPoints (PDF points).
    // Coordinate conversion from UI pixels is intentionally a placeholder and should be handled by the UI layer.
    void StampSignatureBitmap(int32_t pageIndex,
        winrt::Windows::Graphics::Imaging::SoftwareBitmap const& signatureBitmap,
        PdfRect const& rectInPdfPoints);

    void SaveAs(std::wstring const& outputPath);

    // Placeholder for multi-page support
    // int32_t PageCount() const;

private:
    void EnsurePdfiumInitialized();
    void Close();

    struct Impl;
    Impl* m{};
};

