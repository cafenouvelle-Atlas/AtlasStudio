# SPDX-License-Identifier: AGPL-3.0-only
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$LlamaSource,
    [Parameter(Mandatory = $true)][string]$WhisperSource,
    [Parameter(Mandatory = $true)][string]$BuildDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

$LlamaSource = (Resolve-Path -LiteralPath $LlamaSource).Path
$WhisperSource = (Resolve-Path -LiteralPath $WhisperSource).Path
$llamaCommit = & git -C $LlamaSource rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $llamaCommit -ne "cd959bae99ff4bed22bef2604f45241c09bbb8c8") {
    throw "Use the reviewed llama.cpp v0.9.0 checkout."
}
$whisperCommit = & git -C $WhisperSource rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $whisperCommit -ne "4979e04f5dcaccb36057e059bbaed8a2f5288315") {
    throw "Use the reviewed whisper.cpp v1.8.2 checkout."
}

$BuildDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDirectory)
$chatBuild = Join-Path $BuildDirectory "chat"
$voiceBuild = Join-Path $BuildDirectory "voice"
$generator = "Visual Studio 17 2022"

Invoke-Checked cmake @("-S", $PSScriptRoot, "-B", $chatBuild, "-G", $generator, "-A", "x64", "-DLLAMA_SOURCE_DIR=$LlamaSource")
Invoke-Checked cmake @("--build", $chatBuild, "--config", "Release", "--target", "atlas-assistant-worker", "--parallel", "2")
Invoke-Checked cmake @("-S", (Join-Path $PSScriptRoot "voice"), "-B", $voiceBuild, "-G", $generator, "-A", "x64", "-DWHISPER_SOURCE_DIR=$WhisperSource")
Invoke-Checked cmake @("--build", $voiceBuild, "--config", "Release", "--target", "atlas-voice-worker", "--parallel", "2")

Write-Output "Copy these two workers next to atlas-studio.exe from the same Atlas Assistant branch:"
Get-Item (Join-Path $chatBuild "Release\atlas-assistant-worker.exe"),
         (Join-Path $voiceBuild "Release\atlas-voice-worker.exe") |
    Select-Object -ExpandProperty FullName
