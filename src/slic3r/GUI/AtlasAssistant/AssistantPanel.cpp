// SPDX-License-Identifier: AGPL-3.0-only
#include "AssistantPanel.hpp"
#include "Protocol.hpp"
#include "VoiceProtocol.hpp"
#include "GUI/I18N.hpp"
#include "GUI/Plater.hpp"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/filefn.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/timer.h>
#include <windows.h>
#include <bcrypt.h>
#include <mmsystem.h>
#include <sapi.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace Slic3r::GUI {
namespace {
using namespace AtlasAssistant;
constexpr auto mib = 1024ULL * 1024;
constexpr std::size_t max_memory_bytes = 1024;

bool available(unsigned minimum)
{
    MEMORYSTATUSEX mem {};
    mem.dwLength = sizeof(mem);
    return GlobalMemoryStatusEx(&mem) && mem.ullAvailPhys >= minimum * mib;
}

bool local_file(const wxString& path, unsigned limit)
{
    const std::wstring name = path.ToStdWstring();
    if (name.size() < 3 || name[1] != L':' || name[2] != L'\\' ||
        name.find(L':', 2) != std::wstring::npos || GetDriveTypeW(name.substr(0, 3).c_str()) != DRIVE_FIXED)
        return false;
    // Reject offline placeholders and reparse points in the file or any parent directory.
    for (auto part = name; part.size() > 3; part.resize(part.rfind(L'\\'))) {
        const auto attributes = GetFileAttributesW(part.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_OFFLINE))) return false;
    }
    auto size = wxFileName::GetSize(path);
    return size != wxInvalidSize && size.GetValue() > 0 && size.GetValue() <= limit * mib;
}

struct Worker {
    HANDLE map = nullptr, process = nullptr, job = nullptr;
    void* view = nullptr;
    std::wstring key;
    ULONGLONG deadline = 0;

    ~Worker() { close(); }

    void close()
    {
        // The job terminates the worker if Atlas is stopped or crashes.
        if (job) CloseHandle(job);
        if (process) CloseHandle(process);
        if (view) UnmapViewOfFile(view);
        if (map) CloseHandle(map);
        job = process = map = nullptr;
        view = nullptr;
    }

    bool prepare(DWORD bytes)
    {
        GUID id {};
        wchar_t guid[40] {};
        if (FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id, guid, 40)) return false;
        key = std::wstring(L"Local\\AtlasAssistant-") + guid;
        map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bytes, key.c_str());
        if (!map || GetLastError() == ERROR_ALREADY_EXISTS) { close(); return false; }
        view = MapViewOfFile(map, FILE_MAP_WRITE, 0, 0, bytes);
        if (!view) close();
        return view != nullptr;
    }

    bool launch(const wxString& helper, const wxString& model, unsigned seconds, unsigned limit)
    {
        wxFileName path(wxStandardPaths::Get().GetExecutablePath());
        path.SetFullName(helper);
        const auto exe = path.GetFullPath().ToStdWstring();
        // Both file names are absolute and cannot contain double quotes.
        std::wstring command = L"\"" + exe + L"\" \"" + key + L"\" \"" + model.ToStdWstring() + L"\"";
        STARTUPINFOW si {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi {};
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION caps {};
        caps.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
            JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        caps.BasicLimitInformation.ActiveProcessLimit = 1;
        caps.ProcessMemoryLimit = limit * mib;
        job = CreateJobObjectW(nullptr, nullptr);
        if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &caps, sizeof(caps)) ||
            !CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW | CREATE_SUSPENDED | BELOW_NORMAL_PRIORITY_CLASS,
                            nullptr, path.GetPath().wc_str(), &si, &pi)) {
            close();
            return false;
        }
        process = pi.hProcess;
        const bool ready = AssignProcessToJobObject(job, process) && ResumeThread(pi.hThread) != DWORD(-1);
        CloseHandle(pi.hThread);
        if (!ready) { TerminateProcess(process, 1); close(); return false; }
        deadline = GetTickCount64() + seconds * 1000ULL;
        return true;
    }
};

bool pack(ChatExchange& chat, Role role, const wxString& text)
{
    auto utf8 = text.ToUTF8();
    if (!utf8.data() || !utf8.length() || utf8.length() > max_message || chat.count >= max_messages) return false;
    auto& item = chat.messages[chat.count++];
    item.role = role;
    item.bytes = static_cast<std::uint32_t>(utf8.length());
    std::memcpy(item.text, utf8.data(), item.bytes);
    item.text[item.bytes] = 0;
    return true;
}

wxString memory_path(const wxString& project)
{
    if (project.empty()) return {};
    auto key = project.Lower().ToUTF8();
    if (!key.data()) return {};
    BCRYPT_ALG_HANDLE sha = nullptr;
    if (BCryptOpenAlgorithmProvider(&sha, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    unsigned char digest[32];
    const auto rc = BCryptHash(sha, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())), static_cast<ULONG>(key.length()), digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(sha, 0);
    if (rc < 0) return {};
    wxString name;
    for (unsigned char byte : digest) name += wxString::Format("%02x", static_cast<unsigned>(byte));
    wxFileName file(wxStandardPaths::Get().GetUserLocalDataDir(), name + ".txt");
    file.AppendDir("atlas-assistant-memory");
    return file.GetFullPath();
}

bool read_memory(const wxString& path, wxString& result)
{
    result.clear();
    if (path.empty() || !wxFileName::FileExists(path)) return true;
    wxFile file(path);
    return file.IsOpened() && file.Length() >= 0 && file.Length() <= max_memory_bytes &&
           file.ReadAll(&result, wxConvUTF8);
}
}

struct AtlasAssistantPanel::State {
    AtlasAssistantPanel& owner;
    Plater* plate;
    wxTimer timer;
    wxTextCtrl *history, *input;
    wxStaticText* status;
    wxButton *select_chat, *select_voice, *talk, *send, *stop, *clear, *memory;
    wxCheckBox* speak;
    wxString model, whisper, project, pending;
    wxString last_prompt, last_reply;
    Worker worker;
    enum class Activity { idle, listening, transcription, generation, speaking };
    Activity activity = Activity::idle;
    HWAVEIN microphone = nullptr;
    WAVEHDR audio {};
    std::array<std::int16_t, AtlasVoice::max_samples> pcm {};
    ULONGLONG record_until = 0;
    bool held = false;
    ISpVoice* speaker = nullptr;
    bool com = false;

    State(AtlasAssistantPanel& panel, Plater* p) : owner(panel), plate(p), timer(&panel)
    {
        auto* layout = new wxBoxSizer(wxVERTICAL);
        layout->Add(new wxStaticText(&owner, wxID_ANY,
            _L("Local mode. Chat and voice stay on this computer. Project notes are only saved when you edit them.")),
            0, wxALL, 10);
        auto* tools = new wxBoxSizer(wxHORIZONTAL);
        select_chat = new wxButton(&owner, wxID_ANY, _L("Choose chat model…"));
        select_voice = new wxButton(&owner, wxID_ANY, _L("Choose voice model…"));
        memory = new wxButton(&owner, wxID_ANY, _L("Project memory…"));
        clear = new wxButton(&owner, wxID_ANY, _L("Clear chat"));
        for (auto* b : {select_chat, select_voice, memory, clear}) tools->Add(b, 0, wxRIGHT, 6);
        layout->Add(tools, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
        history = new wxTextCtrl(&owner, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
        history->SetName(_L("Assistant conversation"));
        layout->Add(history, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
        input = new wxTextCtrl(&owner, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        input->SetName(_L("Message to Atlas Assistant"));
        input->SetMaxLength(1024);
        layout->Add(input, 0, wxEXPAND | wxALL, 10);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        send = new wxButton(&owner, wxID_ANY, _L("Send"));
        talk = new wxButton(&owner, wxID_ANY, _L("Hold to talk"));
        stop = new wxButton(&owner, wxID_ANY, _L("Stop"));
        speak = new wxCheckBox(&owner, wxID_ANY, _L("Speak replies using Windows voice"));
        for (auto* b : {send, talk, stop}) actions->Add(b, 0, wxRIGHT, 6);
        actions->Add(speak, 0, wxALIGN_CENTER_VERTICAL);
        layout->Add(actions, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
        status = new wxStaticText(&owner, wxID_ANY, _L("Choose local model files to begin."));
        layout->Add(status, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
        owner.SetSizer(layout);
        project = current_project();
        enable();

        select_chat->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { choose(false); });
        select_voice->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { choose(true); });
        memory->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { edit_memory(); });
        clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { clear_chat(); });
        send->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { request(); });
        input->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { request(); });
        stop->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { cancel(_L("Stopped.")); });
        talk->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { talk->SetFocus(); if (listen()) talk->CaptureMouse(); });
        talk->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) { if (activity == Activity::listening) transcribe(); });
        talk->Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) {
            if (activity == Activity::listening) cancel(_L("Recording stopped."));
        });
        talk->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
            if (event.GetKeyCode() == WXK_ESCAPE && activity == Activity::listening) {
                cancel(_L("Recording stopped."));
                return;
            }
            if (event.GetKeyCode() != WXK_SPACE) { event.Skip(); return; }
            if (!held) { held = true; listen(); }
        });
        talk->Bind(wxEVT_KEY_UP, [this](wxKeyEvent& event) {
            if (event.GetKeyCode() != WXK_SPACE) { event.Skip(); return; }
            held = false;
            if (activity == Activity::listening) transcribe();
        });
        talk->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
            held = false;
            if (activity == Activity::listening) cancel(_L("Recording stopped."));
            event.Skip();
        });
        owner.Bind(wxEVT_TIMER, [this](wxTimerEvent&) { poll(); }, timer.GetId());
        timer.Start(150);
    }

    ~State()
    {
        timer.Stop();
        activity = Activity::idle;
        stop_recording();
        if (speaker) {
            speaker->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
            speaker->Release();
        }
        if (com) CoUninitialize();
    }

    wxString current_project() const
    {
        return plate ? plate->get_project_filename(".3mf") : wxString();
    }
    bool slicing() const { return plate && plate->is_background_process_slicing(); }
    void enable()
    {
        bool idle = activity == Activity::idle;
        select_chat->Enable(idle);
        select_voice->Enable(idle);
        send->Enable(idle && !model.empty());
        input->SetEditable(idle);
        talk->Enable((idle && !whisper.empty()) || activity == Activity::listening);
        memory->Enable(idle);
        clear->Enable(idle);
        stop->Enable(!idle);
    }
    void set(Activity value, const wxString& message)
    {
        activity = value;
        status->SetLabel(message);
        enable();
    }
    void clear_chat()
    {
        last_prompt.clear();
        last_reply.clear();
        history->Clear();
        input->Clear();
        pending.clear();
    }
    void sync_project()
    {
        const auto now = current_project();
        if (now == project) return;
        cancel(_L("Project changed. Chat cleared."));
        clear_chat();
        project = now;
    }

    void choose(bool voice)
    {
        wxFileDialog dialog(&owner, voice ? _L("Choose a local Whisper model") : _L("Choose a local chat model"),
            {}, {}, voice ? _L("Whisper model (*.bin)|*.bin") : _L("GGUF chat model (*.gguf)|*.gguf"),
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK) return;
        if (!local_file(dialog.GetPath(), voice ? 200 : 1200)) {
            status->SetLabel(voice ? _L("Use a local Whisper model no larger than 200 MiB.") :
                _L("Use a local chat GGUF no larger than 1,200 MiB."));
            return;
        }
        (voice ? whisper : model) = dialog.GetPath();
        status->SetLabel(_L("Selected: ") + wxFileName(dialog.GetPath()).GetFullName());
        enable();
    }

    void edit_memory()
    {
        sync_project();
        if (project.empty() || !wxFileName::FileExists(project)) {
            status->SetLabel(_L("Save or open a project before editing its memory."));
            return;
        }
        const auto path = memory_path(project);
        wxString previous;
        if (path.empty() || !read_memory(path, previous)) {
            status->SetLabel(_L("Could not read project memory."));
            return;
        }
        wxTextEntryDialog dialog(&owner,
            _L("Review or edit these notes. Save empty notes to delete them. Notes are included in local replies."),
            _L("Atlas Assistant project memory"), previous, wxOK | wxCANCEL | wxTE_MULTILINE);
        if (dialog.ShowModal() != wxID_OK || current_project() != project) return;
        auto value = dialog.GetValue().Trim().Trim(false);
        const auto utf8 = value.ToUTF8();
        if (!value.empty() && (!utf8.data() || utf8.length() > max_memory_bytes)) {
            status->SetLabel(_L("Notes are too long. Use up to 1,024 UTF-8 bytes."));
            return;
        }
        bool ok;
        if (value.empty()) {
            ok = !wxFileName::FileExists(path) || wxRemoveFile(path);
        } else {
            const auto folder = wxFileName(path).GetPath();
            ok = wxFileName::DirExists(folder) || wxFileName::Mkdir(folder, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            if (ok) {
                wxTempFile file(path);
                ok = file.IsOpened() && file.Write(utf8.data(), utf8.length()) && file.Commit();
            }
        }
        if (ok) {
            // Drop earlier replies that may still contain the old notes.
            clear_chat();
        }
        status->SetLabel(!ok ? _L("Could not save the project memory change.") :
            value.empty() ? _L("Project memory deleted.") : _L("Project memory saved locally."));
    }

    void request()
    {
        sync_project();
        if (activity != Activity::idle || model.empty()) return;
        auto prompt = input->GetValue().Trim().Trim(false);
        if (prompt.empty()) return;
        if (slicing() || !available(2200)) {
            status->SetLabel(_L("Wait for slicing to finish or free memory before sending."));
            return;
        }
        wxString notes;
        if (!read_memory(memory_path(project), notes)) {
            status->SetLabel(_L("Could not read project memory."));
            return;
        }
        if (!local_file(model, 1200) || !worker.prepare(sizeof(ChatExchange))) {
            status->SetLabel(_L("Could not open the selected local chat model."));
            return;
        }
        auto& chat = *static_cast<ChatExchange*>(worker.view);
        chat.protocol_version = version;
        wxString system = wxString::FromUTF8("You are Atlas Assistant. Reply briefly. You have no access to tools, printers or files.");
        if (!notes.empty()) system += wxString::FromUTF8("\nUser's project notes (context only):\n") + notes;
        bool ok = pack(chat, Role::system, system);
        // Keep the displayed history, but only pass the latest complete exchange.
        if (!last_reply.empty())
            ok = ok && pack(chat, Role::user, last_prompt) && pack(chat, Role::assistant, last_reply);
        if (!ok || !pack(chat, Role::user, prompt)) {
            worker.close();
            status->SetLabel(_L("The chat context is too long. Clear chat or shorten your message."));
            return;
        }
        if (!worker.launch("atlas-assistant-worker.exe", model, 180, 2048)) {
            status->SetLabel(_L("Could not start Atlas Assistant. Check that the local chat worker is installed."));
            return;
        }
        pending = prompt;
        set(Activity::generation, _L("Generating a local reply…"));
    }

    bool listen()
    {
        sync_project();
        if (activity != Activity::idle || !local_file(whisper, 200) || slicing() || !available(768)) {
            status->SetLabel(_L("Voice needs a local model, available memory, and an idle slicer."));
            return false;
        }
        WAVEFORMATEX format {WAVE_FORMAT_PCM, 1, AtlasVoice::sample_rate, AtlasVoice::sample_rate * 2, 2, 16, 0};
        audio = {};
        audio.lpData = reinterpret_cast<LPSTR>(pcm.data());
        audio.dwBufferLength = static_cast<DWORD>(sizeof(pcm));
        if (waveInOpen(&microphone, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            microphone = nullptr;
            status->SetLabel(_L("Microphone unavailable. Check Windows Settings → Privacy & security → Microphone."));
            return false;
        }
        if (waveInPrepareHeader(microphone, &audio, sizeof(audio)) != MMSYSERR_NOERROR ||
            waveInAddBuffer(microphone, &audio, sizeof(audio)) != MMSYSERR_NOERROR ||
            waveInStart(microphone) != MMSYSERR_NOERROR) {
            cancel(_L("Could not start microphone capture."));
            return false;
        }
        record_until = GetTickCount64() + 15000;
        set(Activity::listening, _L("Listening… Release to transcribe."));
        return true;
    }
    std::uint32_t stop_recording()
    {
        if (!microphone) return 0;
        waveInReset(microphone);
        const auto count = audio.dwBytesRecorded / sizeof(std::int16_t);
        if (audio.dwFlags & WHDR_PREPARED) waveInUnprepareHeader(microphone, &audio, sizeof(audio));
        waveInClose(microphone);
        microphone = nullptr;
        if (talk->HasCapture()) talk->ReleaseMouse();
        return static_cast<std::uint32_t>(count);
    }
    void transcribe()
    {
        activity = Activity::idle; // Releasing capture can dispatch a capture-lost event.
        const auto samples = stop_recording();
        bool ok = AtlasVoice::valid_samples(samples) && !slicing() && available(768) &&
            local_file(whisper, 200) && worker.prepare(sizeof(AtlasVoice::Exchange));
        if (ok) {
            auto& voice = *static_cast<AtlasVoice::Exchange*>(worker.view);
            voice.protocol_version = AtlasVoice::version;
            voice.samples = samples;
            std::memcpy(voice.pcm, pcm.data(), samples * sizeof(std::int16_t));
            ok = worker.launch("atlas-voice-worker.exe", whisper, 60, 700);
        }
        SecureZeroMemory(pcm.data(), sizeof(pcm));
        set(ok ? Activity::transcription : Activity::idle, ok ? _L("Transcribing locally…") :
            !AtlasVoice::valid_samples(samples) ? _L("Hold the microphone button a little longer.") :
            _L("Could not start transcription. Check the voice worker, model and available memory."));
    }

    void cancel(const wxString& label)
    {
        activity = Activity::idle;
        stop_recording();
        SecureZeroMemory(pcm.data(), sizeof(pcm));
        worker.close();
        if (speaker) speaker->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
        pending.clear();
        set(Activity::idle, label);
    }
    bool say(const wxString& reply)
    {
        if (!speaker) {
            const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (SUCCEEDED(hr)) com = true;
            else if (hr != RPC_E_CHANGED_MODE) return false;
            if (FAILED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER, IID_ISpVoice,
                                        reinterpret_cast<void**>(&speaker)))) {
                if (com) { CoUninitialize(); com = false; }
                return false;
            }
        }
        ISpObjectTokenCategory* category = nullptr;
        if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_INPROC_SERVER,
                    IID_ISpObjectTokenCategory, reinterpret_cast<void**>(&category))))
            return false;
        IEnumSpObjectTokens* voices = nullptr;
        const auto enumerated = SUCCEEDED(category->SetId(SPCAT_VOICES, FALSE)) &&
                                SUCCEEDED(category->EnumTokens(L"Vendor=Microsoft", nullptr, &voices));
        category->Release();
        if (!enumerated || !voices) return false;
        ISpObjectToken* token = nullptr;
        const auto found = voices->Next(1, &token, nullptr);
        voices->Release();
        if (found != S_OK || !token) return false;
        const auto selected = speaker->SetVoice(token);
        token->Release();
        if (FAILED(selected)) return false;
        return SUCCEEDED(speaker->Speak(reply.wc_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, nullptr));
    }
    void poll()
    {
        if (!owner.IsShownOnScreen() && activity == Activity::idle) return;
        sync_project();
        if (activity == Activity::idle) return;
        if (!owner.IsShownOnScreen() || slicing() || !available(400)) {
            cancel(_L("Assistant stopped because the tab closed, slicing started, or available memory is low."));
            return;
        }
        if (activity == Activity::speaking) {
            if (speaker->WaitUntilDone(0) != S_FALSE) set(Activity::idle, _L("Ready."));
            return;
        }
        if (activity == Activity::listening) {
            DWORD foreground = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &foreground);
            if (foreground != GetCurrentProcessId() || wxWindow::FindFocus() != talk)
                cancel(_L("Recording stopped."));
            else if (GetTickCount64() >= record_until) transcribe();
            return;
        }
        const auto wait = WaitForSingleObject(worker.process, 0);
        if (wait == WAIT_TIMEOUT && GetTickCount64() < worker.deadline) return;
        DWORD code = 1;
        if (wait != WAIT_OBJECT_0 || !GetExitCodeProcess(worker.process, &code) || code) {
            cancel(wait == WAIT_TIMEOUT ? _L("Local worker timed out.") :
                _L("Local worker failed. Check the selected model and installed worker."));
            return;
        }
        if (activity == Activity::transcription) {
            const auto& voice = *static_cast<AtlasVoice::Exchange*>(worker.view);
            wxString text;
            if (voice.protocol_version == AtlasVoice::version && AtlasVoice::valid_text(voice.text_bytes))
                text = wxString::FromUTF8(voice.text, voice.text_bytes).Trim().Trim(false);
            worker.close();
            if (!text.empty()) input->ChangeValue(text.Left(1024));
            set(Activity::idle, text.empty() ? _L("No speech recognized.") : _L("Review your transcript, then press Send."));
            return;
        }
        const auto& chat = *static_cast<ChatExchange*>(worker.view);
        const auto result = chat.result;
        wxString reply;
        if (chat.protocol_version == version && result == Result::ok && chat.reply_bytes && chat.reply_bytes <= max_reply)
            reply = wxString::FromUTF8(chat.reply, chat.reply_bytes).Trim().Trim(false);
        worker.close();
        if (reply.empty()) {
            cancel(result == Result::prompt ? _L("This request did not fit the model context. Try Clear chat or a shorter message.") :
                _L("The local model could not generate a reply. Try the recommended small chat model."));
            return;
        }
        last_prompt = pending;
        last_reply = reply;
        if (history->GetLastPosition() > 24000) history->Clear();
        history->AppendText(_L("You: ") + pending + "\n\n" + _L("Atlas: ") + reply + "\n\n");
        pending.clear();
        input->Clear();
        const bool should_speak = speak->GetValue();
        const bool speaking = should_speak && say(reply);
        set(speaking ? Activity::speaking : Activity::idle,
            speaking ? _L("Speaking locally…") :
            should_speak ? _L("Reply ready. No Windows voice was available.") : _L("Reply ready."));
    }
};

AtlasAssistantPanel::AtlasAssistantPanel(wxWindow* parent, Plater* plater)
    : wxPanel(parent, wxID_ANY), m_state(std::make_unique<State>(*this, plater)) {}
AtlasAssistantPanel::~AtlasAssistantPanel() = default;
}
