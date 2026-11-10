// SPDX-License-Identifier: AGPL-3.0-only
#include "slic3r/GUI/AtlasAssistant/CommandBroker.hpp"

#include <cstdlib>
#include <iostream>

using namespace Slic3r::GUI::AtlasAssistant;

void expect(bool condition, const char* label)
{
    if (condition) return;
    std::cerr << label << '\n';
    std::exit(1);
}

void action_is(CommandDecision decision, Command command, int plate = 0)
{
    expect(decision.status == CommandStatus::accepted && decision.action &&
           decision.action->command == command && decision.action->plate == plate,
           "expected exactly the requested action");
}

void reject(CommandDecision decision, CommandStatus status)
{
    expect(decision.status == status && !decision.action, "expected rejection without an action");
}

int main()
{
    action_is(recognize_command("  What is the slice status? "), Command::get_slice_status);
    action_is(recognize_command("get_active_plate"), Command::get_active_plate);
    action_is(recognize_command("Please open Preview."), Command::navigate_preview);
    action_is(recognize_command("Switch to plate two."), Command::select_plate, 2);
    action_is(recognize_command("select_plate 12"), Command::select_plate, 12);
    for (auto request : {"select plate", "select plate 1 or 2", "select plate 2 and print",
                         "select plate -1", "select plate 0", "select plate 2147483648"})
        reject(recognize_command(request), CommandStatus::clarify);
    for (auto request : {"do not open preview", "export G-code", "print plate 2", "run cmd.exe",
                         "open preview\nselect plate 2"})
        reject(recognize_command(request), CommandStatus::unsupported);

    action_is(parse_proposal(" \nget_slice_status\r\n"), Command::get_slice_status);
    action_is(parse_proposal("get_active_plate"), Command::get_active_plate);
    action_is(parse_proposal("navigate_preview"), Command::navigate_preview);
    action_is(parse_proposal("select_plate 2"), Command::select_plate, 2);
    for (auto response : {"", "print_plate 2", "get_active_plate 2", "select_plate 02", "select_plate -2",
                          "select_plate 2.0", "select_plate 2 extra", "select_plate 2147483648",
                          "select_plate 1\nnavigate_preview", "Sure: navigate_preview", "navigate_preview."})
        reject(parse_proposal(response), CommandStatus::malformed);
    reject(parse_proposal(std::string_view("select_plate 2\0oops", 19)), CommandStatus::malformed);

    CommandBroker broker;
    broker.begin({Command::select_plate, 2});
    reject(broker.complete("select_plate 3"), CommandStatus::clarify);
    reject(broker.complete("select_plate 2"), CommandStatus::cancelled);
    broker.begin({Command::select_plate, 2});
    reject(broker.complete("navigate_preview"), CommandStatus::clarify);
    broker.begin({Command::navigate_preview});
    reject(broker.complete("clarify"), CommandStatus::clarify);
    broker.begin({Command::navigate_preview});
    reject(broker.complete("unsupported"), CommandStatus::unsupported);
    broker.begin({Command::navigate_preview});
    reject(broker.complete("open preview"), CommandStatus::malformed);
    broker.begin({Command::navigate_preview});
    broker.cancel();
    reject(broker.complete("navigate_preview"), CommandStatus::cancelled);
    broker.begin({Command::navigate_preview});
    reject(broker.complete(std::nullopt), CommandStatus::backend_failure);
    reject(broker.complete("navigate_preview"), CommandStatus::cancelled);
    broker.begin({Command::select_plate, 2});
    action_is(broker.complete("select_plate 2"), Command::select_plate, 2);
    reject(broker.complete("select_plate 2"), CommandStatus::cancelled);

    std::cout << "Command broker validation passed.\n";
}
