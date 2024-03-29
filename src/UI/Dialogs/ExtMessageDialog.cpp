
// -----------------------------------------------------------------------------
// TSoSLADaE - It's a Fork of a Doom Editor!
// Copyright(C) 2008 - 2023 Simon Judd
// Copyright(C) 2022 - 2023 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
//
// TSoSLADaE Email:			  none
// TSoSLADaE Web:			  https://github.com/StarManiaKG/The-Story-of-Slicing-Language-Arrangements-Dramatically-and-Efficiently
//
// Filename:    ExtMessageDialog.cpp
// Description: A simple message dialog that displays a short message and a
//              scrollable extended text area, used to present potentially
//              lengthy text (error logs, etc)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//
// Includes
//
// -----------------------------------------------------------------------------
#include "Main.h"
#include "ExtMessageDialog.h"
#include "General/UI.h"

using namespace slade;


// -----------------------------------------------------------------------------
//
// ExtMessageDialog Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// ExtMessageDialog class constructor
// -----------------------------------------------------------------------------
ExtMessageDialog::ExtMessageDialog(wxWindow* parent, const wxString& caption) :
	wxDialog(parent, -1, caption, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	// Create and set sizer
	auto sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);

	// Add message label
	label_message_ = new wxStaticText(this, -1, "", wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
	sizer->Add(label_message_, 0, wxEXPAND | wxALL, ui::pad());

	// Add extended text box
	text_ext_ = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
	text_ext_->SetFont(wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
	sizer->Add(text_ext_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, ui::pad());

	// Add OK button
	auto hbox = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(hbox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, ui::pad());
	hbox->AddStretchSpacer(1);
	auto btn_ok = new wxButton(this, wxID_OK, "OK");
	btn_ok->SetDefault();
	hbox->Add(btn_ok);

	int size = ui::scalePx(500);
	SetInitialSize(wxSize(size, size));

	// Bind events
	Bind(wxEVT_SIZE, &ExtMessageDialog::onSize, this);
}

// -----------------------------------------------------------------------------
// Sets the dialog short message
// -----------------------------------------------------------------------------
void ExtMessageDialog::setMessage(const wxString& message) const
{
	label_message_->SetLabel(message);
}

// -----------------------------------------------------------------------------
// Sets the dialog extended text
// -----------------------------------------------------------------------------
void ExtMessageDialog::setExt(const wxString& text) const
{
	text_ext_->SetValue(text);
}


// -----------------------------------------------------------------------------
//
// ExtMessageDialog Class Events
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Called when the dialog is resized
// -----------------------------------------------------------------------------
void ExtMessageDialog::onSize(wxSizeEvent& e)
{
	Layout();
	label_message_->Wrap(label_message_->GetSize().GetWidth());
	Layout();
}
