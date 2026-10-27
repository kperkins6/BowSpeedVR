# Dependency provenance

- CommonLibSSE-NG v10.1.0, commit 39f9d07a6ffabea8fb559eee87ab7d27cd463e8a,
  https://github.com/alandtse/CommonLibSSE-NG — GPL-3.0-or-later.
- OpenVR SDK 1.0.15, commit 60eb187801956ad277f1cae6680e3a410ee0873b,
  https://github.com/ValveSoftware/openvr — BSD-3-Clause. The source download
  retains Valve's headers, Windows x64 import library, README and LICENSE;
  sample trees and SDK runtime/debug binaries are excluded.
- spdlog 1.16.0, https://github.com/gabime/spdlog — MIT.
- fmt 12.1.0, https://github.com/fmtlib/fmt — MIT.
- DirectX Tool Kit 2025-10-27 and DirectXMath 2025-04-03,
  https://github.com/microsoft/DirectXTK and https://github.com/microsoft/DirectXMath — MIT.
- rapidcsv 8.90, https://github.com/d99kris/rapidcsv — BSD-3-Clause.
- vcpkg registry baseline ee12231b20c95013c6638d845d04c91559a1d1ff,
  https://github.com/microsoft/vcpkg — MIT; build tooling only.

Full dependency notices are in `licenses/`. Sources and the required vcpkg
build recipes are included in the separate source archive. This project does
not redistribute Ricochet, Archery, Skyrim or SKSE binaries.
