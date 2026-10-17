# Running the first Windows build

The source is in a local Git branch `atlas/main`; `upstream` points to the official
Bambu Studio repository. No GitHub fork or build run has been created yet.

1. In GitHub, fork https://github.com/bambulab/BambuStudio into your account.
2. In a terminal in this repository, using a working Git installation:

   ```sh
   git remote add origin https://github.com/YOUR_ACCOUNT/BambuStudio.git
   git push -u origin atlas/main
   ```

3. In your fork, open Actions. Enable workflows if GitHub asks. Choose
   **Atlas Windows baseline**, click **Run workflow**, and select `atlas/main`.
4. When it finishes, download the baseline artifact. If it fails, download
   `atlas-windows-diagnostics-*` and provide it for debugging.
5. On Windows 11 x64, extract the portable zip and run `bambu-studio.exe`.
   This is deliberately still named Bambu Studio for baseline comparison.
   Check startup, STL import, transforms, P2S selection, slicing, preview,
   export, save/reopen and profile persistence. Do not start a physical print.

No secrets or printer credentials are required for this workflow. GitHub runner
minutes and storage may be limited by your account. The first dependency build
may take considerable time; the initial workflow intentionally uses no cache.

## Toolchain choice

The checked-out source supports VS2022 and VS2026. Current upstream x64 CI uses
VS2026 on windows-latest, while the wiki recommends VS2022. This workflow chooses
windows-2022 / Visual Studio 17 2022 (v143) to avoid a moving runner alias, and pins
CMake 3.31.6 (within upstream's allowed range). Dependencies follow upstream CMake
recipes with DEP_DEBUG=OFF. Perl and pkgconfiglite follow upstream instructions.
CMake discovers the installed SDK rather than assuming an exact SDK folder.
Upstream C++ tests are enabled explicitly and zero discovered tests is an error.

## Local Mac development

The system Git currently fails because Apple Command Line Tools are missing.
Install them via `xcode-select --install` when convenient. This session used the
Git bundled with Codex successfully. A Windows MSVC build cannot execute on this
Mac; the workflow has not run simply because its YAML exists.

## Keeping upstream updates

Keep upstream master separate. The clone is shallow; before doing merges run
`git fetch --unshallow upstream` once. Then `git fetch upstream` and merge an
explicitly reviewed commit into a new integration branch. Record the new commit
in atlas/upstream.json and repeat the baseline build before profile work. Do not
use automated blanket search-and-replace to rename upstream identifiers.
