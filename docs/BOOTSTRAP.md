## Bootstrapping this repo into a buildable WinUI 3 (C++/WinRT) app

This repository contains the **core starter source** under `src/`, but it does **not** include a Visual Studio project/solution yet.

### Create the WinUI 3 C++/WinRT project (Visual Studio)
- Install:
  - Visual Studio 2022 with **Desktop development with C++**
  - **Windows App SDK** tooling
  - Windows 11 SDK (recommended)
- Create a new project:
  - Template: **Blank App, Packaged (WinUI 3 in Desktop)** (C++/WinRT)
  - Project name recommendation: `PutASignature`
  - Solution name recommendation: `PutASignature`

### Copy this repo’s starter sources into the project
- Copy all files from this repo’s `src/` into your Visual Studio project folder (where `MainWindow.xaml` lives).
- In Solution Explorer:
  - Add the copied `.xaml`, `.h`, and `.cpp` files to the project (Add → Existing Item).

### Namespace alignment (important)
This starter uses:
- XAML class: `PutASignature.MainWindow`
- C++ namespace: `winrt::PutASignature::implementation`

If your project’s default namespace differs:
- Either rename your project to `PutASignature`, **or**
- Update:
  - `src/MainWindow.xaml` `x:Class="YourNamespace.MainWindow"`
  - `src/MainWindow.xaml.h/.cpp` namespaces accordingly

### PDFium integration (intentionally high-level)
This repo assumes PDFium is already available to your build:
- Add include paths so these headers resolve:
  - `fpdfview.h`, `fpdf_edit.h`, `fpdf_save.h`
- Link PDFium libraries and deploy DLLs as needed for your build setup.

### First run sanity checklist
- Build + run app.
- Click **Open PDF** → pick a PDF.
- Draw in the signature panel.
- Click **Place Signature** (starter stamps at a fixed placeholder rect).
- Click **Save Signed PDF**.

### Next upgrade you’ll likely implement
- Click-to-place: map pointer position over the rendered page to PDF points.
- Multi-page: add page navigation + render selected page index.

