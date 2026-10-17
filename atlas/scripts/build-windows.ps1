# Atlas Studio foundation additions, 2026-10-17. AGPL-3.0.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'This script requires Windows and Visual Studio 2022.' }
$root = (Resolve-Path "$PSScriptRoot/../..").Path
Set-Location $root
New-Item -ItemType Directory -Force atlas-logs, atlas-dist | Out-Null
Start-Transcript -Path atlas-logs/windows-build.log
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
try {
    Invoke-Checked git @('rev-parse', 'HEAD')
    Invoke-Checked cmake @('--version')
    Invoke-Checked perl @('--version')
    Invoke-Checked pkg-config @('--version')
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
    Invoke-Checked cmake (@('-S', 'deps', '-B', 'deps/build') + $generator + @("-DDESTDIR=$deps", '-DCMAKE_BUILD_TYPE=Release', '-DDEP_DEBUG=OFF'))
    Invoke-Checked cmake @('--build', 'deps/build', '--config', 'Release', '--parallel', '2')
    Invoke-Checked cmake (@('-S', '.', '-B', 'build') + $generator + @('-DBBL_RELEASE_TO_PUBLIC=1', '-DBBL_INTERNAL_TESTING=0', "-DCMAKE_PREFIX_PATH=$deps/usr/local", "-DCMAKE_INSTALL_PREFIX=$install", '-DCMAKE_BUILD_TYPE=Release', "-DWIN10SDK_PATH=$($sdk.FullName)", '-DSLIC3R_BUILD_TESTS=ON'))
    Invoke-Checked cmake @('--build', 'build', '--target', 'install', '--config', 'Release', '--parallel', '2')
    # Test executables may require runtime libraries staged by the install target.
    $env:PATH = "$install;$deps/usr/local/bin;$deps/usr/local/bin/occt;$env:PATH"
    Invoke-Checked ctest @('--test-dir', 'build', '-C', 'Release', '--output-on-failure', '--no-tests=error', '--timeout', '600', '--output-junit', "$root/atlas-logs/tests.xml")
    if (-not (Test-Path "$install/bambu-studio.exe")) { throw 'Installed GUI executable missing.' }
    if (-not (Test-Path "$install/resources/profiles/BBL.json")) { throw 'Installed printer profiles missing.' }
    Copy-Item LICENSE "$install/LICENSE"
    Copy-Item atlas/README.md "$install/ATLAS-BASELINE-README.md"
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
    Compress-Archive -Path "$install/*" -DestinationPath atlas-dist/upstream-baseline-windows-x64.zip -Force
    Get-ChildItem atlas-dist -File | Get-FileHash -Algorithm SHA256 | Format-Table -AutoSize | Out-File atlas-dist/SHA256SUMS.txt
    'Compilation and CTest passed. GUI and physical printer testing remain required.' | Out-File atlas-dist/STATUS.txt
} finally {
    Stop-Transcript
}
