// Atlas AI Creator, 2026-11-06. SPDX-License-Identifier: AGPL-3.0-only
#include "CreatorPanel.hpp"
#include "PrimitiveGenerator.hpp"
#include "ModelSearch.hpp"
#include <wx/utils.h>
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Plater.hpp"
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace Slic3r::GUI {
AtlasAICreatorPanel::AtlasAICreatorPanel(wxWindow* parent, Plater* plater)
    : wxScrolledWindow(parent), m_plater(plater)
{
    SetScrollRate(0, FromDIP(12));
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* title = new wxStaticText(this, wxID_ANY, _L("Atlas AI Creator"));
    title->SetFont(GetFont().Larger().Bold());
    layout->Add(title, 0, wxALL, FromDIP(20));
    layout->Add(new wxStaticText(this, wxID_ANY,
        _L("Create a primitive locally, then preview and slice it in Prepare.")),
        0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    layout->Add(new wxStaticText(this, wxID_ANY,
        _L("Procedural tools available now. Voice and image generation are planned.")),
        0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));

    layout->Add(new wxStaticText(this, wxID_ANY, _L("Search existing models first (internet required)")),
        0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    auto* search = new wxTextCtrl(this, wxID_ANY);
    search->SetHint(_L("What would you like to make?"));
    search->SetMaxLength(256);
    layout->Add(search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    auto* provider = new wxChoice(this, wxID_ANY);
    provider->Append(_L("MakerWorld and STL Buddy"));
    provider->Append(_L("MakerWorld"));
    provider->Append(_L("STL Buddy"));
    provider->SetSelection(0);
    layout->Add(provider, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    auto* search_button = new wxButton(this, wxID_ANY, _L("Search existing models in browser"));
    layout->Add(search_button, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    search_button->Bind(wxEVT_BUTTON, [this, search, provider](wxCommandEvent&) {
        const auto query = search->GetValue().Strip(wxString::both);
        if (query.empty()) {
            m_status->SetLabel(_L("Enter a description to search for existing models."));
            return;
        }
        try {
            const auto text = query.ToStdString(wxConvUTF8);
            bool opened = true;
            if (provider->GetSelection() != 2)
                opened = wxLaunchDefaultBrowser(wxString::FromUTF8(AtlasAICreator::makerworld_search_url(text))) && opened;
            if (provider->GetSelection() != 1)
                opened = wxLaunchDefaultBrowser(wxString::FromUTF8(AtlasAICreator::stlbuddy_search_url(text))) && opened;
            m_status->SetLabel(opened ?
                _L("Search opened. Review the model license, download STL, then import it in Prepare. Or create a local shape below.") :
                _L("Could not open your browser. Check your default browser settings."));
        } catch (const std::invalid_argument&) {
            m_status->SetLabel(_L("Search description is too long."));
        }
        Layout(); FitInside();
    });

    auto* fields = new wxFlexGridSizer(2, FromDIP(12), FromDIP(16));
    fields->Add(new wxStaticText(this, wxID_ANY, _L("Shape")), 0, wxALIGN_CENTER_VERTICAL);
    m_shape = new wxChoice(this, wxID_ANY);
    m_shape->Append(_L("Box"));
    m_shape->Append(_L("Cylinder"));
    m_shape->Append(_L("Sphere"));
    m_shape->SetSelection(0);
    fields->Add(m_shape, 0, wxEXPAND);
    m_x_label = new wxStaticText(this, wxID_ANY, _L("Width (mm)"));
    fields->Add(m_x_label, 0, wxALIGN_CENTER_VERTICAL);
    m_x = new wxTextCtrl(this, wxID_ANY, "20");
    fields->Add(m_x, 0, wxEXPAND);
    fields->Add(new wxStaticText(this, wxID_ANY, _L("Depth (mm)")), 0, wxALIGN_CENTER_VERTICAL);
    m_y = new wxTextCtrl(this, wxID_ANY, "20");
    fields->Add(m_y, 0, wxEXPAND);
    fields->Add(new wxStaticText(this, wxID_ANY, _L("Height (mm)")), 0, wxALIGN_CENTER_VERTICAL);
    m_z = new wxTextCtrl(this, wxID_ANY, "20");
    fields->Add(m_z, 0, wxEXPAND);
    layout->Add(fields, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    auto* create = new wxButton(this, wxID_ANY, _L("Create and open in Prepare"));
    layout->Add(create, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    m_status = new wxStaticText(this, wxID_ANY,
        _L("Dimensions: 0.1–1000 mm. Printer bed limits still apply."));
    layout->Add(m_status, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    SetSizer(layout);
    FitInside();
    m_shape->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { update_fields(); });
    create->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { create_primitive(); });
    on_sys_color_changed();
}

void AtlasAICreatorPanel::update_fields()
{
    const int shape = m_shape->GetSelection();
    m_x_label->SetLabel(shape == 0 ? _L("Width (mm)") : _L("Diameter (mm)"));
    m_y->Enable(shape == 0);
    m_z->Enable(shape != 2);
    Layout();
    FitInside();
}

void AtlasAICreatorPanel::create_primitive()
{
    if (m_plater->only_gcode_mode() || m_plater->using_exported_file()) {
        m_status->SetLabel(_L("Open an editable project in Prepare before creating an object."));
        return;
    }
    if (!m_plater->can_add_model() || m_plater->is_any_job_running()) {
        m_status->SetLabel(_L("Wait for the current slicing or editing job to finish."));
        return;
    }
    AtlasAICreator::PrimitiveRequest request;
    request.shape = static_cast<AtlasAICreator::Primitive>(m_shape->GetSelection());
    if (!m_x->GetValue().ToDouble(&request.x) ||
        (m_y->IsEnabled() && !m_y->GetValue().ToDouble(&request.y)) ||
        (m_z->IsEnabled() && !m_z->GetValue().ToDouble(&request.z))) {
        m_status->SetLabel(_L("Enter a valid number in each enabled dimension field."));
        return;
    }
    try {
        auto mesh = AtlasAICreator::generate(request);
        const std::string name = "Atlas " + m_shape->GetStringSelection().ToStdString(wxConvUTF8);
        if (m_plater->add_generated_mesh(std::move(mesh), name))
            m_status->SetLabel(_L("Created. Use Prepare to inspect, export STL or slice. Undo removes the object."));
        else
            m_status->SetLabel(_L("Finish the current editing operation or slicing job, then try again."));
    } catch (const std::invalid_argument&) {
        m_status->SetLabel(_L("Dimensions must be between 0.1 and 1000 mm."));
    } catch (const std::exception& error) {
        m_status->SetLabel(_L("Could not create the object: ") + wxString::FromUTF8(error.what()));
    }
    Layout();
    FitInside();
}

void AtlasAICreatorPanel::on_sys_color_changed()
{
    wxGetApp().UpdateDarkUI(this);
    for (auto* child : GetChildren())
        wxGetApp().UpdateDarkUI(child);
    Refresh();
}
} // namespace Slic3r::GUI
