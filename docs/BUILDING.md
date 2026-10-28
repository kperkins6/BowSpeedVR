# Build the release

## Desktop editor

The editor builds with Windows x64 MSVC/CMake and no CommonLib, vcpkg, Python or UI framework dependency. Run `scripts/build-editor.ps1`. It builds the editor and native tests in `build-editor`; its executable links the C++ runtime statically. The editor and SKSEVR plugin compile the same physics sources.

For an existing configured plugin build, run `cmake --build build-release` to produce both. Run `python tests/test_editor_cli.py build-release/BowSpeedVREditor.exe` and `python tests/test_editor_package.py` after building the matching DLL. Package them with `python scripts/package-editor.py --build-dir build-release --output <directory>`; this validates actual executable/DLL versions and emits separate Editor and Runtime ZIPs plus hashes. It does not publish or install them.

The optional MGO preset is a data file in `presets/`, copied alongside a built editor. Regeneration instructions are in `presets/README.md`. The scanner's unmodified puff decoder and zlib license are vendored in `editor/vendor/`.

## SKSEVR runtime and source

Use Windows x64 MSVC C++23, CMake3.25+ and Ninja. CommonLibSSE-NG is pinned to39f9d07a6ffabea8fb559eee87ab7d27cd463e8a with OpenVR60eb187801956ad277f1cae6680e3a410ee0873b. Other versions and licenses are listed in THIRD-PARTY.md and the included dependency source manifest.

Run scripts/test.ps1 for the portable suite. Run scripts/build.ps1 with CommonLib, Vcpkg and Dependencies paths to compile the plugin and Release tests. Use CMAKE_TRY_COMPILE_CONFIGURATION=Release when a host's Debug PDB server differs from its compiler. Cached builds may use VCPKG_MANIFEST_INSTALL=OFF only after the pinned dependencies are installed.

Run tests/test_release_packages.py with Python3. Then use scripts/release.py --output <release-directory> --dependency-sources <matching-dependency-source-directory>. The script creates the main standalone ZIP, configuration-only MGO FOMOD and matching source ZIP. Use the included dependency source snapshots and manifest, or obtain exactly the listed revisions and source distributions. No game executable is required in the public source archive.

The release's **source ZIP** contains the implementation, tests, reviewed equipment CSVs, generated profiles, matching dependency sources and licenses. GitHub's automatically generated repository archives contain tracked project files; obtain the pinned dependencies separately when building those archives. OpenVR in the self-contained source ZIP is a filtered SDK snapshot with the headers, Windows x64 import library and Valve notices needed by CommonLib; sample tools and runtime/debug binaries are excluded. The manifest records the original snapshot hash, retained paths and resulting hash. Local research logs and third-party help text are excluded.

The build.ps1 wrapper requires pinned Git checkouts, including the OpenVR submodule. For the Gitless source snapshots instead, verify their SHA-256 hashes against dependencies/manifest.json, extract CommonLibSSE-NG.zip into a commonlib directory and OpenVR.zip into commonlib/extern/openvr. Use a bootstrapped vcpkg checkout at the listed registry baseline. After importing scripts/Import-Toolchain.ps1, configure directly (replace the example paths with your extracted directories):

```powershell
& $CMake -S . -B build-release -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release `
  -DBOWSPEED_BUILD_PLUGIN=ON -DCOMMONLIB_SOURCE_DIR=C:/build/commonlib `
  -DCMAKE_TOOLCHAIN_FILE=C:/build/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
& $CMake --build build-release --parallel 8
& $CTest --test-dir build-release --output-on-failure
```

The CMake configuration maps plugin and dependency source paths to neutral names before compilation. The package builder checks the DLL's narrow and UTF-16 strings and refuses personal build paths. It verifies dependency snapshot hashes before applying the documented OpenVR filter.
