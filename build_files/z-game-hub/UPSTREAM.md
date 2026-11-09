# ZGameHub upstream

This directory vendors the Linux-buildable source, UI, assets, license, and third-party notices from [ZystemZ/ZGameHub](https://github.com/ZystemZ/ZGameHub).

- Upstream revision: `bd79eba8e1a7c7462d03d02f82968bae4b86cfe4` (2026-11-09)
- Upstream project version: 1.3.0
- Vendored paths: `CMakeLists.txt`, `src/`, `ui/`, `assets/`, `LICENSE`, and `THIRD-PARTY-NOTICES.txt`
- Upstream Windows build outputs and installer archives are intentionally excluded from the OS build context.

`CMakeLists.txt` and `ui/Main.qml` are patched locally for the Linux menu entry and icon, fullscreen console startup, browser and legacy-store launch actions, and confirmed return to the desktop. To update this snapshot, review the upstream changes and notices, replace the vendored source paths from a selected upstream commit, retain the local integration patches, update this revision, and run the ZConsole image checks.
