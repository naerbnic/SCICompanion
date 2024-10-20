/***************************************************************************
    Copyright (c) 2015 Philip Fortier

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
***************************************************************************/

//
// General stuff 
//
//
#pragma once

#include "EnumFlags.h"
#include "SciConstants.h"
#include "ScriptId.h"
#include "ResourceTypes.h"
#include "LineCol.h"

#ifdef DEBUG
#define DOCSUPPORT
#endif

// Used to color and identify tabs.
enum MDITabType
{
    TAB_NONE           =  0x00000000,
    TAB_GAME           =  0x00000001,   // The game explorer
    TAB_VIEW           =  0x00000002,
    TAB_PIC            =  0x00000004,
    TAB_SCRIPT         =  0x00000008,
    TAB_VOCAB          =  0x00000010,
    TAB_FONT           =  0x00000020,
    TAB_CURSOR         =  0x00000040,
    TAB_TEXT           =  0x00000080,   // Shared with message
    TAB_SOUND          =  0x00000100,
    TAB_ROOMEXPLORER   =  0x00000200,
    TAB_PALETTE        =  0x00000400,
    TAB_MESSAGE        =  0x00000800
};

static const int NumResourceTypesSCI0 = 10;

// Map ID_SHOW_VIEWS to ResourceType::View, for example.  -1 if no match.
ResourceType ResourceCommandToType(UINT nId);

BOOL IsValidResourceType(int iResourceType);
BOOL IsValidResourceNumber(int iResourceNum);

std::string GetGameIniFileName(const std::string &gameFolder);

const TCHAR *g_rgszTypeToSectionName[];
struct sPOINT
{
    __int16 x;
    __int16 y;
};

bool DirectoryExists(LPCTSTR szPath);
void AdvancePastWhitespace(const std::string &line, size_t &offset);

#define DEFAULT_PIC_WIDTH      320
#define DEFAULT_PIC_HEIGHT     190  
#define sPIC_WIDTH_MAX          320
#define sPIC_HEIGHT_MAX         200

// 320 x 190 pixels, each a byte. (For our drawing buffers)
#define BMPSIZE (DEFAULT_PIC_WIDTH * DEFAULT_PIC_HEIGHT)
#define BMPSIZE_MAX (sPIC_WIDTH_MAX * sPIC_HEIGHT_MAX)

struct EGACOLOR
{
    // Color2 comes before color 1, as it occupies the low bits.
    BYTE color2:4;      // 0000 xxxx
    BYTE color1:4;      // xxxx 0000

    BYTE ToByte() const
    {
        return color2 | (color1 << 4);
    }
};

#define EGACOLOR_TO_BYTE(x) ((x).color2 | ((x).color1 << 4))
#define EGACOLOR_EQUAL(a, b) (EGACOLOR_TO_BYTE(a) == EGACOLOR_TO_BYTE(b))

TCHAR g_szGdiplusFilter[];

extern RGBQUAD g_egaColors[16];
extern RGBQUAD g_egaColorsPlusOne[17];
extern RGBQUAD g_egaColorsExtended[256];	// 256, 16 repeated 16 times.
extern RGBQUAD g_egaColorsMixed[256];		// 256, 16x16 mixed (136 unique colors)
extern RGBQUAD g_continuousPriorityColors[256];
extern COLORREF g_egaColorsCR[16];
#define EGA_TO_RGBQUAD(x)  g_egaColors[(x)]
#define EGA_TO_COLORREF(x) g_egaColorsCR[(x)]
RGBQUAD _Combine(RGBQUAD color1, RGBQUAD color2);
RGBQUAD _Darker(RGBQUAD color);
RGBQUAD _Lighter(RGBQUAD color);
RGBQUAD _RGBQuadFromColorRef(COLORREF color);
COLORREF _ColorRefFromRGBQuad(RGBQUAD color);
RGBQUAD EgaColorToRGBQuad(EGACOLOR ega);
extern const int VocabKernelNames;

EGACOLOR g_egaColorChooserPalette[];

EGACOLOR EGAColorFromByte(BYTE b);

int GetColorDistance(COLORREF color1, COLORREF color2);
int GetColorDistanceRGB(RGBQUAD color1, RGBQUAD color2);
EGACOLOR GetClosestEGAColor(int iAlgorithm, bool gammaCorrected, int iPalette, COLORREF color);
EGACOLOR GetClosestEGAColorFromSet(int iAlgorithm, bool gammaCorrected, COLORREF color, EGACOLOR *rgColors, int cColors);

// EGA:
#define PALETTE_SIZE 40
#define GET_PALDEX(s)               ((s)/PALETTE_SIZE)
#define GET_PALCOL(s)               ((s)%PALETTE_SIZE)

// TODO: use for logging errors
#define REPORTERROR(x) 

// These values are important.  Pattern codes look like this:
// 87654321
// bits 6 an 5 are the flags below. Bits 3, 2 and 1 are the pattern size.
#define PATTERN_FLAG_RECTANGLE 0x10
#define PATTERN_FLAG_USE_PATTERN 0x20

#define RECTWIDTH(rc) ((rc).right - (rc).left)
#define RECTHEIGHT(rc) ((rc).bottom - (rc).top)

//
// BITMAPINFO for standard SCI0 raster resources.  We use 8bit
// even though we may only use 16 (or less) of the 256 colours.
//
struct SCIBitmapInfo : public BITMAPINFO
{
    SCIBitmapInfo() {}
    SCIBitmapInfo(int cx, int cy, const RGBQUAD *pPalette = nullptr, int count = 0);
    RGBQUAD _colors[256];
};

// Has an extended palette with duplicate colours, so we can hide information
// within.
struct SCIBitmapInfoEx : public BITMAPINFO
{
    SCIBitmapInfoEx(int cx, int cy);
    RGBQUAD _colors[256];
};

class ViewPort;
BYTE PriorityFromY(WORD y, const ViewPort &picState);

// The actual width of a cel, used in the bitmaps (multiple of 32 bits)
#define CX_ACTUAL(cx) (((cx) + 3) / 4 * 4)

void DisplayFileError(HRESULT hr, BOOL fOpen, LPCTSTR pszFileName);


struct GlobalAllocGuard
{
    GlobalAllocGuard(HGLOBAL hGlobal)
    {
        Global = hGlobal;
    }

    GlobalAllocGuard(UINT uFlags, SIZE_T dwBytes)
    {
        Global = GlobalAlloc(uFlags, dwBytes);
    }

    void Free()
    {
        if (Global)
        {
            GlobalFree(Global);
        }
        Global = nullptr;
    }

    HGLOBAL RelinquishOwnership()
    {
        HGLOBAL temp = Global;
        Global = nullptr;
        return temp;
    }

    ~GlobalAllocGuard()
    {
        Free();
    }

    HGLOBAL Global;
};

template<typename _T>
struct GlobalLockGuard
{
    GlobalLockGuard(HGLOBAL mem)
    {
        Object = reinterpret_cast<_T>(GlobalLock(mem));
    }

    GlobalLockGuard(GlobalAllocGuard &allocGuard)
    {
        Object = reinterpret_cast<_T>(GlobalLock(allocGuard.Global));
    }

    void Unlock()
    {
        if (Object)
        {
            GlobalUnlock(Object);
        }
        Object = nullptr;
    }

    ~GlobalLockGuard()
    {
        Unlock();
    }

    _T Object;
};


// Common combination of flags.
#define MB_ERRORFLAGS (MB_OK | MB_ICONEXCLAMATION | MB_APPLMODAL)

WORD _HexToWord(PCTSTR psz);

bool IsSCIKeyword(LangSyntax lang, const std::string &word);
bool IsTopLevelKeyword(LangSyntax lang, const std::string &word);
const std::vector<std::string> &GetTopLevelKeywords(LangSyntax lang);
bool IsCodeLevelKeyword(LangSyntax lang, const std::string &word);
bool IsClassLevelKeyword(LangSyntax lang, const std::string &word);
const std::vector<std::string> &GetCodeLevelKeywords(LangSyntax lang);
const std::vector<std::string> &GetClassLevelKeywords(LangSyntax lang);
const std::vector<std::string> &GetValueKeywords(LangSyntax lang);
bool IsValueKeyword(const std::string &word);
int string_to_int(const std::string &word);

extern const std::string SCILanguageMarker;

//
// equality operator for ScriptId
//
bool operator==(const ScriptId& script1, const ScriptId& script2);

bool operator<(const ScriptId& script1, const ScriptId& script2);


//
// Hash function for ScriptId
//
//template<> UINT AFXAPI HashKey<ScriptId&>( ScriptId& key );

#define PACKVERSION(major,minor) MAKELONG(minor,major)
DWORD GetDllVersion(LPCTSTR lpszDllName);

class CPrecisionTimer
{
  LARGE_INTEGER lFreq, lStart;

public:
  CPrecisionTimer()
  {
    QueryPerformanceFrequency(&lFreq);
  }

  inline void Start()
  {
    //SetThreadAffinityMask( GetCurrentThread(), 1 );
    QueryPerformanceCounter(&lStart);
  }
  
  inline double Stop()
  {
    // Return duration in seconds...
    LARGE_INTEGER lEnd;
    QueryPerformanceCounter(&lEnd);
    return (static_cast<double>(lEnd.QuadPart - lStart.QuadPart) / static_cast<double>(lFreq.QuadPart));
  }
};

const std::string MakeFile(PCSTR pszContent, const std::string &filename);
void ShowTextFile(PCSTR pszContent, const std::string &filename);
void ShowFile(const std::string &actualPath);
std::string MakeTextFile(PCSTR pszContent, const std::string &filename);
std::string GetBinaryDataVisualization(const uint8_t *data, size_t length, int columns = 16);

#define RGB_TO_COLORREF(x) RGB(x.rgbRed, x.rgbGreen, x.rgbBlue)

void throw_if(bool value, const char *message);

class IClassBrowserEvents
{
public:
    enum BrowseInfoStatus
    {
        Ok,
        Errors,
        InProgress
    };
    virtual void NotifyClassBrowserStatus(BrowseInfoStatus status, int iPercent) = 0;
};

class ResourceBlob;
BOOL OpenResource(const ResourceBlob *pData, bool setModifier = false);
int ResourceNumberFromFileName(PCTSTR pszFileName);
void testopenforwrite(const std::string &filename);
uint32_t GetResourceOffsetInFile(uint8_t secondHeaderByte);
extern const TCHAR g_szResourceSpec[];
extern const TCHAR* g_szResourceSpecByType[static_cast<std::size_t>(ResourceType::Max)];
void ToUpper(std::string &aString);
bool IsCodeFile(const std::string &text);

class ResourceEntity;
struct Cel;
const Cel &GetCel(const ResourceEntity *pvr, int &nLoop, int &nCel);

enum class PicScreen
{
    Visual = 0,
    Priority = 1,
    Control = 2,
    Aux = 3,
};

enum class PicScreenFlags
{
    None = 0x0000,
    Visual = 0x0001,
    Priority = 0x0002,
    Control = 0x0004,
    Aux = 0x0008,
    All = 0x0007,
    VGA = 0x0080,       // A hack, to pass information through to our draw functions that we should write vga
};
DEFINE_ENUM_FLAGS(PicScreenFlags, uint32_t)

bool CopyFilesOver(HWND hwnd, const std::string &from, const std::string &to);
bool DeleteDirectory(HWND hwnd, const std::string &folder);
std::string GetRandomTempFolder();
bool EnsureFolderExists(const std::string &folderName, bool throwException = true);

enum class OutputPaneType
{
    Compile = 0,
    Find = 1,
    Debug = 2
};

bool TerminateProcessTree(HANDLE hProcess, DWORD retCode);
