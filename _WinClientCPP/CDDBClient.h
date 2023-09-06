#ifndef CDDBCLIENT_H
#define CDDBCLIENT_H

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <wmp.h>  // WMP 9 SDK header

#define MAXIMUM_NUMBER_TRACKS 100
#define RAW_SECTOR_SIZE			2352
#define CD_SECTOR_SIZE			2048
#define MAXIMUM_NUMBER_TRACKS	100
#define SECTORS_AT_READ			20
#define CD_BLOCKS_PER_SECOND	75
#define IOCTL_CDROM_RAW_READ	0x2403E
#define IOCTL_CDROM_READ_TOC	0x24000


typedef struct _TRACK_DATA {
  UCHAR Reserved;
  UCHAR Control : 4;
  UCHAR Adr : 4;
  UCHAR TrackNumber;
  UCHAR Reserved1;
  UCHAR Address[4];
} TRACK_DATA, *PTRACK_DATA;

typedef struct _CDROM_TOC {
  UCHAR Length[2];
  UCHAR FirstTrack;
  UCHAR LastTrack;
  TRACK_DATA TrackData[MAXIMUM_NUMBER_TRACKS];
} CDROM_TOC, *PCDROM_TOC;

typedef enum _TRACK_MODE_TYPE
{
	YellowMode2,
	XAForm2,
	CDDA
} TRACK_MODE_TYPE, *PTRACK_MODE_TYPE;

typedef struct __RAW_READ_INFO
{
	LARGE_INTEGER  DiskOffset;
	ULONG  SectorCount;
	TRACK_MODE_TYPE  TrackMode;
} RAW_READ_INFO, *PRAW_READ_INFO;



// Msf: Hours, Minutes, Seconds, Frames
ULONG AddressToSectors( UCHAR Addr[4] );


class CDDBClient {
public:
    CDDBClient();
	std::string BSTRToString(const BSTR bstr);
	std::string processCD(const std::string& drivePath);
    std::string calculateCDDBDiscID(CDROM_TOC &toc);
	std::string convertToCddbFormat(DWORD discID);
	std::string CDDBClient::calculateMBID(const CDROM_TOC &toc);
	int LoadMBIDTOC(const CDROM_TOC &toc, int offsets[]);
    void parseJson(const std::string& jsonStr, std::vector<std::string>& artistNames, std::vector<std::string>& trackNames);
    void updateWMPPlaylist(IWMPPlaylist *pPlaylist, const std::vector<std::string>& artistNames, const std::vector<std::string>& trackNames);
};

#endif // CDDBCLIENT_H
