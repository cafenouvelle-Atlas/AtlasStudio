// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <wx/panel.h>
#include <memory>

namespace Slic3r::GUI {
class Plater;
class AtlasAssistantPanel final : public wxPanel {
public:
    AtlasAssistantPanel(wxWindow* parent, Plater* plater);
    ~AtlasAssistantPanel() override;
private:
    struct State;
    std::unique_ptr<State> m_state;
};
}
