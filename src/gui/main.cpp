#include <wx/event.h>

#include "maindialog.h"
#include "../agent.h"

MyFrame::MyFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxFrame(parent, id, title, pos, size, wxDEFAULT_FRAME_STYLE)
{

	const int ID_CONNECT = 1;

	SetSize(wxSize(625, 332));
	SetTitle(wxT("frame"));

	notebook_1 = new wxNotebook(this, wxID_ANY);
	notebook_1_pane_1 = new wxPanel(notebook_1, wxID_ANY);
	notebook_1->AddPage(notebook_1_pane_1, wxT("Account"));

	wxBoxSizer *sizer_2 = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *sizer_5 = new wxBoxSizer(wxHORIZONTAL);
	sizer_2->Add(sizer_5, 1, wxEXPAND, 0);
	wxStaticBitmap *bitmap_1 = new wxStaticBitmap(notebook_1_pane_1, wxID_ANY, wxNullBitmap);
	sizer_5->Add(bitmap_1, 0, 0, 0);
	wxStaticText *static_text_1 = new wxStaticText(notebook_1_pane_1, wxID_ANY, wxT("static_text_1"));
	sizer_5->Add(static_text_1, 0, wxALIGN_CENTER, 0);
	wxStaticText *static_text_2 = new wxStaticText(notebook_1_pane_1, wxID_ANY, wxT("static_text_2"));
	sizer_5->Add(static_text_2, 0, wxALIGN_CENTER, 0);
	wxStaticLine *static_line_1 = new wxStaticLine(notebook_1_pane_1, wxID_ANY);
	sizer_2->Add(static_line_1, 0, wxEXPAND, 0);
	wxBoxSizer *sizer_3 = new wxBoxSizer(wxHORIZONTAL);
	sizer_2->Add(sizer_3, 1, wxEXPAND, 0);
	list_box_1 = new wxListBox(notebook_1_pane_1, wxID_ANY, wxDefaultPosition);
	list_box_1->SetMinSize(wxSize(300, 150));

	sizer_3->Add(list_box_1, 0, 0, 0);
	wxBoxSizer *sizer_4 = new wxBoxSizer(wxHORIZONTAL);
	sizer_2->Add(sizer_4, 1, wxEXPAND, 0);

	button_1 = new wxButton(notebook_1_pane_1, ID_CONNECT, wxT("Connect"));
	Connect(ID_CONNECT, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::OnClickConnect));

	sizer_4->Add(button_1, 0, 0, 0);

	button_2 = new wxButton(notebook_1_pane_1, wxID_ANY, wxT("Disconnect"));

	sizer_4->Add(button_2, 0, 0, 0);

	notebook_1_Logactivity = new wxPanel(notebook_1, wxID_ANY);
	notebook_1->AddPage(notebook_1_Logactivity, wxT("Log activity"));
	wxBoxSizer *sizer_1 = new wxBoxSizer(wxVERTICAL);
	text_ctrl_1 = new wxTextCtrl(notebook_1_Logactivity, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(500,300), wxTE_MULTILINE);
	sizer_1->Add(text_ctrl_1, 0, wxEXPAND|wxSHAPED, 0);
	notebook_1_General = new wxPanel(notebook_1, wxID_ANY);
	notebook_1->AddPage(notebook_1_General, wxT("General"));
    
	notebook_1_Logactivity->SetSizer(sizer_1);
	notebook_1_pane_1->SetSizer(sizer_2);
	Layout();
}

class MyApp: public wxApp {
public:
	bool OnInit();
};

IMPLEMENT_APP(MyApp)

void MyFrame::OnClickConnect(wxCommandEvent &event)
{
	wxString	 wstr;
	int		 id;
	const char	*network;

	id = this->list_box_1->GetSelection();
	wstr = this->list_box_1->GetString(id);

	network = wstr.mb_str(wxConvUTF8);
	agent_thread_start(network);
}


// TODO makes it global for now.
MyFrame *frame;

void MyFrame::onConnect(const char *ip)
{

}

void MyFrame::onDisconnect()
{

}

void MyFrame::UpdateLog(wxString logline)
{
	frame->text_ctrl_1->AppendText(logline);
}

void MyFrame::onLog(const char *logline)
{
	/* This callback is fired from another thread. By using CallAfter(),
	 * the function UpdateLog() will be able to write the logline to the
	 * text widget from the GUI thread.
	 */
	frame->CallAfter(&MyFrame::UpdateLog, wxString::FromUTF8(logline));
}

void MyFrame::onListNetworks(const char *network)
{
	frame->list_box_1->Append( wxString::FromAscii(network));
}

bool MyApp::OnInit()
{
	ndb_init();
	agent_init_cb();

	wxInitAllImageHandlers();
	frame = new MyFrame(NULL, wxID_ANY, wxEmptyString);
	SetTopWindow(frame);

	agent_cb->connected = frame->onConnect;
	agent_cb->disconnected = frame->onDisconnect;
	agent_cb->log = frame->onLog;

	ndb_networks(frame->onListNetworks);

	frame->Show();
	return true;
}
