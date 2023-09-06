// MainDlg.cpp : Implementation of CMainDlg
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//

#include "stdafx.h"
#include "MainDlg.h"
#include "OpenURLDlg.h"


#pragma comment(lib, "wsock32.lib")

// This constant is copied from wmp_i.c which can be generated from wmp.idl
const IID DIID__WMPOCXEvents = {0x6BF52A51,0x394A,0x11d3,{0xB1,0x53,0x00,0xC0,0x4F,0x79,0xFA,0xA6}};

/////////////////////////////////////////////////////////////////////////////
// CMainDlg

//***************************************************************************
// Constructor
//
//***************************************************************************
CMainDlg::CMainDlg()
{
    m_pView = NULL;
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	cddbClient = new CDDBClient();
	m_previousState = wmppsStopped;
	g_hThread = NULL;
	discID = "00000001";
	isProcessingCD = false;
	cdDrive = "E:";
	/*WSADATA wsaData;
	if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
		std::cout << "Failed to initialize WinSock" << std::endl;
	}*/
}

//***************************************************************************
// Destructor
//
//***************************************************************************
CMainDlg::~CMainDlg()
{
}

//***************************************************************************
// OnInitDialog()
// Initialize the dialog and create WMP OCX
//
//***************************************************************************
LRESULT CMainDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    HRESULT                             hr = S_OK;
    RECT                                rcClient;
    CComPtr<IObjectWithSite>            spHostObject;
    CComPtr<IAxWinHostWindow>           spHost;
    CComObject<CRemoteHost>             *pRemoteHost = NULL;


    // Create an ActiveX control container
    AtlAxWinInit();
    m_pView = new CAxWindow();  
    if(!m_pView)
    {
        hr = E_OUTOFMEMORY;
    }
    
    if(SUCCEEDED(hr))
    {
        ::GetWindowRect(GetDlgItem(IDC_RANGE), &rcClient);
        //ScreenToClient(&rcClient);
        m_pView->Create(m_hWnd, rcClient, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

        if(::IsWindow(m_pView->m_hWnd))
        {
            hr = m_pView->QueryHost(IID_IObjectWithSite, (void **)&spHostObject);
			if(!spHostObject.p)
			{
				hr = E_POINTER;
			}
        }
    }

    // Create remote host which implements IServiceProvider and IWMPRemoteMediaServices
    if(SUCCEEDED(hr))
    {
        hr = CComObject<CRemoteHost>::CreateInstance(&pRemoteHost);
        if(pRemoteHost)
        {
            pRemoteHost->AddRef();
        }
		else
		{
			hr = E_POINTER;
		}
    }

    // Set site to the remote host
    if(SUCCEEDED(hr))
    {
        hr = spHostObject->SetSite((IWMPRemoteMediaServices *)pRemoteHost);
    }

    if(SUCCEEDED(hr))
    {
        hr = m_pView->QueryHost(&spHost);
		if(!spHost.p)
		{
			hr = E_NOINTERFACE;
		}
    }

    // Create WMP Control here
    if(SUCCEEDED(hr))
    {
        hr = spHost->CreateControl(CComBSTR(L"{6BF52A52-394A-11d3-B153-00C04F79FAA6}"), m_pView->m_hWnd, NULL);
    }

    if(SUCCEEDED(hr))
    {
        hr = m_pView->QueryControl(&m_spPlayer);
		if(!m_spPlayer.p)
		{
			hr = E_NOINTERFACE;
		}
    }

    // Set skin to be custom skin
    if(SUCCEEDED(hr))
    {
        // Hook the event listener
        DispEventAdvise(m_spPlayer);
        // Put the UI mode to be a skin
        hr = m_spPlayer->put_uiMode(CComBSTR(_T("custom")));
    }

    // Release remote host object
    if(pRemoteHost)
    {
        pRemoteHost->Release();
    }
    return 1;  // Let the system set the focus
}

//***************************************************************************
// OnDestroy()
// Release WMP OCX and its container here
//
//***************************************************************************
LRESULT CMainDlg::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(m_spPlayer)
    {
        // Unhook the event listener
        DispEventUnadvise(m_spPlayer);
        m_spPlayer.Release();
    }
    if(m_pView != NULL)
    {
        delete m_pView;
    }

	CoUninitialize();
	CloseHandle(g_hThread);
	g_hThread = NULL;

    return 1;  // Let the system set the focus
}

//***************************************************************************
// OnCancel()
// When users click close button or press Esc, this function is called
// to close main dialog
//
//***************************************************************************
LRESULT CMainDlg::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    EndDialog(wID);
    return 0;
}

//***************************************************************************
// OnOpenURL()
// When users click OpenURL button, this function is called to open
// a dialog so that users can give URL to play
//
//***************************************************************************
LRESULT CMainDlg::OnOpenURL(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    COpenURLDlg dlg;
    if(IDOK == dlg.DoModal() && m_spPlayer.p)
    {
        m_spPlayer->put_URL(dlg.m_bstrURL);

        CComPtr<IWMPControls>   spControls;
        m_spPlayer->get_controls(&spControls);
        if(spControls.p)
        {
            spControls->play();
        }
    }
    return 0;
}

//***************************************************************************
// OnGoToML()
// When users click Go to Media Library button, this function is called to
// undock the player and go to Media Library pane
//
//***************************************************************************
LRESULT CMainDlg::OnGoToML(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    HRESULT                         hr = E_FAIL;
    CComPtr<IWMPPlayerApplication>  spPlayerApp;
    CComPtr<IWMPPlayerServices>     spPlayerServices;  

    if(m_spPlayer.p)
    {
        hr = m_spPlayer->QueryInterface(&spPlayerServices);
		if(!spPlayerServices.p)
		{
			hr = E_NOINTERFACE;
		}
    }

    if(SUCCEEDED(hr))
    {
        // Switch to media library pane
        spPlayerServices->setTaskPane(CComBSTR(_T("MediaLibrary")));
    }

    if(m_spPlayer.p)
    {
        hr = m_spPlayer->get_playerApplication(&spPlayerApp);
		if(!spPlayerApp.p)
		{
			hr = E_NOINTERFACE;
		}
    }

    if(SUCCEEDED(hr))
    {
        // Undock the player
        spPlayerApp->switchToPlayerApplication();
    }

    return 0;
}

std::string CMainDlg::BSTRToString(const BSTR bstr) {
    int wslen = SysStringLen(bstr);
    int len = WideCharToMultiByte(CP_ACP, 0, (wchar_t*)bstr, wslen, NULL, 0, NULL, NULL);
    std::string str(len, 0);
    WideCharToMultiByte(CP_ACP, 0, (wchar_t*)bstr, wslen, &str[0], len, NULL, NULL);
    return str;
}

// Static member function to act as a thread function
DWORD WINAPI CMainDlg::ThreadWrapper(LPVOID lpParam) {
    CMainDlg* pThis = (CMainDlg*)lpParam;
    pThis->queryCDDB();
	return 0;
}

void CMainDlg::queryCDDB() {
	CoInitialize(NULL);
	CComPtr<IWMPPlayer4> spPlayerWorker;
	CoGetInterfaceAndReleaseStream(pStream, __uuidof(IWMPPlayer4), (void**)&spPlayerWorker);

	//DWORD discID = cddbClient->processCD(driveLetter);
	std::string newDiscID = cddbClient->processCD(this->cdDrive);

	// Step 2: Send CDDB Data Over HTTP
	std::string httpResponse = sendHttpRequestCDDB(newDiscID);

	if (discID == newDiscID) {
		return;
	}

	discID = newDiscID;


	// Step 3: Parse the JSON Response
	std::vector<std::string> artistNames;
	std::vector<std::string> trackNames;
	cddbClient->parseJson(httpResponse, artistNames, trackNames);

	// Step 4: Update WMP 9 SDK Playlist
	CComPtr<IWMPPlaylist> spPlaylist;
	HRESULT hr = spPlayerWorker->get_currentPlaylist(&spPlaylist);
	std::cout << "Updating playlist items" << std::endl;
	cddbClient->updateWMPPlaylist(spPlaylist, artistNames, trackNames);
	if (SUCCEEDED(hr)) {
		std::cout << "Forcing playlist refresh" << std::endl;
		hr = spPlayerWorker->put_currentPlaylist(spPlaylist);
	}
	CoUninitialize();
}

std::string CMainDlg::GetDriveLetterFromIndex(int index)
{
    // Get the bitmask representing all drives in the system
    DWORD dwDrives = GetLogicalDrives();
    std::cout << "dwDrives " << dwDrives << std::endl;
    int nDriveIndex = 0;
	std::string result = "";
    for (int i = 0; i < 26; ++i)
    {
		//std::cout << "scanning hdd+cdd index: " << i << std::endl;
        // Check if the ith bit is set
        if (dwDrives & (1 << i))
        {
            // Check if the drive type is CD-ROM
			std::string drivePath = std::string(1, 'A' + i) + "://";
			std::cout << "drivePath " << drivePath << std::endl;
            UINT driveType = GetDriveType(drivePath.c_str());
            if (driveType == DRIVE_CDROM)
            {
				std::cout << "is CD: yes " << std::endl;
                // Check if this is the drive index we're interested in
                if (nDriveIndex == index)
                {
					std::cout << "found drive index " << i << std::endl;
					return result.append(1, 'A' + i);
                }
                ++nDriveIndex;
            }
        }
    }
    return "";
}

//***************************************************************************
// OnPlayStateChange()
// PlayStateChange event handler. When the player is undocked and user starts
// to play an item in it, this function docks the player
//
//***************************************************************************
HRESULT CMainDlg::OnPlayStateChange(long NewState)
{
	CoInitialize(NULL);

    HRESULT                         hr = E_FAIL;
    CComPtr<IWMPPlayerApplication>  spPlayerApp;
    VARIANT_BOOL                    bDocked;
	WMPPlayState newState = static_cast<WMPPlayState>(NewState);

	std::cout << "NewState: " << NewState << std::endl;
 
	if(m_spPlayer.p && (m_previousState == wmppsStopped || newState == wmppsStopped) && !isProcessingCD) {
		// m_previousState == wmppsStopped
		//  || newState == wmppsStopped
		
		CComPtr<IWMPMedia> spMedia;
		hr = m_spPlayer->get_currentMedia(&spMedia);
        // Get the controls interface
        CComPtr<IWMPControls> spControls;
        hr = m_spPlayer->get_controls(&spControls);

		isProcessingCD = true;
		spControls->stop();
		if(SUCCEEDED(hr) && spMedia)
        {
			// Get source URL
            CComBSTR bstrSourceURL;
            hr = spMedia->get_sourceURL(&bstrSourceURL);
            if (SUCCEEDED(hr))
            {
                // Convert BSTR to std::wstring
				std::string wsSourceURL = BSTRToString(bstrSourceURL);
				std::cout << wsSourceURL << std::endl;
                // Extract drive index (assuming the URL is in the format "wmpcd://2/1")
                if (wsSourceURL.find("wmpcd://") != std::string::npos)
                {
                    std::string driveIndexStr = wsSourceURL.substr(8, wsSourceURL.find('/') - 8);
					std::cout << "driveIndexStr: " << driveIndexStr << std::endl;
                    int driveIndex;
                    std::stringstream wss(driveIndexStr);
                    wss >> driveIndex;

                    // Map the drive index to an actual drive letter
					std::cout << "driveIndex: " << driveIndex << std::endl;
					this->cdDrive = GetDriveLetterFromIndex(driveIndex) + ":";
					if (this->cdDrive == ":") {
						this->cdDrive = "J:";
					}
                    std::cout << "Drive Letter: " << this->cdDrive << std::endl;
                }
            }
		} else {
			this->cdDrive = "J:";
			std::cout << "cannot get drive letter from current media, using default: " << this->cdDrive << std::endl;
		}

		std::cout << "queryCDDB started " << std::endl;

		CoMarshalInterThreadInterfaceInStream(__uuidof(IWMPPlayer4), m_spPlayer, &pStream);

		g_hThread = CreateThread(
            NULL,
            0,
			CMainDlg::ThreadWrapper,
            this,  // Pass this pointer for the thread function to use
            0,
            NULL
        );

		//spControls->play();
		isProcessingCD = false;
	}
	else if(m_spPlayer.p && (m_previousState == wmppsTransitioning && newState == wmppsPlaying)) {
		CComPtr<IWMPMedia> spMedia;
		CComPtr<IWMPControls> spControls;

		// Get current media and controls
		CComPtr<IWMPPlaylist> spPlaylist;
	    hr = m_spPlayer->get_currentPlaylist(&spPlaylist);
		hr = m_spPlayer->get_currentMedia(&spMedia);
		hr = m_spPlayer->get_controls(&spControls);

        if(SUCCEEDED(hr) && spMedia && spControls)
        {
		CComBSTR bstrName;
        double duration;
        double currentPosition;

        spMedia->get_duration(&duration);
        spControls->get_currentPosition(&currentPosition);

        // Convert BSTR to standard string
        spMedia->get_name(&bstrName);
        std::string trackName = BSTRToString(bstrName);
		CComBSTR authorBSTR("Author");
		CComBSTR authorValue;
        spMedia->getItemInfo(authorBSTR, &authorValue);
        std::string author = BSTRToString(authorValue);

        // For the sake of example, let's assume trackNumber is 1
        /*long trackNumber;
		CComBSTR trackNumberBSTR("WM/TrackNumber");
		CComBSTR trackNumberValue;
		if (SUCCEEDED(spMedia->getItemInfo(trackNumberBSTR, &trackNumberValue))) {
			trackNumber = _wtol(trackNumberValue);
		}*/

        // Duration and currentPosition are in seconds
        int dur = static_cast<int>(duration);
        int curPos = static_cast<int>(currentPosition);
		int trackNumber;

		long count = 0;
        spPlaylist->get_count(&count);

        for (long i = 0; i < count; ++i)
        {
            CComPtr<IWMPMedia> spMediaInPlaylist;
            hr = spPlaylist->get_item(i, &spMediaInPlaylist);

            if (SUCCEEDED(hr) && spMediaInPlaylist)
            {
                long attrCount;
                spMediaInPlaylist->get_attributeCount(&attrCount);

				VARIANT_BOOL isSame;
				spMedia->get_isIdentical(spMediaInPlaylist, &isSame);
				if (isSame == VARIANT_TRUE) {
					trackNumber = i + 1;
		            std::cout << "Current track is track #" << trackNumber << std::endl;
					m_previousState = newState;
					break;
				}
                
                std::cout << "------" << std::endl;
            }
        }


        // Send HTTP GET request
        SendHttpGetRequest(author, trackName, trackNumber, dur, curPos);

		}
	} else if(m_spPlayer.p && (NewState == wmppsPlaying))
    {
        // When playState is wmppsPlaying, try to dock the player
        hr = m_spPlayer->get_playerApplication(&spPlayerApp);
		if(!spPlayerApp.p)
		{
			hr = E_NOINTERFACE;
		}
    }

    /*if(SUCCEEDED(hr))
    {
        hr = spPlayerApp->get_playerDocked(&bDocked);
        // If the player is now in undocked state, dock it.
        if(SUCCEEDED(hr) && (bDocked == VARIANT_FALSE))
        {
            hr = spPlayerApp->switchToControl();
        }
    }*/

	m_previousState = newState;

    return S_OK;
}

std::string CMainDlg::UrlEncode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char) c);
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

// Function to get content length from headers
int CMainDlg::getContentLength(const std::string& headers) {
    size_t pos = headers.find("Content-Length: ");
    if (pos != std::string::npos) {
        size_t end_pos = headers.find("\r\n", pos);
        if (end_pos != std::string::npos) {
            std::string contentLengthStr = headers.substr(pos + 16, end_pos - (pos + 16));
            return atoi(contentLengthStr.c_str());
        }
    }
    return -1;
}

std::string CMainDlg::sendHttpRequestCDDB(const std::string& discID)
{
	std::cout << "sendHttpRequestCDDB" << std::endl;

	u_long iMode = 0;  // 0 for blocking, 1 for non-blocking

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
    {
		std::cout << "WSAStartup failed for winsock" << std::endl;
        return "";
    }

    SOCKET Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Set to blocking mode
	if (ioctlsocket(Socket, FIONBIO, &iMode) != NO_ERROR) {
		std::cout << "ioctlsocket failed to set blocking mode.\n";
		return "";
	}
	//u_long mode = 1;
	//ioctlsocket(Socket, FIONBIO, &mode);
    struct hostent *host = gethostbyname("192.168.1.244");

    SOCKADDR_IN SockAddr;
    SockAddr.sin_port = htons(5000);
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr);

    if (connect(Socket, (SOCKADDR*)(&SockAddr), sizeof(SockAddr)) == SOCKET_ERROR)
    {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			std::cout << "Failed to connect" << std::endl;
			closesocket(Socket);
			WSACleanup();
			return "";
		}
    }

	/*fd_set writeSet;
	FD_ZERO(&writeSet);
	FD_SET(Socket, &writeSet);

	timeval tv = {5 , 0 }; //5s timeout
	if (select(0, NULL, &writeSet, NULL, &tv) <= 0) {
		std::cout << "Connection timeout or error" << std::endl;
		closesocket(Socket);
		WSACleanup();
        return false;
	}*/

    std::stringstream ss;
    //ss << "GET /cddb?discid=" << cddbClient->convertToCddbFormat(discID) << " HTTP/1.1\r\nHost: localhost\r\n\r\n";
	ss << "GET /cddb?discid=" << discID << " HTTP/1.1\r\nHost: localhost\r\n\r\n";

    const std::string request = ss.str();
    send(Socket, request.c_str(), request.length(), 0);

	std::string headers;
    std::string body;
    char buffer[2048];
	int contentLength = 0;
	while (true) {
        int size = recv(Socket, buffer, sizeof(buffer) - 1, 0);
        if (size > 0) {
            buffer[size] = '\0';
            headers += buffer;

            size_t pos = headers.find("\r\n\r\n");
            if (pos != std::string::npos) {
                body = headers.substr(pos + 4);
                headers = headers.substr(0, pos + 4);

                contentLength = getContentLength(headers);
                if (contentLength >= 0) {
                    break;
                }
            }
        } else if (size == 0) {
            std::cout << "Connection closed.\n";
            break;
        } else {
            std::cout << "recv() failed.\n";
            break;
        }
    }

    // Read the remaining part of the body
    while (body.length() < static_cast<size_t>(contentLength)) {
        int size = recv(Socket, buffer, sizeof(buffer) - 1, 0);
        if (size > 0) {
            buffer[size] = '\0';
            body += buffer;
        } else if (size == 0) {
            std::cerr << "Connection closed.\n";
            break;
        } else {
            std::cerr << "recv() failed.\n";
            break;
        }
    }

   
    closesocket(Socket);
    WSACleanup();

	// Set back to non-blocking mode
	/*iMode = 1;
	if (ioctlsocket(Socket, FIONBIO, &iMode) != NO_ERROR) {
		std::cout << "ioctlsocket failed to set non-blocking mode.\n";
	}*/

    return body;

}
bool CMainDlg::SendHttpGetRequest(const std::string& author, const std::string& trackName, int trackNumber, int duration, int currentPosition)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
    {
		std::cout << "WSAStartup failed for winsock" << std::endl;
        return false;
    }

    SOCKET Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	u_long mode = 1;
	ioctlsocket(Socket, FIONBIO, &mode);
    struct hostent *host = gethostbyname("192.168.1.244");

    SOCKADDR_IN SockAddr;
    SockAddr.sin_port = htons(5000);
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr);

    if (connect(Socket, (SOCKADDR*)(&SockAddr), sizeof(SockAddr)) == SOCKET_ERROR)
    {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			std::cout << "Failed to connect" << std::endl;
			closesocket(Socket);
			WSACleanup();
			return false;
		}
    }

	/*fd_set writeSet;
	FD_ZERO(&writeSet);
	FD_SET(Socket, &writeSet);

	timeval tv = {5 , 0 }; //5s timeout
	if (select(0, NULL, &writeSet, NULL, &tv) <= 0) {
		std::cout << "Connection timeout or error" << std::endl;
		closesocket(Socket);
		WSACleanup();
        return false;
	}*/

    std::stringstream ss;
    ss << "GET /cdplay?trackName=" << UrlEncode(trackName)
       << "&author=" << UrlEncode(author)
       << "&trackNumber=" << trackNumber
       << "&duration=" << duration
       << "&currentPosition=" << currentPosition
       << "&discid=" << UrlEncode(discID)
       << " HTTP/1.1\r\nHost: localhost\r\n\r\n";

    const std::string request = ss.str();
    send(Socket, request.c_str(), request.length(), 0);

    char buffer[256];
    int size = recv(Socket, buffer, sizeof(buffer) - 1, 0);
    if (size > 0)
    {
        buffer[size] = '\0';
        std::cout << buffer << std::endl;
    }

    closesocket(Socket);
    WSACleanup();

    return true;
}


