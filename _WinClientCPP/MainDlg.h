// MainDlg.h : Declaration of the CMainDlg
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//

#ifndef __MAINDLG_H_
#define __MAINDLG_H_

#include "resource.h"       // main symbols
#include <iostream>
#include <stdio.h>
#include <windows.h>
#include <winsock.h>
#include <comutil.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <atlbase.h>
#include <atlhost.h>
#include "wmp.h"
#include "wmpids.h"
#include "RemoteHost.h"
#include "CDDBClient.h"

static  _ATL_FUNC_INFO  PlayStateChangeInfo = { CC_STDCALL, VT_EMPTY, 1, {VT_I4} };

/////////////////////////////////////////////////////////////////////////////
// CMainDlg
class CMainDlg : 
    public IDispEventSimpleImpl<IDC_WMP, CMainDlg, &DIID__WMPOCXEvents>,
    public CAxDialogImpl<CMainDlg>
{
public:
    CMainDlg();
    ~CMainDlg();

    enum { IDD = IDD_MAINDLG };

BEGIN_MSG_MAP(CMainDlg)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    COMMAND_ID_HANDLER(IDC_OPENURL, OnOpenURL)
    COMMAND_ID_HANDLER(IDC_GOTOML, OnGoToML)
END_MSG_MAP()

BEGIN_SINK_MAP(CMainDlg)
    SINK_ENTRY_INFO(IDC_WMP, DIID__WMPOCXEvents, DISPID_WMPCOREEVENT_PLAYSTATECHANGE, OnPlayStateChange, &PlayStateChangeInfo)
END_SINK_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnOpenURL(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnGoToML(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    HRESULT STDMETHODCALLTYPE OnPlayStateChange(long NewState);
	bool SendHttpGetRequest(const std::string& author, const std::string& trackName, int trackNumber, int duration, int currentPosition);
	std::string BSTRToString(const BSTR bstr);
	std::string UrlEncode(const std::string &value);
	void queryCDDB();
	std::string sendHttpRequestCDDB(const std::string& discID);
	int getContentLength(const std::string& headers);
	std::string GetDriveLetterFromIndex(int index);
	static DWORD WINAPI ThreadWrapper(LPVOID lpParam);

private:
    CAxWindow                       *m_pView;
	std::string						discID;
	std::string						cdDrive;
	bool isProcessingCD;
	IStream*						pStream;
    CComPtr<IWMPPlayer4>            m_spPlayer; 
	CDDBClient						*cddbClient;
	WMPPlayState					m_previousState;
	HANDLE							g_hThread;  // Global variable to store the thread handle
};

#endif //__MAINDLG_H_
