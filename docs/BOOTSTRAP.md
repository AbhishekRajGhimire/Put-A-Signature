## Bootstrapping this repo into a buildable WinUI 3 (C++/WinRT) app

This repository contains a Visual Studio WinUI 3 solution **and** some earlier “starter-only” sources.

### What’s the real app source?

- **Real buildable WinUI app project**: `Put A Signature/Put A Signature/`
- **Packaging project**: `Put A Signature/Put A Signature (Package)/`
- **Build outputs / cache**: `.vs/`, `x64/`, `Put A Signature/Put A Signature/x64/`, `packages/` (normal for VS)

If you still have a separate top-level `src/` folder, treat it as **legacy/reference**—the build uses the `.vcxproj` project folder above.

### If you have NOT created the WinUI 3 C++/WinRT project yet
- Install:
  - Visual Studio 2022 with **Desktop development with C++**
  - **Windows App SDK** tooling
  - Windows 11 SDK (recommended)
- Create a new project:
  - Template: **Blank App, Packaged (WinUI 3 in Desktop)** (C++/WinRT)
  - Project name recommendation: `PutASignature`
  - Solution name recommendation: `PutASignature`

### If you already created the project in this repo (most likely)
You don’t need to create anything else—just work inside:

- `Put A Signature/Put A Signature/`

That project should contain:
- `MainWindow.xaml/.h/.cpp` (UI + event handlers)
- `PdfDocumentHandler.*` and `SignatureCapture.*` (backend + ink capture)

### Namespace alignment (important)
Your Visual Studio template uses:
- XAML class: `Put_A_Signature.MainWindow`
- C++ namespace: `winrt::Put_A_Signature::implementation`

Keep these consistent across XAML + C++.

### PDFium integration (intentionally high-level)
This repo assumes PDFium is already available to your build:
- Add include paths so these headers resolve:
  - `fpdfview.h`, `fpdf_edit.h`, `fpdf_save.h`
- Link PDFium libraries and deploy DLLs as needed for your build setup.

### First run sanity checklist
- Build + run app.
- Click **Open PDF** → pick a PDF.
- Draw in the signature panel (this starter uses a **Canvas + pointer events** because **InkCanvas is not supported in WinUI 3**).
- Click **Place Signature** (starter stamps at a fixed placeholder rect).
- Click **Save Signed PDF**.

### Next upgrade you’ll likely implement
- Click-to-place: map pointer position over the rendered page to PDF points.
- Multi-page: add page navigation + render selected page index.

