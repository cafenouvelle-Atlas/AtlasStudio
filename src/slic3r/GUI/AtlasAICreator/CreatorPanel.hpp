// Atlas AI Creator, 2026-11-06. SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <wx/scrolwin.h>

class wxChoice;
class wxStaticText;
class wxTextCtrl;

namespace Slic3r::GUI {
class Plater;

class AtlasAICreatorPanel final : public wxScrolledWindow {
public:
    AtlasAICreatorPanel(wxWindow* parent, Plater* plater);
    void on_sys_color_changed();
private:
    void update_fields();
    void create_primitive();
    Plater* m_plater;
    wxChoice* m_shape;
    wxStaticText* m_x_label;
    wxTextCtrl* m_x;
    wxTextCtrl* m_y;
    wxTextCtrl* m_z;
    wxStaticText* m_status;
};
} // namespace Slic3r::GUI
