# Atlas Studio foundation additions, 2026-10-17. AGPL-3.0.
[CmdletBinding()]
param(
    [ValidateSet('All', 'Preflight', 'Dependencies', 'Configure', 'Build', 'Test', 'Package')]
    [string]$Stage = 'All'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'This script requires Windows and Visual Studio 2022.' }
$root = (Resolve-Path "$PSScriptRoot/../..").Path
Set-Location $root
New-Item -ItemType Directory -Force atlas-logs, atlas-dist | Out-Null
Start-Transcript -Path "atlas-logs/windows-$Stage.log"
$script:commandIndex = 0
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    $script:commandIndex++
    $log = "atlas-logs/$Stage-command-$script:commandIndex.txt"
    Write-Output "Running $Program $Arguments (full output: $log)"
    & $Program @Arguments *> $log
    $commandExit = $LASTEXITCODE
    Get-Content $log -Tail 30 | Write-Output
    if ($commandExit -ne 0) {
        # CTest failures need assertion context, not only the final summary.
        if ($Program -eq 'ctest') {
            Get-Content $log | Write-Output
        }
        # Keep every compiler error visible even when it precedes the last 30 lines.
        Select-String -Path $log -Pattern 'error C[0-9]+|error LNK[0-9]+|fatal error|CMake Error' |
            ForEach-Object { Write-Output $_.Line }
        throw "$Program failed with exit code $commandExit; see $log"
    }
}
try {
    Invoke-Checked git @('rev-parse', 'HEAD')
    Invoke-Checked cmake @('--version')
    Invoke-Checked perl @('--version')
    # Strawberry includes an extensionless Perl pkg-config wrapper that PowerShell
    # can launch but CMake execute_process cannot. Pin Chocolatey's native binary.
    $pkgConfigRoot = Join-Path $env:ChocolateyInstall 'lib/pkgconfiglite'
    $pkgConfig = Get-ChildItem $pkgConfigRoot -Recurse -File -Filter 'pkg-config.exe' |
        Select-Object -First 1
    if (-not $pkgConfig) { throw "Native pkg-config.exe missing under $pkgConfigRoot" }
    $pkgConfigExe = $pkgConfig.FullName.Replace('\', '/')
    Write-Output "Using native pkg-config: $pkgConfigExe"
    Invoke-Checked $pkgConfigExe @('--version')
    $env:PKG_CONFIG_EXECUTABLE = $pkgConfigExe
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vs = & $vswhere -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath
    if (-not $vs) { throw 'Visual Studio 2022 C++ desktop tools are required.' }
    $sdk = Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/Include" -Directory |
        Where-Object { Test-Path "$($_.FullName)/winrt/windows.graphics.printing3d.h" } |
        Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    if (-not $sdk) { throw 'No suitable Windows SDK was found.' }
    $deps = "$root/deps/build/BambuStudio_dep"
    $install = "$root/install-dir"
    $generator = @('-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_GENERATOR_INSTANCE=$vs")
    if ($Stage -eq 'Preflight') { return }
    if ($Stage -in @('All', 'Dependencies')) {
        Invoke-Checked cmake (@('-S', 'deps', '-B', 'deps/build') + $generator + @("-DDESTDIR=$deps", '-DCMAKE_BUILD_TYPE=Release', '-DDEP_DEBUG=OFF'))
        Invoke-Checked cmake @('--build', 'deps/build', '--config', 'Release', '--parallel', '2')
    }
    if ($Stage -in @('All', 'Configure')) {
        Invoke-Checked python @('atlas/scripts/apply-branding.py')
        Invoke-Checked python @('atlas/scripts/apply-centauri-profile.py')
        Invoke-Checked cmake (@('-S', '.', '-B', 'build') + $generator + @('-DBBL_RELEASE_TO_PUBLIC=1', '-DBBL_INTERNAL_TESTING=0', "-DCMAKE_PREFIX_PATH=$deps/usr/local", "-DCMAKE_INSTALL_PREFIX=$install", '-DCMAKE_BUILD_TYPE=Release', "-DWIN10SDK_PATH=$($sdk.FullName)", '-DSLIC3R_BUILD_TESTS=ON', "-DCMAKE_PROJECT_INCLUDE=$root/atlas/scripts/test-build-compat.cmake", "-DPKG_CONFIG_EXECUTABLE:FILEPATH=$pkgConfigExe"))
    }
    if ($Stage -in @('All', 'Build')) {
        # Validate the real profile with the engine before the much longer GUI build.
        # The complete build and test stages still run after this mandatory check.
        Invoke-Checked cmake @('--build', 'build', '--target', 'atlas_centauri_profile_tests', '--config', 'Release', '--parallel', '2')
        $env:PATH = "$install;$deps/usr/local/bin;$deps/usr/local/bin/occt;$env:PATH"
        Invoke-Checked ctest @('--test-dir', 'build', '-C', 'Release', '--output-on-failure', '-R', '^atlas_centauri_profile_tests$', '--no-tests=error', '--timeout', '600', '--output-junit', "$root/atlas-logs/centauri-preflight.xml")
        Invoke-Checked cmake @('--build', 'build', '--target', 'install', '--config', 'Release', '--parallel', '2')
    }
    if ($Stage -in @('All', 'Test')) {
        # Test executables may require runtime libraries staged by the install target.
        $env:PATH = "$install;$deps/usr/local/bin;$deps/usr/local/bin/occt;$env:PATH"
        Invoke-Checked ctest @('--test-dir', 'build', '-C', 'Release', '--output-on-failure', '--exclude-regex', '^(libnest2d_tests|libslic3r_tests|fff_print_tests|sla_print_tests)$', '--no-tests=error', '--timeout', '600', '--output-junit', "$root/atlas-logs/tests.xml")
    }
    if ($Stage -in @('All', 'Package')) {
        if (-not (Test-Path "$install/atlas-studio.exe")) { throw 'Installed GUI executable missing.' }
        if (-not (Test-Path "$install/resources/profiles/BBL.json")) { throw 'Installed printer profiles missing.' }
        Copy-Item LICENSE "$install/LICENSE"
        Copy-Item resources/profiles/Elegoo/ATLAS-CENTAURI-SETUP.txt "$install/ATLAS-CENTAURI-SETUP.txt"
        @'
Atlas Studio - Windows development candidate
Launch atlas-studio.exe from this extracted folder, with all DLLs and resources.
Based on Bambu Studio; original copyright and AGPL notices remain included.
See TEST-COVERAGE.txt and CTEST-RESULTS.xml for actual validation limitations.
source.zip contains the pinned upstream source and Atlas build scripts.
The build applies atlas/scripts/apply-branding.py before CMake configuration;
this reproducible overlay changes display names and the Windows launcher only.
'@ | Set-Content "$install/ATLAS-README.txt"
        $coverage = @'
Development baseline: only the selected regression tests run.
Deferred upstream suites: libnest2d_tests, libslic3r_tests, fff_print_tests,
sla_print_tests. They fail to compile against current upstream APIs.
This is NOT a full upstream test-suite pass. Windows GUI and printer testing
remain required. See ATLAS-BUILD-STATUS.md in the development workspace.
'@
        [xml]$testReport = Get-Content "$root/atlas-logs/tests.xml" -Raw
        $suite = $testReport.DocumentElement
        $profileCases = @($testReport.SelectNodes("//testcase[@name='atlas_centauri_profile_tests']"))
        if ($profileCases.Count -ne 1 -or $profileCases[0].SelectNodes('failure').Count -gt 0 -or
            $profileCases[0].SelectNodes('error').Count -gt 0 -or
            $profileCases[0].SelectNodes('skipped').Count -gt 0) {
            throw 'Centauri Carbon native load/slice/export test did not pass; package blocked.'
        }

        $testStatus = "Selected CTest results: tests=$($suite.GetAttribute('tests')); failures=$($suite.GetAttribute('failures')); errors=$($suite.GetAttribute('errors')); disabled=$($suite.GetAttribute('disabled'))."
        $coverage = "DEVELOPMENT CANDIDATE - validation incomplete. Do not use for physical printing.`n$testStatus`n" + $coverage
        Copy-Item "$root/atlas-logs/tests.xml" "$install/CTEST-RESULTS.xml"
        Copy-Item "$root/atlas-logs/tests.xml" atlas-dist/CTEST-RESULTS.xml
        $coverage | Out-File "$install/TEST-COVERAGE.txt"
        $coverage | Out-File atlas-dist/TEST-COVERAGE.txt
        # Preserve all checked-in license files with their original directory structure.
        $licenses = git ls-files | Where-Object { $_ -match '(?i)(license|notice|copying)' }
        foreach ($path in $licenses) {
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                $target = Join-Path "$install/third-party-notices" $path
                New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
                Copy-Item -LiteralPath $path -Destination $target
            }
        }
        # Include fetched dependency source trees and build recipes for review.
        # A public release still needs the dependency license/source audit in FOUNDATION.md.
        Invoke-Checked tar @('-czf', 'atlas-dist/dependency-build-inputs.tar.gz', '--exclude=BambuStudio_dep', '--exclude=*.obj', '--exclude=*.pdb', '--exclude=*.lib', 'deps/build')
        Invoke-Checked git @('archive', '--format=zip', '--output=atlas-dist/source.zip', 'HEAD')
        Compress-Archive -Path "$install/*" -DestinationPath atlas-dist/atlas-studio-windows-x64.zip -Force
        $hashLines = Get-ChildItem atlas-dist -File |
            Where-Object Name -ne 'SHA256SUMS.txt' |
            Get-FileHash -Algorithm SHA256 |
            ForEach-Object { "$($_.Hash.ToLowerInvariant())  $(Split-Path $_.Path -Leaf)" }
        $hashLines | Set-Content atlas-dist/SHA256SUMS.txt
        $coverage | Out-File atlas-dist/STATUS.txt
    }
} finally {
    Stop-Transcript
}
