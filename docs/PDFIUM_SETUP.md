## PDFium setup (x64, without V8) for this solution

You downloaded **PDFium x64 (without V8)**. Great — that’s the recommended starting point.

### 1) Put the files in this repo

Create these folders (relative to the repo root):

```
external/pdfium/include/
external/pdfium/lib/x64/
external/pdfium/bin/x64/
```

Copy:
- **Headers** (e.g. `fpdfview.h`, `fpdf_edit.h`, `fpdf_save.h`, and any other PDFium headers) → `external/pdfium/include/`
- **Library** (`pdfium.lib`) → `external/pdfium/lib/x64/`
- **DLL** (`pdfium.dll`) → `external/pdfium/bin/x64/`

> If your `.lib` has a different name, either rename it to `pdfium.lib` **or** update the project setting “Additional Dependencies”.

### 2) Rebuild and run

In Visual Studio:
- Set configuration **Debug** and platform **x64**
- **Build → Rebuild Solution**
- Run the **Packaging** project (Startup Project: `Put A Signature (Package)`)

### 3) What should happen
- **Open PDF** should now render page 1.
- Page navigation (Prev/Next + page box) should work.

### Note (packaged apps)
This project loads PDFs using `StorageFile` → `FileIO::ReadBufferAsync` and then `FPDF_LoadMemDocument`.  
This avoids file-path access issues in packaged/MSIX apps and tends to be more reliable.

### Troubleshooting

- **Build error: cannot open include file `fpdfview.h`**
  - Your headers are not in `external/pdfium/include/` or include path not picked up.
- **Link error: cannot open file `pdfium.lib`**
  - Your `.lib` isn’t in `external/pdfium/lib/x64/` or it’s named differently.
- **Runtime error: `pdfium.dll` missing**
  - Make sure `external/pdfium/bin/x64/pdfium.dll` exists.
  - This repo copies it next to the exe on build, **and** includes it in the MSIX packaging project so the packaged app can launch.

