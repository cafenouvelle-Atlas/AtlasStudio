// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace Slic3r::GUI::AtlasAssistant {

enum class Command { get_slice_status, get_active_plate, select_plate, navigate_preview };
struct ActionProposal {
    Command command;
    int plate = 0; // One-based, and present only for select_plate.
};

enum class CommandStatus { accepted, unsupported, clarify, malformed, cancelled, backend_failure };
struct CommandDecision {
    CommandStatus status;
    std::optional<ActionProposal> action = std::nullopt;
};

inline int positive_plate(std::string_view text)
{
    if (text.empty() || text.front() < '1' || text.front() > '9') return 0;
    int number = 0;
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, number);
    return result.ec == std::errc{} && result.ptr == end ? number : 0;
}

inline CommandDecision recognize_command(std::string_view request)
{
    if (request.size() > 256) return {CommandStatus::unsupported};
    std::string phrase;
    for (char c : request) {
        if (c == ' ' || c == '\t') {
            if (!phrase.empty() && phrase.back() != ' ') phrase.push_back(' ');
        } else {
            phrase.push_back(c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c);
        }
    }
    if (!phrase.empty() && phrase.back() == ' ') phrase.pop_back();
    if (!phrase.empty() && std::string_view(".?!").find(phrase.back()) != std::string_view::npos) phrase.pop_back();
    std::string_view words(phrase);
    if (words.substr(0, 7) == "please ") words.remove_prefix(7);

    if (words == "get_slice_status" || words == "slice status" || words == "what is the slice status" ||
        words == "is it slicing" || words == "is the current plate sliced")
        return {CommandStatus::accepted, ActionProposal{Command::get_slice_status}};
    if (words == "get_active_plate" || words == "active plate" || words == "which plate is active" ||
        words == "what is the active plate")
        return {CommandStatus::accepted, ActionProposal{Command::get_active_plate}};
    if (words == "navigate_preview" || words == "open preview" || words == "show preview" ||
        words == "go to preview" || words == "show the preview")
        return {CommandStatus::accepted, ActionProposal{Command::navigate_preview}};
    for (std::string_view prefix : {"select plate", "switch to plate", "go to plate", "select_plate"}) {
        if (words == prefix) return {CommandStatus::clarify};
        if (words.substr(0, prefix.size()) != prefix || words.size() <= prefix.size() ||
            words[prefix.size()] != ' ') continue;
        const auto target = words.substr(prefix.size() + 1);
        int number = positive_plate(target);
        constexpr std::string_view numbers[] = {"one", "two", "three", "four", "five",
                                                "six", "seven", "eight", "nine", "ten"};
        for (int i = 0; i < 10 && number == 0; ++i)
            if (target == numbers[i]) number = i + 1;
        if (number == 0) return {CommandStatus::clarify};
        return {CommandStatus::accepted, ActionProposal{Command::select_plate, number}};
    }
    return {CommandStatus::unsupported};
}

// The model may emit exactly one command name, with a single decimal argument
// for select_plate. No prose, extra arguments, or multiple actions are accepted.
inline CommandDecision parse_proposal(std::string_view response)
{
    if (response.size() > 64) return {CommandStatus::malformed};
    const auto first = response.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {CommandStatus::malformed};
    response = response.substr(first, response.find_last_not_of(" \t\r\n") - first + 1);
    if (response == "clarify") return {CommandStatus::clarify};
    if (response == "unsupported") return {CommandStatus::unsupported};
    if (response == "get_slice_status") return {CommandStatus::accepted, ActionProposal{Command::get_slice_status}};
    if (response == "get_active_plate") return {CommandStatus::accepted, ActionProposal{Command::get_active_plate}};
    if (response == "navigate_preview") return {CommandStatus::accepted, ActionProposal{Command::navigate_preview}};
    constexpr std::string_view prefix = "select_plate ";
    if (response.substr(0, prefix.size()) == prefix)
        if (const int number = positive_plate(response.substr(prefix.size())))
            return {CommandStatus::accepted, ActionProposal{Command::select_plate, number}};
    return {CommandStatus::malformed};
}

class CommandBroker {
public:
    void begin(ActionProposal requested) { expected = requested; }
    void cancel() { expected.reset(); }

    CommandDecision complete(std::optional<std::string_view> reply)
    {
        const auto requested = expected;
        cancel();
        if (!requested) return {CommandStatus::cancelled};
        if (!reply) return {CommandStatus::backend_failure};
        const auto proposal = parse_proposal(*reply);
        if (!proposal.action) return proposal;
        if (proposal.action->command != requested->command || proposal.action->plate != requested->plate)
            return {CommandStatus::clarify};
        return proposal;
    }

private:
    std::optional<ActionProposal> expected;
};

inline constexpr char command_prompt[] =
    "Translate one Atlas navigation request. Reply with exactly one line, no punctuation. "
    "For selecting a plate, output select_plate followed by a space and the decimal plate number. "
    "For opening the preview, output navigate_preview. "
    "Examples: Select plate two -> select_plate 2; Switch to plate 12 -> select_plate 12; "
    "Go to plate 3 -> select_plate 3; Open Preview -> navigate_preview. "
    "Copy the command name exactly, including its underscore. "
    "If uncertain, output clarify. If unsupported, output unsupported.";

}
