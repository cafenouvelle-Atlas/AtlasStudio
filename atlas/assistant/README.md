# Atlas Assistant: Phase 2 development candidate

This opt-in Windows panel extends the Atlas Studio editor on the validated
filament repair base. Set `ATLAS_ENABLE_ASSISTANT=1` before launching an Atlas
Studio build from this branch. The existing eight Windows suites and their
assertions are unchanged. There is no cloud provider, credential storage, model
downloader or action broker in this phase.

## Windows setup and smoke checklist

1. Build Atlas Studio from this branch with the existing Windows workflow or
   build script. Separately obtain local Git checkouts of [llama.cpp v0.9.0
   (MIT)](https://github.com/ggml-org/llama.cpp/tree/cd959bae99ff4bed22bef2604f45241c09bbb8c8)
   and [whisper.cpp v1.8.2 (MIT)](https://github.com/ggml-org/whisper.cpp/tree/4979e04f5dcaccb36057e059bbaed8a2f5288315).
   Run the following in PowerShell from the repository root with VS 2022 C++
   tools, Git and CMake installed. This script builds from the supplied
   checkouts; it does not fetch packages or install software.
   ```powershell
   .\atlas\assistant\build-workers.ps1 -LlamaSource C:\deps\llama.cpp -WhisperSource C:\deps\whisper.cpp -BuildDirectory C:\build\atlas-assistant
   ```
   Copy the resulting two workers next to the Atlas launcher. Do not add them
   to an older build, which has no Assistant tab.
2. Obtain a local chat GGUF such as [Qwen2.5-0.5B-Instruct Q4_K_M
   (Apache-2.0)](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF) and
   the [Whisper tiny.en model](https://huggingface.co/ggerganov/whisper.cpp).
   Move files out of online-only directories to a local fixed drive. Prefer
   the small model on an 8 GB machine. Review each model's license separately
   from its runtime.
3. Launch from PowerShell with `$env:ATLAS_ENABLE_ASSISTANT='1'; .\atlas-studio.exe`.
   In **Atlas Assistant**, choose the GGUF. Disconnect the network and send
   “What is 2 + 2?” Verify the displayed reply, follow-up context, and Stop.
4. Choose tiny.en. Hold **Hold to talk** (or focus it and hold Space), then
   release. Verify that listening is visible only while recording and that
   the transcript is editable before Send. Test denied microphone access and
   cancellation. Select **Speak replies using Windows voice** and verify
   playback and Stop. Install a Windows desktop voice ahead of time if needed.
5. Open or save a project. Open **Project memory**, save a short fact, and
   ask about it. Restart with the same project; the note should persist and
   the conversation should be empty. Switch projects and confirm isolation.
   Save empty notes to delete the memory; reopen to confirm deletion.
6. Check that slicing or leaving the Assistant tab stops active work, and
   that the existing Creator, Bambu and Centauri workflows still operate.
   Verify responsive UI and peak RAM on the target Windows 11 machine.

## Resource and privacy design

Planning estimates, not Windows measurements: allow 2.5–4 GiB for Windows and
shared graphics, 0.6–1.8 GiB for an idle Atlas session (depending on the model
on the plate), and 0.8–1.6 GiB for a chat request with the recommended model.
Budget approximately 0.25–0.6 GiB for transcription or under 0.15 GiB for
Windows speech instead of chat. A large project or other running applications
can leave too little RAM to start the assistant. The default x64 worker build
uses the runtimes' x86 CPU instruction defaults, including AVX2; verify the
target CPU supports them.

- The chat and transcription providers are separate CPU worker processes with
  versioned, bounded memory exchanges. They load models on demand and exit
  after each request. Only one activity runs at a time. Stop closes the
  worker's Windows job; there is also a time limit and a low memory check.
- Chat uses two threads, a 1,024 token context and up to 128 reply tokens.
  It sends only the most recent complete exchange plus any project note and
  the current prompt. A large request can exceed this token budget even when
  it fits in the input field. Chat requires about 2.15 GiB of reported free
  physical memory before launch and caps worker private commit at 2 GiB.
  The OS, slicer, file mappings and integrated graphics also use RAM; this
  cap is not a measurement or a whole-system memory guarantee.
- Transcription uses two threads, mono 16 kHz input, a 15 second maximum
  recording and a 700 MiB worker private commit cap. It requires 768 MiB
  available before recording. Audio is kept in memory and is never written
  to a project file. Windows SAPI uses an installed Microsoft desktop voice
  without loading a separate neural model. [Piper](https://github.com/OHF-Voice/piper1-gpl)
  is a later alternative: its maintained runtime is GPL-3.0 and individual
  voices have separate terms.
- Chat history is session-only. User-entered notes (up to 1,024 UTF-8 bytes)
  are stored as text in the current user's local application data, keyed
  by the project's path. The note is not written into the project. Renaming
  or moving a project does not migrate its notes. The model receives those
  notes as local context; project files and printers are not exposed to it.
- Atlas code remains AGPL-3.0. If distributing the separate workers, include
  upstream license notices and fulfill the corresponding-source obligations
  for Atlas code. Windows voices are OS components and are not redistributed.
  Models are selected by the user and are not bundled.

For a portable CPU check, configure either worker's CMake project with
`-DATLAS_ASSISTANT_SMOKE=ON` and its local dependency path. Build
`atlas-assistant-smoke` and run it with a compatible GGUF. Build
`atlas-voice-smoke` and run it with tiny.en and the raw 16-bit little-endian
PCM samples extracted from whisper.cpp's mono 16 kHz `samples/jfk.wav`.
These checks exercise the same backend functions as the Windows workers;
they do not validate wxWidgets, the Windows process wrapper, microphone
hardware, project storage, or SAPI.

Verified on the macOS x86-64 development host: both CPU smoke targets compiled
and exited 0 with real models. Qwen2.5-0.5B Q4_K_M answered the arithmetic check
with `4`; Whisper tiny.en transcribed the sample speech. The Atlas source
integrity check passed. The full Atlas application and the two Windows worker
executables have not been compiled or tested on Windows for this phase.
