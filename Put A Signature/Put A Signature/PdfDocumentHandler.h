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
// If PDFium headers are not available, this compiles but throws at runtime
// with a clear "PDFium not integrated" message.
class PdfDocumentHandler
{
public:
    PdfDocumentHandler();
    ~PdfDocumentHandler();

    PdfDocumentHandler(PdfDocumentHandler const&) = delete;
    PdfDocumentHandler& operator=(PdfDocumentHandler const&) = delete;

    bool IsLoaded() const noexcept;

    void LoadFromPath(std::wstring const& path);

    // Preferred for packaged apps: load from in-memory PDF bytes (keeps bytes alive for PDFium).
    void LoadFromBytes(std::vector<uint8_t> bytes);

    // Render a page to a BGRA8 SoftwareBitmap (premultiplied alpha).
    // scale: 1.0 = native pixel size based on page points -> pixels at 96 DPI (placeholder).
    winrt::Windows::Graphics::Imaging::SoftwareBitmap RenderPageToSoftwareBitmap(int32_t pageIndex, float scale);

    // Stamp a signature bitmap (BGRA8) onto a page at rectInPdfPoints (PDF points).
    // Coordinate conversion from UI pixels is intentionally a placeholder and should be handled by the UI layer.
    void StampSignatureBitmap(int32_t pageIndex,
        winrt::Windows::Graphics::Imaging::SoftwareBitmap const& signatureBitmap,
        PdfRect const& rectInPdfPoints);

    void SaveAs(std::wstring const& outputPath);

    // Returns number of pages in the loaded document, or 0 if not loaded / PDFium not integrated.
    int32_t PageCount() const noexcept;

private:
    void Close();

    struct Impl;
    Impl* m{};
};

