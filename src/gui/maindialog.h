#ifndef WXGLADE_OUT_H
#define WXGLADE_OUT_H

#include <wx/wx.h>
#include <wx/image.h>

#include <wx/notebook.h>
#include <wx/statline.h>

class MyFrame: public wxFrame {
public:
	MyFrame(wxWindow *parent, wxWindowID id, const wxString &title,
		const wxPoint &pos=wxDefaultPosition, const wxSize &size=wxDefaultSize,
		long style=wxDEFAULT_FRAME_STYLE);

	/* Interface between C backend and WX GUI */
	static void onListNetworks(const char *);
	static void onConnect(const char *);
	static void onDisconnect();
	static void onLog(const char *);

private:
	void OnClickConnect(wxCommandEvent &event);
	void UpdateLog(wxString logline);
protected:
	wxNotebook	*notebook_1;
	wxPanel		*notebook_1_pane_1;
	wxListBox	*list_box_1;
	wxButton	*button_1;
	wxButton	*button_2;
	wxPanel		*notebook_1_Logactivity;
	wxTextCtrl	*text_ctrl_1;
	wxPanel		*notebook_1_General;
};

#endif
