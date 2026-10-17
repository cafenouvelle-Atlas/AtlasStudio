# Foundation audit and delivery status — 17 October 2026

## Evidence

Official repository: https://github.com/bambulab/BambuStudio
Inspected commit: 79df7ae309be398ed30c9f397c13c9911c2edc57 (master at checkout).
Windows guide: https://github.com/bambulab/BambuStudio/wiki/Windows-Compile-Guide
Local sources: CMakeLists.txt, deps/CMakeLists.txt, deps/deps-windows.cmake,
build_win.bat, .github/workflows/build_bambu.yml and build_deps.yml.
The wiki and current CI differ: CI now uses VS2026 on windows-latest; the source
explicitly supports VS2022 too. Atlas uses the latter for the first attempt.

## Architecture

| Component | Existing implementation |
|---|---|
| Slicing, geometry, supports, G-code | src/libslic3r |
| Model preparation, preview, workflow | src/slic3r/GUI/Plater.cpp and adjacent wxWidgets/OpenGL GUI |
| Presets, inheritance, project configuration | src/libslic3r/PresetBundle.cpp; resources/profiles |
| Device management | src/slic3r/GUI/DeviceManager.cpp |
| Network plugin loader | src/slic3r/Utils/NetworkAgent.cpp |
| Other print hosts | src/slic3r/Utils/PrintHost.cpp |
| Dependencies and packaging | deps, cmake, src/CMakeLists.txt, root CMakeLists.txt |
| Tests | tests/CMakeLists.txt; Catch2 and CTest; disabled by default |

The code is C++17 with CMake; upstream supports Windows, macOS and Linux.
Dependencies include Boost (recipe uses 1.84.0), wxWidgets, OpenVDB, CGAL, OCCT,
OpenSSL, FFmpeg, GMP/MPFR and bundled UI/geometry libraries. Use the source recipes
rather than replacing this stack. Existing project/profile persistence remains
untouched. A unified Devices experience will extend existing device UI later.

## Licenses and unavailable functionality

Read root LICENSE, including sections 4–6 (notices, modification notices and
Corresponding Source), and the upstream README licensing/network plugin notes.
Reviewed embedded license texts for ImGui and miniz (MIT-style), Catch2 (Boost
Software License), GLEW (multiple notices), and MCUT (GPL/commercial options).
The existing AboutDialog.cpp contains upstream third-party acknowledgements;
these and all existing copyright headers remain unchanged.

For distributed modified builds preserve legal notices, mark changes and dates,
license the covered derivative under AGPLv3, and provide matching Corresponding
Source and build scripts alongside binaries. Preserve each dependency's notices
and satisfy its separate source/redistribution terms. The workflow includes a
source archive, checked-in notices and fetched dependency build inputs. That is
supporting material, not a completed dependency compliance audit. Before public
release, inventory every fetched/prebuilt library, including FFmpeg, OCCT,
GMP/MPFR, OpenMeshCraft and web assets, confirm matching source availability and
license options, and include required notices/source. Do not publish a release
until this is resolved. No root LICENSE or third-party text has been rewritten.

Bambu's optional networking plugin is non-free (upstream README); its actual
implementation is not in this source tree. NetworkAgent dynamically loads it.
Cloud login, printer authorization, camera and multi-material network behaviour
cannot be guaranteed in a fork. Preserve source paths but do not redistribute a
plugin without checking its terms or bypass device authentication. SDK-dependent
Windows repair functionality is conditional in upstream CMake. Service access
and branding rights are not granted merely by the source license.

## Printer findings and boundaries

P2S exists in resources/profiles/BBL/machine/Bambu Lab P2S.json, model_id N7,
with 0.2/0.4/0.6/0.8 nozzle variants and associated filament/process profiles.
The 0.4 profile declares printable_height 256. These are source findings only:
firmware requirements, full inherited bed geometry, available connection modes,
AMS behaviour and actual printer compatibility still need official documentation
and device validation. No P2S profiles were changed.

No Centauri Carbon profile was found in this checkout. Its profile is deliberately
not fabricated. Milestone 3 must locate official ELEGOO configurations, record
firmware/version and license provenance, resolve inheritance, verify bed limits,
start/end sequences, output container and PLA/PETG settings. 256x256x256 is the
user's target, not a verified configuration here. LAN investigation is separate;
file export/manual transfer is the first target. No commands were sent to printers.

## Milestones and acceptance

1. Foundation: source clone, upstream remote, baseline branch, Windows workflow,
   scripts and audit prepared. Local integrity check executed. Windows compile,
   upstream CTest and packaging execution remain pending a GitHub runner.
2. Branding: pending baseline success. Change user-visible identity, icon, app
   configuration directory and file associations deliberately; preserve attribution.
3. ELEGOO: pending provenance/firmware review, baseline success and branding test.
4. Dual workflow: slice the same fixture per printer, assert machine metadata,
   boundaries and output type; save/reopen and switch profiles without leakage.
5. Connectivity: preserve authorized Bambu routes; add only verified ELEGOO routes.
6. Release: Windows UI acceptance, dependency/source audit, installer and release
   instructions. Require owner participation before any physical print.

All implemented changes in this milestone are build support/documentation only.
Build tested: NO. Software/UI tested: NO. Physically printer tested: NO.
The source integrity check is not a slicer functionality test.
