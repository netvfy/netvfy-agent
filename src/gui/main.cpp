#include <wx/event.h>
#include <wx/taskbar.h>

#include "maindialog.h"
#include "../agent.h"

#include "rc/nvagent_ico.xpm"

static int	 systray_state = true;

MyFrame::MyFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxFrame(parent, id, title, pos, size, wxNETVFYDEFAULT)
{
	const int ID_CONNECT = 1;
	const int ID_DISCONNECT = 2;
	const int ID_ADD_NETWORK = 3;
	const int ID_DELETE_NETWORK = 4;
	const int ID_EXIT = 5;

	SetSize(wxSize(370, 270));
	SetTitle(wxT("netvfy-agent"));
#ifdef WIN32
	SetIcon(wxICON(AppIcon));
#endif

	notebook_1 = new wxNotebook(this, wxID_ANY);
	notebook_1_pane_1 = new wxPanel(notebook_1, wxID_ANY);
	notebook_1->AddPage(notebook_1_pane_1, wxT("Account"));

	wxBoxSizer *sizer_2 = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *sizer_3 = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *sizer_4 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *sizer_5 = new wxBoxSizer(wxVERTICAL);

	static_text_1 = new wxStaticText(notebook_1_pane_1,
		wxID_ANY, wxT("Disconnected."));
	static_text_2 = new wxTextCtrl(notebook_1_pane_1, wxID_ANY,
		wxEmptyString, wxDefaultPosition, wxSize(-1,-1), wxTE_READONLY | wxNO_BORDER);
	static_text_2->SetBackgroundColour(this->GetBackgroundColour());

	list_box_1 = new wxListBox(notebook_1_pane_1, wxID_ANY, wxDefaultPosition);
	list_box_1->SetMinSize(wxSize(250, 150));

	button_1 = new wxButton(notebook_1_pane_1, ID_CONNECT, wxT("Connect"));
	Connect(ID_CONNECT, wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(MyFrame::onClickConnect));

	button_1_b = new wxButton(notebook_1_pane_1, ID_DISCONNECT, wxT("Disconnect"));
	Connect(ID_DISCONNECT, wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(MyFrame::onClickDisconnect));
	button_1_b->Enable(false);

	button_2 = new wxButton(notebook_1_pane_1, ID_ADD_NETWORK, wxT("Add"));
	Connect(ID_ADD_NETWORK, wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(MyFrame::onClickAddNetwork));

	button_3 = new wxButton(notebook_1_pane_1, ID_DELETE_NETWORK, wxT("Delete"));
	Connect(ID_DELETE_NETWORK, wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(MyFrame::onClickDeleteNetwork));

	button_exit = new wxButton(notebook_1_pane_1, ID_EXIT, wxT("Exit"));
	Connect(ID_EXIT, wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(MyFrame::onClickExit));

	/* The order the widgets are added to the sizers is important */
	sizer_2->Add(sizer_3, 1, wxEXPAND, 0);
	sizer_2->Add(sizer_4, 1, wxEXPAND, 0);

	sizer_3->Add(10,10,0,0,0);
	sizer_3->Add(static_text_1, 1, wxALIGN_LEFT, 0);
	sizer_3->Add(static_text_2, 1, wxALIGN_LEFT, 0);
	sizer_3->Add(10,10,0,0,0);

	sizer_4->Add(list_box_1, 0, 0, 0);
	sizer_4->Add(sizer_5, 1, wxEXPAND, 0);

	sizer_5->Add(button_1, 0, wxALIGN_CENTER, 0);
  	sizer_5->Add(button_1_b, 0, wxALIGN_CENTER, 0);
	sizer_5->Add(10,10,0,0,0);
	sizer_5->Add(button_2, 0, wxALIGN_CENTER, 0);
	sizer_5->Add(button_3, 0, wxALIGN_CENTER, 0);
#ifdef WIN32
	sizer_5->Add(10,35,0,0,0);
#else
	sizer_5->Add(10,35,0,0,0);
#endif
	sizer_5->Add(button_exit, 0, wxALIGN_CENTER, 0);


	/* TODO (we don't need it for now)
	button_2 = new wxButton(notebook_1_pane_1, ID_DISCONNECT, wxT("Disconnect!"));
	Connect(ID_DISCONNECT, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickDisconnect));
	sizer_4->Add(button_2, 0, 0, 0);
	*/

	notebook_1_Logactivity = new wxPanel(notebook_1, wxID_ANY);
	notebook_1->AddPage(notebook_1_Logactivity, wxT("Log activity"));
	wxBoxSizer *sizer_1 = new wxBoxSizer(wxVERTICAL);
	text_ctrl_1 = new wxTextCtrl(notebook_1_Logactivity, wxID_ANY,
		wxEmptyString, wxDefaultPosition, wxSize(500,300), wxTE_MULTILINE);
	sizer_1->Add(text_ctrl_1, 0, wxEXPAND|wxSHAPED, 0);

	/* TODO (we don't need it for now)
	notebook_1_General = new wxPanel(notebook_1, wxID_ANY);
	notebook_1->AddPage(notebook_1_General, wxT("General"));
	*/

	notebook_1_Logactivity->SetSizer(sizer_1);
	notebook_1_pane_1->SetSizer(sizer_2);

	stray = new MyTaskBarIcon();
#ifdef WIN32
	stray->SetIcon(wxICON(AppIcon), wxString::FromUTF8("netvfy-agent"));
#else
	stray->SetIcon(wxICON(nvagent_ico), wxString::FromUTF8("netvfy-agent"));
#endif

	this->Connect(this->GetId(), wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MyFrame::onClose));

	Layout();
}

class MyApp: public wxApp {
public:
	bool OnInit();
};

IMPLEMENT_APP(MyApp)

void MyFrame::onClickConnect(wxCommandEvent &event)
{
	wxString	 wstr;
	int		 id;
	const char	*network;

	static_text_1->SetLabel("Connecting...");
	id = this->list_box_1->GetSelection();
	/* If nothing is selected yet, just do nothing */
	if (id == -1)
		return;

	wstr = this->list_box_1->GetString(id);
	network = wstr.mb_str(wxConvUTF8);
	agent_thread_start(network);
}

void MyFrame::onClickAddNetwork(wxCommandEvent &event)
{
	wxString	 	 provkey;
	wxString		 network;
	wxTextEntryDialog	 myDialog1(this, "Enter provisioning key:", "Add new network", "");
	wxTextEntryDialog	 myDialog2(this, "Choose the network name:", "Add new network", "");

	if (myDialog1.ShowModal() == wxID_OK) {
		provkey = myDialog1.GetValue();
	} else {
		return;
	}

	if (myDialog2.ShowModal() == wxID_OK) {
		network = myDialog2.GetValue();
	} else {
		return;
	}

	/* Strip " " around the provisioning key */
	provkey.Replace("\"", "", true);

	ndb_provisioning(provkey.mb_str(wxConvUTF8), network.mb_str(wxConvUTF8));

	/* Update the list of network */
	this->list_box_1->Clear();
	ndb_networks(this->onListNetworks);
}

void MyFrame::onClickDeleteNetwork(wxCommandEvent &event)
{
	wxString	 wstr;
	int		 id;
	const char	*network;

	id = this->list_box_1->GetSelection();
	/* If nothing is selected yet, just do nothing */
	if (id == -1)
		return;

	wstr = this->list_box_1->GetString(id);
	network = wstr.mb_str(wxConvUTF8);

	wxMessageDialog *myDialog1 = new wxMessageDialog(NULL,
		network, wxT("Are you sure you want to delete this network ?"),
		wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION);
	if (myDialog1->ShowModal() != wxID_YES) {
		return;
	}

	ndb_network_remove(network);

	/* Update the list of network */
	this->list_box_1->Clear();
	ndb_networks(this->onListNetworks);
}

void MyFrame::onClickExit(wxCommandEvent &event)
{
	wxMessageDialog	*myDialog = new wxMessageDialog(NULL,
		wxT("Are you sure you want to exit ?"), wxT("Netvfy-Agent"),
		wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION);
	if (myDialog->ShowModal() != wxID_YES) {
		return;
	}

	stray->Destroy();
	wxWindow::Destroy();
}

void MyFrame::onClickDisconnect(wxCommandEvent &event)
{
	this->static_text_1->SetLabel("Disconnecting...");
	this->static_text_2->SetLabel("");

	agent_thread_fini();

	this->button_1->Enable(true);
	this->button_1_b->Enable(false);

	this->list_box_1->Enable(true);
	this->button_2->Enable(true);
	this->button_3->Enable(true);

	this->static_text_1->SetLabel("Disconnected.");
}


void MyFrame::onClose(wxCloseEvent &event)
{
	if (systray_state) {
		this->Hide();
		systray_state = false;
	} else {
		this->Show();
		systray_state = true;
	}

	event.Veto();
}

MyFrame *frame;

wxBEGIN_EVENT_TABLE(MyTaskBarIcon, wxTaskBarIcon)
	EVT_TASKBAR_LEFT_DOWN (MyTaskBarIcon::onLeftButtonClick)
wxEND_EVENT_TABLE()

void MyTaskBarIcon::onLeftButtonClick(wxTaskBarIconEvent &event)
{
	if (systray_state) {
		frame->Hide();
		systray_state = false;
	} else {
		frame->Show();
		systray_state = true;
	}
}

wxMenu *MyTaskBarIcon::CreatePopupMenu()
{
	if (systray_state) {
		frame->Hide();
		systray_state = false;
	} else {
		frame->Show();
		systray_state = true;
	}

	return NULL;
}

/* Here we use a trampoline technique to communicate events (onConnect, onDisconnect, onLog)
 * between the backend and the GUI interface. Also, by using CallAfter(), the widgets are updated
 * from the GUI main thread instead of the thread of backend.
 */
void MyFrame::updateConnect(wxString ip)
{
	frame->static_text_1->SetLabel("Now Connected");
	frame->static_text_2->SetLabel(ip);

	frame->button_1->Enable(false);
	frame->button_1_b->Enable(true);

	frame->list_box_1->Enable(false);
	frame->button_2->Enable(false);
	frame->button_3->Enable(false);
}
void MyFrame::onConnect(const char *ip)
{
	frame->CallAfter(&MyFrame::updateConnect, wxString::FromUTF8(ip));
}

void MyFrame::updateDisconnect()
{
	frame->static_text_2->SetLabel("");
}

void MyFrame::onDisconnect()
{
	frame->CallAfter(&MyFrame::updateDisconnect);
}

void MyFrame::updateLog(wxString logline)
{
	frame->text_ctrl_1->AppendText(logline);
}

void MyFrame::onLog(const char *logline)
{
	frame->CallAfter(&MyFrame::updateLog, wxString::FromUTF8(logline));
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
