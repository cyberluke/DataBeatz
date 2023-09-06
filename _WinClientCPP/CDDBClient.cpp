#include "stdafx.h"
#include "CDDBClient.h"
#include <windows.h>
#include <comdef.h>
#include <winioctl.h>
#include <winsock.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <wmp.h> // WMP 9 SDK header
#include "sha1.h"
#include "base64.h"

#define XA_INTERVAL		((60 + 90 + 2) * 75)
#define DATA_TRACK		0x04

CDDBClient::CDDBClient() {
    // Initialization code if needed.
}

std::string CDDBClient::processCD(const std::string& drivePath) {
	// Step 1: Read TOC and Calculate CDDB DiscID
	std::string fullPath = "\\\\.\\" + drivePath;
	std::cout << "CD fullPath: " << fullPath << std::endl;
    HANDLE hDevice = CreateFile(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cout << "Failed to open CD-ROM drive " << drivePath << std::endl;
        return "";
    }

    CDROM_TOC toc;
    DWORD dwBytesReturned;

    if (!DeviceIoControl(hDevice, IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof(toc), &dwBytesReturned, NULL)) {
        std::cout << "Failed to read TOC." << std::endl;
        CloseHandle(hDevice);
        return "";
    }

	std::string discID = calculateMBID(toc);
	std::cout << "DiscID: " << discID << std::endl;
    CloseHandle(hDevice);

    return discID;
}

static int AddressToSectors2(const UCHAR address[])
{
	return address[1] * 4500 + address[2] * 75 + address[3];
}

int CDDBClient::LoadMBIDTOC(const CDROM_TOC &toc, int offsets[])  {
	int first_audio_track, last_audio_track, i;

	if (toc.FirstTrack < 1) {
		std::cout << "invalid CD TOC - first track number must be 1 or higher" << std::endl;
		return 0;
	}

	if (toc.LastTrack < 1) {
		std::cout << "invalid CD TOC - last track number must be 99 or lower" << std::endl;
		return 0;
	}

	/* we can't just skip data tracks at the front
	 * releases are always expected to start with track 1 by MusicBrainz
	 */
	first_audio_track = toc.FirstTrack;
	last_audio_track = -1;
	/* scan the TOC for audio tracks */
	for (i = toc.FirstTrack; i <= toc.LastTrack; i++) {
		int control = toc.TrackData[i - 1].Control;
		if ( !(control & DATA_TRACK) ) {
			last_audio_track = i;
		}
	}

	if (last_audio_track < 0) {
		std::cout << "no actual audio tracks on disc: CDROM or DVD?" << std::endl;
		return 0;
	}

	/* get offsets for all found data tracks */
	for (i = first_audio_track; i <= last_audio_track; i++) {
		int offset = AddressToSectors2(toc.TrackData[i - 1].Address) - 150;
	    int control = toc.TrackData[i - 1].Control;
		if (offset > 0) {
			offsets[i] = offset + 150;
		} else {
			/* this seems to happen on "copy-protected" discs */
			offsets[i] = 150;
		}
	}

	/* Lead-out is stored after the last track */
	offsets[0] = AddressToSectors2(toc.TrackData[toc.LastTrack].Address) - 150;
	int control0 = toc.TrackData[toc.LastTrack].Control;


	/* if the last audio track is not the last track on the CD,
	 * use the offset of the next data track as the "lead-out" offset */
	if (last_audio_track < toc.LastTrack) {
		int offset = offsets[last_audio_track + 1];
		offsets[0] = offset - XA_INTERVAL + 150;
	} else {
		/* use the regular lead-out track */
		offsets[0] = offsets[0] + 150;
	}

	/* as long as the lead-out isn't actually bigger than
	 * the position of the last track, the last track is invalid.
	 * This happens on "copy-protected"/invalid discs.
	 * The track is then neither a valid audio track, nor data track.
	 */
	while (offsets[0] < offsets[last_audio_track]) {
		last_audio_track = --last_audio_track;
		offsets[last_audio_track + 1] = 0;
		offsets[0] = offsets[last_audio_track + 1] - XA_INTERVAL + 150;
	}

	return 1;
}


std::string CDDBClient::calculateMBID(const CDROM_TOC &toc) {
	SHA_INFO	sha;
	unsigned char	digest[20], *base64;
	unsigned long	size;
	char		tmp[17]; /* for 8 hex digits (16 to avoid trouble) */

	sha_init(&sha);
	sprintf(tmp, "%02X", toc.FirstTrack);
	sha_update(&sha, (unsigned char *) tmp, strlen(tmp));
	sprintf(tmp, "%02X", toc.LastTrack);
	sha_update(&sha, (unsigned char *) tmp, strlen(tmp));

	int offsets[100];  // Declare a static array of 100 integers

    // Initialize or use the array
    for (int i = 0; i < 100; ++i) {
        offsets[i] = 0;
    }

	LoadMBIDTOC(toc, offsets);

	for (int i = 0; i < 100; i++) {
		sprintf(tmp, "%08X", offsets[i]);
		//std::cout << "Track TOC: " << offsets[i] << std::endl;
		sha_update(&sha, (unsigned char *) tmp, strlen(tmp));
	}

	sha_final(digest, &sha);

	base64 = rfc822_binary(digest, sizeof(digest), &size);
	std::cout << " TOC: " << base64 << std::endl;
	base64[size] = '\0';
	
	std::string result(reinterpret_cast<char*>(base64), 28);

	return result;
}



std::string CDDBClient::calculateCDDBDiscID(CDROM_TOC &toc) {
    DWORD t = 0;
    DWORD discID = 0;

    for (int i = toc.FirstTrack; i <= toc.LastTrack; i++) {
        DWORD minute = toc.TrackData[i - 1].Address[1];
        DWORD second = toc.TrackData[i - 1].Address[2];
        DWORD frame = toc.TrackData[i - 1].Address[3];
        
        DWORD trackOffset = (minute * 60 + second) * 75 + frame;
        
        while (trackOffset > 0) {
            t += (trackOffset % 10);
            trackOffset /= 10;
        }
    }

    DWORD minute = toc.TrackData[toc.LastTrack].Address[1];
    DWORD second = toc.TrackData[toc.LastTrack].Address[2];
    DWORD frame = toc.TrackData[toc.LastTrack].Address[3];
    DWORD leadOut = (minute * 60 + second) * 75 + frame;

    minute = toc.TrackData[0].Address[1];
    second = toc.TrackData[0].Address[2];
    frame = toc.TrackData[0].Address[3];
    DWORD firstTrack = (minute * 60 + second) * 75 + frame;

    discID = ((leadOut - firstTrack) << 24) | (t << 8) | (toc.LastTrack - toc.FirstTrack + 1);

	std::cout << "CDDB Disc ID: " << convertToCddbFormat(discID) << std::endl;
    return convertToCddbFormat(discID);
}

std::string CDDBClient::convertToCddbFormat(DWORD discID) {
	std::stringstream ss;
	ss << std::hex << std::setw(8) << std::setfill('0') << discID;

	return ss.str();
}

/**
[{"artist": "Artist1", "track": "Track1"}, {"artist": "Artist2", "track": "Track2"}]

*/
void CDDBClient::parseJson(const std::string& jsonStr, std::vector<std::string>& artistNames, std::vector<std::string>& trackNames) {
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;

	while ((pos = jsonStr.find('"', prev)) != std::string::npos) {
        std::string token = jsonStr.substr(prev, pos-prev);
        prev = pos+1;

        if (token == "artist" || token == "track") {
            pos = jsonStr.find('"', prev+1);
            prev = pos+1;
            pos = jsonStr.find('"', prev+1);
            std::string value = jsonStr.substr(prev, pos-prev);
            prev = pos+1;

            if (token == "artist") {
                artistNames.push_back(value);
            } else if (token == "track") {
                trackNames.push_back(value);
            }
        }
    }
}

std::string CDDBClient::BSTRToString(const BSTR bstr) {
    int wslen = SysStringLen(bstr);
    int len = WideCharToMultiByte(CP_ACP, 0, (wchar_t*)bstr, wslen, NULL, 0, NULL, NULL);
    std::string str(len, 0);
    WideCharToMultiByte(CP_ACP, 0, (wchar_t*)bstr, wslen, &str[0], len, NULL, NULL);
    return str;
}

void CDDBClient::updateWMPPlaylist(IWMPPlaylist *pPlaylist, const std::vector<std::string>& artistNames, const std::vector<std::string>& trackNames) {
	long count;
	HRESULT hr = pPlaylist->get_count(&count);
	for (long i = 0; i < count; ++i) {
        CComPtr<IWMPMedia> pMedia;
		std::cout << "get item" << std::endl;
        pPlaylist->get_item(i, &pMedia);

		std::cout << "put name" << std::endl;
		pMedia->put_name(_bstr_t(trackNames[i].c_str()));
		//pMedia->put_name(_bstr_t(artistNames[i].c_str()));
		std::cout << "put AlbumArtist" << std::endl;
		pMedia->setItemInfo(L"WM/AlbumArtist", _bstr_t(artistNames[i].c_str()));
        std::cout << "put Author" << std::endl;
		pMedia->setItemInfo(L"WM/Author", _bstr_t(artistNames[i].c_str()));
		pMedia->setItemInfo(L"Author", _bstr_t(artistNames[i].c_str()));
        pMedia->setItemInfo(L"WM/Genre", _bstr_t("test4"));
		pMedia->setItemInfo(L"Genre", _bstr_t("test5"));
    }
}
