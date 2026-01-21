## Put a Signature — WinUI 3 (C++/WinRT) + PDFium Starter

#work in progress preview of interface

<img width="2559" height="1532" alt="Screenshot 2026-01-22 044851" src="https://github.com/user-attachments/assets/201db51c-4269-42ad-8f70-b239b88c4979" />

This repo is a **minimal, functional starting point** for a native Windows 11 PDF signing app using:

- **WinUI 3** (Windows App SDK) for modern UI
- **C++/WinRT** for app + backend logic
- **PDFium** for fast PDF loading/rendering/editing

> Build integration for PDFium is intentionally not included. The code assumes the PDFium headers/libs are available and linked.

## Bootstrap (make it buildable)

If you created the Visual Studio template **inside this repo**, your real app code is in:

- `Put A Signature/Put A Signature/`

See `docs/BOOTSTRAP.md` for a quick explanation of the generated folders and what to edit.

## PDFium (make PDF rendering work)

Follow `docs/PDFIUM_SETUP.md` to drop your downloaded PDFium (x64, without V8) into `external/pdfium/` and rebuild.

---

## Architecture (Separation of Concerns)

### UI layer (WinUI 3 XAML)
**Responsibility**: layout, input events, visual state.

- `src/MainWindow.xaml`: modern Windows 11 layout (Open / Place / Save + PDF view + Ink signature panel)
- `src/MainWindow.xaml.h/.cpp`: event handlers + light orchestration

### App orchestration (code-behind minimal, MVVM-friendly)
**Responsibility**: calls into backend services; keeps UI responsive via async.

- Holds a `PdfDocumentHandler` instance.
- On open: pick PDF → load → render page → show in `Image`.
- On place: capture signature (bitmap) → stamp into PDF at chosen coordinates.
- On save: call handler save-as.

### Backend PDF service (C++ wrapper around PDFium)
**Responsibility**: PDFium lifecycle, document/page operations, rendering, stamping, saving.

- `src/PdfDocumentHandler.h/.cpp`
- Keeps **no UI types** except `SoftwareBitmap` for render input/output.

### Component interaction (how MainWindow talks to the backend)

- `MainWindow` owns a `PdfDocumentHandler` instance.
- **Open PDF**
  - WinUI uses `FileOpenPicker` → gets a `StorageFile` → calls `m_pdf.LoadFromPath(file.Path())`
  - UI requests `RenderPageToSoftwareBitmap(0, scale)` and sets an `Image` source via `SoftwareBitmapSource`.
- **Place Signature**
  - UI captures `InkCanvas` to a `SoftwareBitmap` via `SignatureCapture::CaptureInkCanvasAsync`
  - UI calls `m_pdf.StampSignatureBitmap(pageIndex, signatureBitmap, rectInPdfPoints)`
  - UI re-renders the page for instant feedback
- **Save Signed PDF**
  - WinUI uses `FileSavePicker` → gets output path → calls `m_pdf.SaveAs(outputPath)`

If you move `PdfDocumentHandler` into a separate WinRT component later, this interaction stays the same—only the class location and ABI surface change.

### Optional WinRT component boundary (recommended for larger apps)
For a larger app, put the backend in a separate **C++/WinRT Windows Runtime Component** project (e.g., `PdfCore`) and expose:

- `LoadAsync(hstring path)`
- `RenderPageAsync(int32 pageIndex) -> SoftwareBitmap`
- `StampSignatureAsync(int32 pageIndex, SoftwareBitmap signature, PdfRect rect)`
- `SaveAsAsync(hstring outputPath)`

The WinUI app project references the component and calls it from UI handlers.  
This repo keeps everything in one place to be a minimal starting point, but the code is structured so you can move it into a component later.

---

## UI design (Windows 11 / Fluent)

The main window is:

- **Top command bar**: Open PDF, Place Signature, Save Signed PDF
- **Center**: large PDF view (renders a page as an image; multi-page hooks included)
- **Right panel**: Signature InkCanvas (draw with mouse/pen), plus Clear

Mica: enabled from code-behind using `MicaBackdrop` (WinUI 3 system backdrop).

---

## Source layout

```
src/
  MainWindow.xaml
  MainWindow.xaml.h
  MainWindow.xaml.cpp
  PdfDocumentHandler.h
  PdfDocumentHandler.cpp
  SignatureCapture.h
  SignatureCapture.cpp
```

---

## Notes / next steps you’ll likely add

- **Page navigation** (thumbnails, scrollable pages, or virtualization)
- **Coordinate conversion** from UI pixels → PDF points (origin differences)
- **Drag-to-place** signature preview overlay in the PDF view
- **Optional cryptographic signatures** (PAdES) instead of visual stamping

---

## Important integration notes (PDFium)

- **Paths**: PDFium expects the file path as UTF-8 (`FPDF_LoadDocument`), so the starter converts `std::wstring` → UTF-8.
- **Rendering**: PDFium renders to a BGRA buffer (`FPDFBitmap_BGRA`). The starter copies that into a `SoftwareBitmap` for WinUI display.
- **Stamping**: The starter creates an image page object (`FPDFPageObj_NewImageObj`) and sets a bitmap via `FPDFImageObj_SetBitmap`, then positions it via `FPDFImageObj_SetMatrix`.
- **Multi-page**: add a page count method and a way to select `pageIndex` in the UI; current UI renders page 0.
