# SPDX-License-Identifier: AGPL-3.0-only
[CmdletBinding()]
param([ValidateSet('Build', 'Stage')][string]$Phase)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path "$PSScriptRoot/../..").Path
$work = Join-Path $env:RUNNER_TEMP 'atlas-assistant'
$install = Join-Path $root 'install-dir'
$llama = Join-Path $work 'llama'
$whisper = Join-Path $work 'whisper'
$chatModel = Join-Path $work 'qwen.gguf'
$voiceModel = Join-Path $work 'tiny.en.bin'
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
function Fetch-Pinned([string]$Url, [string]$Revision, [string]$Path) {
    Invoke-Checked git @('init', $Path)
    Invoke-Checked git @('-C', $Path, 'fetch', '--depth', '1', $Url, $Revision)
    Invoke-Checked git @('-C', $Path, 'checkout', '--detach', 'FETCH_HEAD')
}
function Fetch-Model([string]$Url, [string]$Path, [string]$Hash) {
    Invoke-Checked curl.exe @('-fLsS', '--retry', '2', '--max-time', '240', $Url, '-o', $Path)
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash) {
        throw "Unexpected test model SHA256: $Path"
    }
}
if ($Phase -eq 'Build') {
    New-Item -ItemType Directory -Force $work | Out-Null
    Fetch-Pinned 'https://github.com/ggml-org/llama.cpp.git' 'cd959bae99ff4bed22bef2604f45241c09bbb8c8' $llama
    Fetch-Pinned 'https://github.com/ggml-org/whisper.cpp.git' '4979e04f5dcaccb36057e059bbaed8a2f5288315' $whisper
    & "$PSScriptRoot/build-workers.ps1" -LlamaSource $llama -WhisperSource $whisper -BuildDirectory "$work/build"
    Fetch-Model 'https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/qwen2.5-0.5b-instruct-q4_k_m.gguf' $chatModel '74a4da8c9fdbcd15bd1f6d01d621410d31c6fc00986f5eb687824e7b93d7a9db'
    Fetch-Model 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin' $voiceModel '921e4cf8686fdd993dcd081a5da5b6c365bfde1162e72b08d75ac75289920b1f'
    New-Item -ItemType Directory -Force "$work/smoke" | Out-Null
    Copy-Item "$work/build/chat/Release/atlas-assistant-worker.exe", "$work/build/voice/Release/atlas-voice-worker.exe" "$work/smoke"
    Invoke-Checked python @("$PSScriptRoot/verify-windows-workers.py", "$work/smoke", $chatModel, $voiceModel, "$whisper/samples/jfk.wav")
}
if ($Phase -eq 'Stage') {
    if (-not (Test-Path "$install/atlas-studio.exe" -PathType Leaf)) { throw 'Installed Atlas executable is missing.' }
    Copy-Item "$work/smoke/atlas-assistant-worker.exe", "$work/smoke/atlas-voice-worker.exe" $install
    Copy-Item "$PSScriptRoot/Start Atlas Assistant.cmd", "$PSScriptRoot/TEST-WINDOWS.txt" $install
    $notices = Join-Path $install 'third-party-notices'
    New-Item -ItemType Directory -Force $notices | Out-Null
    Copy-Item "$llama/LICENSE" "$notices/llama.cpp-LICENSE.txt"
    Copy-Item "$whisper/LICENSE" "$notices/whisper.cpp-LICENSE.txt"
    Invoke-Checked git @('-C', $llama, 'archive', '--format=zip', "--output=$root/atlas-dist/llama-cpp-source.zip", 'HEAD')
    Invoke-Checked git @('-C', $whisper, 'archive', '--format=zip', "--output=$root/atlas-dist/whisper-cpp-source.zip", 'HEAD')
}
