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

#include "stdafx.h"

#include <absl/status/status.h>

#include "ResourceContainer.h"
#include "ResourceMap.h"
#include "ResourceRecency.h"
#include "View.h"
#include "Cursor.h"
#include "Font.h"
#include "Pic.h"
#include "Text.h"
#include "Sound.h"
#include "Vocab000.h"
#include "PaletteOperations.h"
#include "Message.h"
#include "Audio.h"
#include "AudioMap.h"
#include "ResourceEntity.h"
#include "CompiledScript.h"
#include "ResourceMapOperations.h"
#include "MessageHeaderFile.h"
#include "format.h"
#include "Logger.h"
#include "ResourceMapEvents.h"
#include "ResourceBlob.h"
#include "VersionDetectionHelper.h"
#include "BaseResourceUtil.h"
#include "ResourceUtil.h"
#include "BaseWindowsUtil.h"
#include "ResourceSourceImpls.h"

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

std::string ResourceDisplayNameFromType(ResourceType type);

HRESULT copyfile(const string &destination, const string &source)
{
    return CopyFile(destination.c_str(), source.c_str(), FALSE) ? S_OK : ResultFromLastError();
}

HRESULT SetFilePositionHelper(HANDLE hFile, DWORD dwPos)
{
    HRESULT hr;
    if (INVALID_SET_FILE_POINTER != SetFilePointer(hFile, dwPos, nullptr, FILE_BEGIN))
    {
        hr = S_OK;
    }
    else
    {
        hr = ResultFromLastError();
    }
    return hr;
}



HRESULT WriteResourceMapTerminatingBits(HANDLE hFileMap)
{
    HRESULT hr;
    // Write the terminating bits.
    DWORD cbWritten;
    RESOURCEMAPENTRY_SCI0 entryTerm;
    memset(&entryTerm, 0xff, sizeof(entryTerm));
    if (WriteFile(hFileMap, &entryTerm, sizeof(entryTerm), &cbWritten, nullptr) && (cbWritten == sizeof(entryTerm)))
    {
        hr = S_OK;
    }
    else
    {
        hr = ResultFromLastError();
    }
    return hr;
}

HRESULT TestForReadOnly(const string &filename)
{
    HRESULT hr = S_OK;
    DWORD dwAttribs = GetFileAttributes(filename.c_str());
    if (INVALID_FILE_ATTRIBUTES == dwAttribs)
    {
        hr = ResultFromLastError();
    }
    else if (dwAttribs & FILE_ATTRIBUTE_READONLY)
    {
        hr = HRESULT_FROM_WIN32(ERROR_FILE_READ_ONLY);
    }
    return hr;
}

HRESULT TestDelete(const string &filename)
{
    HRESULT hr = S_OK;
    HANDLE hFile = CreateFile(filename.c_str(), DELETE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
    }
    else
    {
        hr = ResultFromLastError();
    }
    return hr;
}

bool IsValidPackageNumber(int iPackageNumber)
{
    return (iPackageNumber >= 0) && (iPackageNumber < 63);
}

absl::Status RebuildResources(const std::shared_ptr<const GameFolderHelper> &helper, SCIVersion version, BOOL fShowUI, ResourceSaveLocation saveLocation, std::map<ResourceType, RebuildStats> &stats)
{
    try
    {
        // Do the audio stuff first, because it will end up adding new audio maps to the game's resources
        // (and RebuildResource should clean out the old ones)
        if (version.AudioVolumeName != AudioVolumeName::None)
        {
            std::unique_ptr<ResourceSource> resourceSource = CreateResourceSource(version, ResourceTypeFlags::All, helper, ResourceSourceFlags::AudioCache);
            resourceSource->RebuildResources(true, *resourceSource, stats);
        }

        // Enumerate resources and write the ones we have not already encountered.
        std::unique_ptr<ResourceSource> resourceSource = CreateResourceSource(version, ResourceTypeFlags::All, helper, ResourceSourceFlags::ResourceMap);
        ResourceSource *theActualSource = resourceSource.get();
        std::unique_ptr<ResourceSource> patchFileSource;
        if (saveLocation == ResourceSaveLocation::Patch)
        {
            // If this project saves to patch files by default, then we should use patch files as the source for rebuilding the resource package.
            patchFileSource = CreateResourceSource(version, ResourceTypeFlags::All, helper, ResourceSourceFlags::PatchFile);
            theActualSource = patchFileSource.get();
        }
        resourceSource->RebuildResources(true, *theActualSource, stats);

        if (version.MessageMapSource != MessageMapSource::Included)
        {
            ResourceSourceFlags sourceFlags = (version.MessageMapSource == MessageMapSource::MessageMap) ? ResourceSourceFlags::MessageMap : ResourceSourceFlags::AltMap;
            std::unique_ptr<ResourceSource> messageSource = CreateResourceSource(version, ResourceTypeFlags::All, helper, ResourceSourceFlags::MessageMap);
            messageSource->RebuildResources(true, *messageSource, stats);
        }

    }
    catch (std::exception &e)
    {
        return absl::InternalError(e.what());
    }
    return absl::OkStatus();
}

//
// CResourceMap
// Helper class for managing resources.
//
CResourceMap::CResourceMap(ISCIAppServices *appServices, ResourceRecency *resourceRecency) :
    _resourceRecency(resourceRecency), _appServices(appServices), _gameFolderHelper(GameFolderHelper::Create()), _version(sciVersion0)
{
    _paletteListNeedsUpdate = true;
    _skipVersionSniffOnce = false;
    _pVocab000 = nullptr;
    _cDeferAppend = 0;
    _gameFolderHelper->SetLanguage(LangSyntaxUnknown);
    _deferredResources.reserve(300);            // So we don't need to resize much it when adding
    _emptyPalette = std::make_unique<PaletteComponent>();
    memset(_emptyPalette->Colors, 0, sizeof(_emptyPalette->Colors));
}

CResourceMap::~CResourceMap()
{
    assert(_syncs.empty()); // They should remove themselves.
    assert(_cDeferAppend == 0);
}

//
// Adds the name of the resource to the game.ini file.
//
void CResourceMap::AssignName(const ResourceBlob &resource)
{
    // Assign the name of the item.
    std::string keyName = default_reskey(resource.GetResourceNum());
    std::string name = resource.GetName();
    if (!name.empty())
    {
        AssignName(resource.GetResourceId(), name.c_str());
    }
}

void CResourceMap::AssignName(const ResourceId& resource_id, PCTSTR pszName)
{
    // Assign the name of the item.
    std::string newValue;
    if (pszName)
    {
        newValue = pszName;
    }

    std::string keyName = default_reskey(resource_id.GetResourceNum());

    if (keyName != newValue)
    {
        Helper().SetIniString(g_resourceInfo[(int)resource_id.GetType()].pszTitleDefault, keyName, newValue);
    }
}

void CResourceMap::BeginDeferAppend()
{
    ASSERT((_cDeferAppend > 0) || _deferredResources.empty());
    ++_cDeferAppend;
}
void CResourceMap::AbandonAppend()
{
    // Abandon all the resources we were piling up.
    if (_cDeferAppend)
    {
        --_cDeferAppend;
        if (_cDeferAppend == 0)
        {
            _deferredResources.clear();
        }
    }
}
HRESULT CResourceMap::EndDeferAppend()
{
    HRESULT hr = S_OK;
    if (_cDeferAppend)
    {
        --_cDeferAppend;
        if (_cDeferAppend == 0)
        {
            if (!_deferredResources.empty())
            {
                // TODO: Assert that all defereed resources come from the same source.
                // Or rather we could support from multiple sources. The defer append only happens during
                // "compile all". Which actually might eventually support auto-generating message resources, so this is
                // probably something we'll need to handle. And now also audiomaps

                // Bucketize the resources by resourcesource and map context.
                std::map<uint64_t, std::vector<const ResourceBlob*>> keyToIndices;
                for (size_t i = 0; i < _deferredResources.size(); i++)
                {
                    uint32_t mapContext = (uint32_t)((_deferredResources[i].GetBase36() == NoBase36) ? -1 : _deferredResources[i].GetNumber());
                    uint64_t bucketKey = (uint64_t)_deferredResources[i].GetSourceFlags() | (((uint64_t)mapContext) << 32);
                    keyToIndices[bucketKey].push_back(&_deferredResources[i]);
                }

                for (auto &pair : keyToIndices)
                {
                    // For each bucket, create a resourcesource and save them.
                    std::vector<const ResourceBlob*> &blobsForThisSource = pair.second;
                    ResourceSourceFlags sourceFlags = blobsForThisSource[0]->GetSourceFlags();

                    int mapContext = (blobsForThisSource[0]->GetBase36() == NoBase36) ? -1 : blobsForThisSource[0]->GetNumber();
                    // Enumerate resources and write the ones we have not already encountered.
                    std::unique_ptr<ResourceSource> resourceSource = CreateResourceSource(_version, ResourceTypeFlags::All, _gameFolderHelper, sourceFlags, ResourceSourceAccessFlags::ReadWrite, mapContext);

                    try
                    {
                        // We're going to tell the relevant folks to reload completely, so we
                        // don't care about the return value here (replace or append)
                        resourceSource->AppendResources(blobsForThisSource);
                    }
                    catch (std::exception &e)
                    {
                        Logger::UserWarning("While saving resources: %s", e.what());
                    }
                }

                bool reload[NumResourceTypes] = {};
                for (ResourceBlob &blob : _deferredResources)
                {
                    AssignName(blob);
                    reload[(int)blob.GetType()] = true;
                }

                _deferredResources.clear();

                // Tell the views that might have changed, to reload:
                for (int iType = 0; iType < ARRAYSIZE(reload); iType++)
                {
                    if (reload[iType])
                    {
                        NotifyToReloadResourceType((ResourceType)iType);
                    }
                }
            }
        }
    }
    return hr;
}

//
// Suggest an unused resource number for this type.
//
int CResourceMap::SuggestResourceNumber(ResourceType type)
{
    int iNumber = 0;
    // Figure out a number to suggest...
    vector<bool> rgNumbers(1000, false);

    auto resourceContainer = Resources(ResourceTypeToFlag(type), Helper().GetDefaultEnumFlags() | ResourceEnumFlags::MostRecentOnly);
    for (auto &blobIt = resourceContainer->begin(); blobIt != resourceContainer->end(); ++blobIt)
    {
        int iThisNumber = blobIt.GetResourceNumber();
        if (iThisNumber >= 0 && iThisNumber < (int)rgNumbers.size())
        {
            rgNumbers[iThisNumber] = true;
        }
    }

    // Find the first one that is still false
    // (iterators on c style arrays are pointers to those arrays)
    auto result = find(rgNumbers.begin(), rgNumbers.end(), false);
    iNumber = (int)(result - rgNumbers.begin());
    return iNumber;
}

bool CResourceMap::IsResourceCompatible(const ResourceBlob &resource)
{
    return ::IsResourceCompatible(_version, resource);
}

void CResourceMap::PokeResourceMapReloaded()
{
    // Refresh everything.
    for_each(_syncs.begin(), _syncs.end(), [&](IResourceMapEvents *events) {
      events->OnResourceMapReloaded(false);
    });
}
 
void CResourceMap::RepackageAudio(bool force)
{
    // Rebuild any out-of-date audio resources. This should nearly be a no-op if none are out of data.
    // TODO: This could be slow, so provide some kind of UI feedback?
    if (GetSCIVersion().AudioVolumeName != AudioVolumeName::None)
    {
        std::map<ResourceType, RebuildStats> stats;
        std::unique_ptr<ResourceSource> resourceSource = CreateResourceSource(_version, ResourceTypeFlags::All, _gameFolderHelper, ResourceSourceFlags::AudioCache);
        resourceSource->RebuildResources(force, *resourceSource, stats);
    }
}

ResourceSaveLocation CResourceMap::GetDefaultResourceSaveLocation()
{
    return Helper().GetResourceSaveLocation(ResourceSaveLocation::Default);
}

HRESULT CResourceMap::AppendResource(const ResourceBlob &resource)
{
    HRESULT hr;
    if (_cDeferAppend)
    {
        // If resource appends are deferred, then just add it to a list.
        _deferredResources.push_back(resource);
        hr = S_OK;
    }
    else
    {
        if (_appServices && (resource.GetType() == ResourceType::View))
        {
            _appServices->SetRecentlyInteractedView(resource.GetNumber());
        }

        // Enumerate resources and write the ones we have not already encountered.
        int mapContext = (resource.GetBase36() == NoBase36) ? -1 : resource.GetNumber();
        std::unique_ptr<ResourceSource> resourceSource = CreateResourceSource(_version, ResourceTypeFlags::All, _gameFolderHelper, resource.GetSourceFlags(), ResourceSourceAccessFlags::ReadWrite, mapContext);
        std::vector<const ResourceBlob*> blobs;
        blobs.push_back(&resource);

        AppendBehavior appendBehavior = AppendBehavior::Append;
        hr = E_FAIL;
        try
        {
            appendBehavior = resourceSource->AppendResources(blobs);
            hr = S_OK;
        }
        catch (std::exception &e)
        {
            Logger::UserWarning("While saving resources: %s", e.what());
        }

        AssignName(resource);

        if (SUCCEEDED(hr))
        {
            if (resource.GetType() == ResourceType::Script)
            {
                // We'll need to re-gen this:
                _globalCompiledScriptLookups.reset(nullptr);
            }

            if (resource.GetType() == ResourceType::Palette)
            {
                _paletteListNeedsUpdate = true;
                if (resource.GetNumber() == 999)
                {
                    _pPalette999.reset(nullptr);
                }
            }

            // pResource is only valid for the length of this call.  Nonetheless, call our syncs
            for (auto &sync : _syncs)
            {
                sync->OnResourceAdded(&resource, appendBehavior);
            }
        }

#if 0
        if (FAILED(hr))
        {
            // Prepare error.
            TCHAR szError[MAX_PATH];
            szError[0] = 0;
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, HRESULT_CODE(hr), 0, szError, ARRAYSIZE(szError), nullptr);
            if (hr == E_ACCESSDENIED)
            {
                StringCchCat(szError, ARRAYSIZE(szError), TEXT("\nThe file may be in use. Maybe the game is running?"));
            }

            TCHAR szMessage[MAX_PATH];
            StringCchPrintf(szMessage, ARRAYSIZE(szMessage), TEXT("There was an error writing the resource: 0x%x\n%s"), hr, szError);
            Logger::UserError("There was an error writing the resource: 0x%x\n%s", hr, szError);
        }
#endif
    }
    return hr;
}

bool CResourceMap::AppendResource(const ResourceEntity &resource, int *pChecksum)
{
    return AppendResource(resource, resource.GetResourceLocation(), "", pChecksum);
}

bool ValidateResourceSize(const SCIVersion &version, DWORD cb, ResourceType type)
{
    // The maximum resource size increases with SCI versions, as the map and resource package header size/offset bit widths increased.
    // Audio is special, because it was stored with a custom map format.
    // There are probably actual values that are correct, but we'll use 64K for earlier SCI versions,
    // and a much larger value for later SCI versions. If we wanted to be "perfect", we could base it off the
    // bit-width of the map offsets/package resource size fields.
    bool fRet = IsValidResourceSize(version, cb, type);
    if (!fRet)
    {
        // uses MB_ERRORFLAGS
        Logger::UserError("Resources can't be bigger than %d bytes.  This resource is %d bytes.", MaxResourceSize, cb);
        fRet = false;
    }
    return fRet;
}


bool CResourceMap::AppendResource(const ResourceEntity& resource, const ResourceLocation& resource_location, const std::string& name, int* pChecksum)
{
    if (!resource.PerformChecks())
    {
        return false;
    }
    ResourceBlob data;
    sci::ostream serial;
    resource.WriteTo(serial, true, resource_location.GetNumber(), data.GetPropertyBag());
    if (!ValidateResourceSize(_version, serial.tellp(), resource.GetType()))
    {
        return false;
    }
    sci::istream readStream = istream_from_ostream(serial);
    ResourceSourceFlags sourceFlags = resource.SourceFlags;
    if (sourceFlags == ResourceSourceFlags::Invalid)
    {
        sourceFlags = (GetDefaultResourceSaveLocation() == ResourceSaveLocation::Patch) ? ResourceSourceFlags::PatchFile : ResourceSourceFlags::ResourceMap;
    }

    auto resourceName = Helper().FigureOutName(resource_location.GetResourceId());
    data.CreateFromBits(resourceName, resource_location, &readStream, _version, sourceFlags);
    if (!name.empty())
    {
        data.SetName(name.c_str());
    }

    auto success = SUCCEEDED(AppendResource(data));

    if (pChecksum)
    {
        *pChecksum = data.GetChecksum();
    }

    return success;
}

std::unique_ptr<ResourceContainer> CResourceMap::Resources(ResourceTypeFlags types, ResourceEnumFlags enumFlags, int mapContext)
{
    ResourceRecency *pRecency = nullptr;
    if (_resourceRecency && ((enumFlags & ResourceEnumFlags::CalculateRecency) != ResourceEnumFlags::None))
    {
        pRecency = _resourceRecency;
        for (int i = 0; i < NumResourceTypes; i++)
        {
            if ((int)types & (1 << i))
            {
                // This resource type is being re-enumerated.
                pRecency->ClearResourceType(i);
            }
        }
    }

    return Helper().Resources(_version, types, enumFlags, pRecency, mapContext);
}

bool CResourceMap::DoesResourceExist(const ResourceId& resource_id, std::string* retrieveName, ResourceSaveLocation location) const
{
    return Helper().DoesResourceExist(_version, resource_id, retrieveName, location);
}

std::unique_ptr<ResourceBlob> CResourceMap::MostRecentResource(const ResourceId& resource_id, bool getName, int mapContext) const
{
    ResourceEnumFlags flags = ResourceEnumFlags::AddInDefaultEnumFlags;
    if (getName)
    {
        flags |= ResourceEnumFlags::NameLookups;
    }
    return Helper().MostRecentResource(_version, resource_id, flags, mapContext);
}

void CResourceMap::_SniffSCIVersion()
{
    if (_skipVersionSniffOnce)
    {
        _skipVersionSniffOnce = false;
        return;
    }

    _version = SniffSCIVersion(*_gameFolderHelper);
}

void CResourceMap::NotifyToRegenerateImages()
{
    for_each(_syncs.begin(), _syncs.end(), [&](IResourceMapEvents *events) {
      events->OnImagesInvalidated();
    });
}

void CResourceMap::NotifyToReloadResourceType(ResourceType iType)
{
    for_each(_syncs.begin(), _syncs.end(), [&](IResourceMapEvents *events) {
      events->OnResourceTypeReloaded(iType);
    });

    if (iType == ResourceType::Palette)
    {
        _paletteListNeedsUpdate = true;
    }

    if (iType == ResourceType::Script)
    {
        // We'll need to re-gen this:
        _globalCompiledScriptLookups.reset(nullptr);
    }
}

//
// Add a listener for events.
//
void CResourceMap::AddSync(IResourceMapEvents *pSync)
{
    _syncs.push_back(pSync);
}

//
// Before your object gets destroyed, it MUST remove itself
//
void CResourceMap::RemoveSync(IResourceMapEvents *pSync)
{
    // We wouldn't expect to remove a sync that isn't there, so we can just erase
    // whatever we find (which should be a valid iterator)
    _syncs.erase(find(_syncs.begin(), _syncs.end(), pSync));
}

//
// Deletes the resource specified by pData, using the resource number, type and bits to compare.
//
void CResourceMap::DeleteResource(const ResourceBlob *pData)
{
    // Early bail out. Without palette 999, we'll mis-identify the SCI type, and things will be bad
    // (and also... we won't have a global palette)
    if ((pData->GetType() == ResourceType::Palette) && (pData->GetNumber() == 999))
    {
        Logger::UserError("Palette 999 is the global palette and cannot be deleted.");
        return;
    }

    try
    {
        ::DeleteResource(*this, *pData);
    }
    catch (std::exception &e)
    {
        Logger::UserWarning("While deleting resource: %s", e.what());
    }

    // Call our syncs, so they update.
    if (pData->GetType() == ResourceType::Script)
    {
        // We'll need to re-gen this:
        _globalCompiledScriptLookups.reset(nullptr);
    }

    for_each(_syncs.begin(), _syncs.end(), [&](IResourceMapEvents *events) {
      events->OnResourceDeleted(pData);
    });
    if (pData->GetType() == ResourceType::Palette)
    {
        _paletteListNeedsUpdate = true;
    }
}

std::string CResourceMap::GetGameFolder() const
{
    return _gameFolderHelper->GetGameFolder();
}

void CResourceMap::_SniffGameLanguage()
{
    if (_gameFolderHelper->GetLanguage() == LangSyntaxUnknown)
    {
        std::string languageValue = _gameFolderHelper->GetIniString(GameSection, LanguageKey);
        if (languageValue == "scp")
        {
            // We have left this turd in from old game.inis. We don't support cpp as a default game language,
            // so let's just convert it to Studio.
            _gameFolderHelper->SetLanguage(LangSyntaxStudio);
        }
        else if (languageValue == LanguageValueSCI)
        {
            _gameFolderHelper->SetLanguage(LangSyntaxSCI);
        }
        else if (languageValue == LanguageValueStudio)
        {
            _gameFolderHelper->SetLanguage(LangSyntaxStudio);
        }
        else
        {
            // Nothing specified. This could be an old fan game from Studio, or the first time someone
            // has opened this game in Companion. Let's look for a script to see if there is one with
            // Studio language. If so, we'll use that. Otherwise, we'll use SCI.
            // We'll explicitly set the language, so this doesn't happen again.
            std::string scriptZeroFilename = _gameFolderHelper->GetScriptFileName(0);
            auto lang = DetermineFileLanguage(scriptZeroFilename);
            if (lang == LangSyntaxStudio)
            {
                SetGameLanguage(LangSyntaxStudio);
            }
            else if (lang == LangSyntaxSCI)
            {
                SetGameLanguage(LangSyntaxSCI);
            }
        }
    }
}

//
// Gets the include folder that has read-only headers
//
std::string CResourceMap::GetIncludeFolder()
{
    std::string includeFolder = _includeFolderOverride;
    if (includeFolder.empty())
    {
        return _gameFolderHelper->GetIncludeFolder();
    }
    includeFolder += "include";
    return includeFolder;
}

//
// Gets the include folder that has read-only headers
//
std::string CResourceMap::GetTemplateFolder()
{
    return GetExeSubFolder("TemplateGame");
}

std::string CResourceMap::GetTopLevelSamplesFolder()
{
    return GetExeSubFolder("Samples");
}

//
// Gets the samples folder 
//
std::string CResourceMap::GetSamplesFolder()
{
    std::string folder = GetExeSubFolder("Samples");
    if (GetSCIVersion().MapFormat == ResourceMapFormat::SCI0)
    {
        folder += "\\SCI0";
    }
    else
    {
        folder += "\\SCI1.1";
    }
    return folder;
}


//
// Gets the objects folder 
//
std::string CResourceMap::GetObjectsFolder()
{
    std::string folder = GetExeSubFolder("Objects");
    if (GetSCIVersion().MapFormat == ResourceMapFormat::SCI0)
    {
        folder += "\\SCI0";
    }
    else
    {
        folder += "\\SCI1.1";
    }
    return folder;
}

std::string CResourceMap::GetDecompilerFolder()
{
    return GetExeSubFolder("Decompiler");
}

bool hasEnding(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else {
        return false;
    }
}

std::string CResourceMap::GetIncludePath(const std::string &includeFileName)
{
    if (hasEnding(includeFileName, ".shm"))
    {
        return Helper().GetMsgFolder() + "\\" + includeFileName;
    }
    else if (hasEnding(includeFileName, ".shp"))
    {
        return Helper().GetPolyFolder() + "\\" + includeFileName;
    }
    else
    {
        std::string includeFolder = GetIncludeFolder();
        if (!includeFolder.empty())
        {
            includeFolder += "\\";
            includeFolder += includeFileName;
            if (PathFileExists(includeFolder.c_str()))
            {
                return includeFolder;
            }
        }
        includeFolder = Helper().GetSrcFolder();
        includeFolder += "\\";
        includeFolder += includeFileName;
        if (PathFileExists(includeFolder.c_str()))
        {
            return includeFolder;
        }
        return "";
    }
}


HRESULT CResourceMap::GetGameIni(PTSTR pszBuf, size_t cchBuf)
{
    HRESULT hr = E_FAIL;
    if (!_gameFolderHelper->GetGameFolder().empty())
    {
        hr = StringCchPrintf(pszBuf, cchBuf, TEXT("%s\\%s"), _gameFolderHelper->GetGameFolder().c_str(), TEXT("game.ini"));
    }
    return hr;
}

const SCIVersion &CResourceMap::GetSCIVersion() const
{
    return _version;
}

void CResourceMap::SetVersion(const SCIVersion &version)
{
    _version = version;
}

// Someone on the forums started hitting problems once they had over 350 scripts. Our size was 5000.
// This allows 4x that. Should be high enough.
const DWORD c_IniSectionMaxSize = 20000;

HRESULT CResourceMap::GetScriptNumber(ScriptId script, WORD &wScript)
{
    wScript = 0xffff;
    TCHAR szGameIni[MAX_PATH];
    HRESULT hr = GetGameIni(szGameIni, ARRAYSIZE(szGameIni));
    if (SUCCEEDED(hr))
    {
        hr = E_INVALIDARG;
		DWORD cchBuf = c_IniSectionMaxSize;
		std::unique_ptr<char[]> szNameValues = std::make_unique<char[]>(cchBuf);

		DWORD nLength = GetPrivateProfileSection(TEXT("Script"), szNameValues.get(), cchBuf, szGameIni);
		if (nLength > 0 && ((cchBuf - 2) != nLength)) // returns (cchNameValues - 2) in case of insufficient buffer 
        {
            TCHAR *psz = szNameValues.get();
            while(*psz && (FAILED(hr)))
            {
                size_t cch = strlen(psz);
                char *pszEq = StrChr(psz, TEXT('='));
                CString strTitle = script.GetTitle().c_str();
                if (pszEq && (0 == strTitle.CompareNoCase(pszEq + 1)))
                {
                    // We have a match in script name... find the number
                    TCHAR *pszNumber = StrChr(psz, TEXT('n'));
                    if (pszNumber)
                    {
                        wScript = (WORD)StrToInt(pszNumber + 1);
                        ASSERT(script.GetResourceNumber() == InvalidResourceNumber ||
                               script.GetResourceNumber() == wScript);
                        hr = S_OK;
                    }
                }
                // Advance to next string.
                psz += (cch + 1);
            }
        }
    }

    if (FAILED(hr) && script.GetResourceNumber() != InvalidResourceNumber)
    {
        wScript = script.GetResourceNumber();
        hr = S_OK;
    }

    return hr;
}

const Vocab000 *CResourceMap::GetVocab000()
{
    ResourceEntity *pResource = GetVocabResourceToEdit();
    if (pResource)
    {
        return pResource->TryGetComponent<Vocab000>();
    }
    return nullptr;
}

std::unique_ptr<PaletteComponent> CResourceMap::GetPalette(int fallbackPaletteNumber)
{
    std::unique_ptr<PaletteComponent> paletteReturn;
    if (fallbackPaletteNumber == 999)
    {
        const PaletteComponent *pc = GetPalette999();
        if (pc)
        {
            paletteReturn = make_unique<PaletteComponent>(*pc);
        }
    }
    else
    {
        std::unique_ptr<ResourceEntity> paletteFallback = CreateResourceFromNumber(ResourceId::Create(ResourceType::Palette, fallbackPaletteNumber));
        if (paletteFallback && paletteFallback->TryGetComponent<PaletteComponent>())
        {
            paletteReturn = make_unique<PaletteComponent>(paletteFallback->GetComponent<PaletteComponent>());
        }
    }
    return paletteReturn;
}

std::unique_ptr<PaletteComponent> CResourceMap::GetMergedPalette(const ResourceEntity &resource, int fallbackPaletteNumber)
{
    assert((_version.ViewFormat != ViewFormat::EGA) || (_version.PicFormat != PicFormat::EGA));
    std::unique_ptr<PaletteComponent> paletteReturn;
    const PaletteComponent *paletteEmbedded = resource.TryGetComponent<PaletteComponent>();
    if (!paletteEmbedded)
    {
        paletteEmbedded = _emptyPalette.get();
    }

    // Clone the embedded palette first - REVIEW: make_unique arg forwarding doesn't work with copy constructor. No object copy happens.
    paletteReturn = make_unique<PaletteComponent>(*paletteEmbedded);
    if (fallbackPaletteNumber == 999)
    {
        paletteReturn->MergeFromOther(GetPalette999());
    }
    else
    {
        std::unique_ptr<ResourceEntity> paletteFallback = CreateResourceFromNumber(ResourceId::Create(ResourceType::Palette, fallbackPaletteNumber));
        if (paletteFallback)
        {
            paletteReturn->MergeFromOther(paletteFallback->TryGetComponent<PaletteComponent>());
        }
    }
    return paletteReturn;
}

void CResourceMap::SaveAudioMap65535(const AudioMapComponent &newAudioMap, int mapContext)
{
    // Best idea is to get the existing one, modify, then save. That way we don't need to figure out where it came from.
    int number = mapContext;
    if (mapContext == -1)
    {
        number = newAudioMap.Traits.MainAudioMapResourceNumber;
    }
    std::unique_ptr<ResourceEntity> entity = CreateResourceFromNumber(ResourceId::Create(ResourceType::AudioMap, number), mapContext);
    
    // Assign the new component to it.
    entity->RemoveComponent<AudioMapComponent>();
    entity->AddComponent<AudioMapComponent>(std::make_unique<AudioMapComponent>(newAudioMap));
    AppendResource(*entity);
}

const PaletteComponent *CResourceMap::GetPalette999()
{
    PaletteComponent *globalPalette = nullptr;
    if (_version.HasPalette)
    {
        if (!_pPalette999)
        {
            _pPalette999 = CreateResourceFromNumber(ResourceId::Create(ResourceType::Palette, 999));
        }
        if (_pPalette999)
        {
            globalPalette = _pPalette999->TryGetComponent<PaletteComponent>();
        }
    }
    return globalPalette;
}

std::vector<int> CResourceMap::GetPaletteList()
{
    if (_paletteListNeedsUpdate)
    {
        _paletteListNeedsUpdate = false;
        _paletteList.clear();
        auto paletteContainer = Resources(ResourceTypeFlags::Palette, Helper().GetDefaultEnumFlags() | ResourceEnumFlags::MostRecentOnly);
        bool has999 = false;
        for (auto it = paletteContainer->begin(); it != paletteContainer->end(); ++it)
        {
            if (it.GetResourceNumber() == 999)
            {
                has999 = true;
            }
            else
            {
                _paletteList.push_back(it.GetResourceNumber());
            }
        }

        // Sort and put 999 first.
        std::sort(_paletteList.begin(), _paletteList.end());
        if (has999)
        {
            _paletteList.insert(_paletteList.begin(), 999);
        }
    }
    return _paletteList;
}

GlobalCompiledScriptLookups *CResourceMap::GetCompiledScriptLookups()
{
    if (!_globalCompiledScriptLookups)
    {
        _globalCompiledScriptLookups = make_unique<GlobalCompiledScriptLookups>();
        if (!_globalCompiledScriptLookups->Load(_version, Helper().GetResourceLoader()))
        {
            // Warning... (happens in LB Dagger)
        }
    }
    return _globalCompiledScriptLookups.get();
}

ResourceEntity *CResourceMap::GetVocabResourceToEdit()
{
    if (!_pVocab000)
    {
        _pVocab000 = CreateResourceFromNumber(ResourceId::Create(ResourceType::Vocab, _version.MainVocabResource));
    }
    return _pVocab000.get();
}

void CResourceMap::ClearVocab000()
{
    _pVocab000.reset(nullptr);
}

HRESULT GetFilePositionHelper(HANDLE hFile, DWORD *pdwPos)
{
    HRESULT hr = S_OK;
    *pdwPos = SetFilePointer(hFile, 0, nullptr, FILE_CURRENT);
    if (*pdwPos == INVALID_SET_FILE_POINTER)
    {
        // Might have failed...
        hr = ResultFromLastError();
    }
    return hr;
}

std::vector<ScriptId> CResourceMap::GetAllScripts()
{
    std::vector<ScriptId> scripts;
    TCHAR szIniFile[MAX_PATH];
    if (SUCCEEDED(GetGameIni(szIniFile, ARRAYSIZE(szIniFile))))
    {
		DWORD cchBuf = c_IniSectionMaxSize;
		std::unique_ptr<char[]> szNameValues = std::make_unique<char[]>(cchBuf);
        DWORD nLength =  GetPrivateProfileSection(TEXT("Script"), szNameValues.get(), cchBuf, szIniFile);
        if (nLength > 0 && ((cchBuf - 2) != nLength)) // returns (cchBuf - 2) in case of insufficient buffer 
        {
            TCHAR *psz = szNameValues.get();
            while(*psz)
            {
                // The format is
                // n000=ScriptName
                size_t cch = strlen(psz);
                char *pszNumber = StrChr(psz, TEXT('n'));
                if (pszNumber)
                {
                    char *pszEq = StrChr(pszNumber, TEXT('='));
                    if (pszEq)
                    {
                        // Add this script...
                        auto scriptId = ScriptId::FromFullFileName(_gameFolderHelper->GetScriptFileName(pszEq + 1));
                        // Isolate the number.
                        *pszEq = 0;     // n123
                        pszNumber++;    // 123
                        scriptId.SetResourceNumber(StrToInt(pszNumber));
                        scripts.push_back(scriptId);
                    }
                }
                // Advance to next string.
                psz += (cch + 1);
            }
        }
    }
    return scripts;
}

void CResourceMap::GetNumberToNameMap(std::unordered_map<WORD, std::string> &scos)
{
    TCHAR szIniFile[MAX_PATH];
    if (SUCCEEDED(GetGameIni(szIniFile, ARRAYSIZE(szIniFile))))
    {
		DWORD cchBuf = c_IniSectionMaxSize;
		std::unique_ptr<char[]> szNameValues = std::make_unique<char[]>(cchBuf);
        DWORD nLength =  GetPrivateProfileSection(TEXT("Script"), szNameValues.get(), cchBuf, szIniFile);
        if (nLength > 0 && ((cchBuf - 2) != nLength)) // returns (cchBuf - 2) in case of insufficient buffer 
        {
            TCHAR *psz = szNameValues.get();
            while(*psz)
            {
                // The format is
                // n000=ScriptName
                size_t cch = strlen(psz);
                char *pszNumber = StrChr(psz, TEXT('n'));
                if (pszNumber)
                {
                    ++pszNumber; // Advance past the 'n'
                    char *pszEq = StrChr(pszNumber, TEXT('='));
                    if (pszEq)
                    {
                        scos[(WORD)StrToInt(pszNumber)] = pszEq + 1;
                    }
                }
                // Advance to next string.
                psz += (cch + 1);
            }
        }
    }
}

bool CResourceMap::CanSaveResourcesToMap()
{
    return true;    // Now supported for all.
}

MessageSource *CResourceMap::GetVerbsMessageSource(bool reload)
{
    if (!_verbsHeaderFile || reload)
    {
        string messageFilename = "Verbs.sh";
        string messageFilePath = fmt::format("{0}\\{1}", Helper().GetSrcFolder(), messageFilename);
        _verbsHeaderFile = make_unique<MessageHeaderFile>(messageFilePath, messageFilename, initializer_list<string>({ "VERBS" }));
    }
    return _verbsHeaderFile->GetMessageSource();
}

MessageSource *CResourceMap::GetTalkersMessageSource(bool reload)
{
    if (!_talkersHeaderFile || reload)
    {
        string messageFilename = "Talkers.sh";
        string messageFilePath = fmt::format("{0}\\{1}", Helper().GetSrcFolder(), messageFilename);
        _talkersHeaderFile = make_unique<MessageHeaderFile>(messageFilePath, messageFilename, initializer_list<string>({}));
    }
    return _talkersHeaderFile->GetMessageSource();
}

//
// Called when we open a new game.
//
void CResourceMap::SetGameFolder(const string &gameFolder)
{
    _gameFolderHelper->SetGameFolder(gameFolder);
    _talkerToView = TalkerToViewMap(Helper().GetLipSyncFolder());
    ClearVocab000();
    _pPalette999.reset(nullptr);                    // REVIEW: also do this if global palette is edited.
    _globalCompiledScriptLookups.reset(nullptr);
    _gameFolderHelper->SetLanguage(LangSyntaxUnknown);
    _talkersHeaderFile.reset(nullptr);
    _verbsHeaderFile.reset(nullptr);
    if (!gameFolder.empty())
    {
        try
        {
            _SniffGameLanguage();

            // We get here when we close documents.
            _SniffSCIVersion();

            // Send initial load notification
            for_each(_syncs.begin(), _syncs.end(), [](IResourceMapEvents *events) { events->OnResourceMapReloaded(true); });

            _paletteListNeedsUpdate = true;
        }
        catch (std::exception &e)
        {
            Logger::UserWarning("Unable to open resource map: %s", e.what());
            _gameFolderHelper->SetGameFolder("");
            throw e;
        }
    }

    if (_appServices)
    {
        _appServices->OnGameFolderUpdate();
    }
}

TalkerToViewMap &CResourceMap::GetTalkerToViewMap()
{
    return _talkerToView;
}

std::unique_ptr<ResourceEntity> CResourceMap::CreateResourceFromNumber(const ResourceId& resource_id, int mapContext ) const
{
    std::unique_ptr<ResourceEntity> pResource;
    unique_ptr<ResourceBlob> data = MostRecentResource(resource_id, mapContext);
    // This can legitimately fail. For instance, a script that hasn't yet been compiled.
    if (data)
    {
        pResource = CreateResourceFromResourceData(*data);
    }
    return pResource;
}

void DoNothing(ResourceEntity &resource) {}

std::unique_ptr<ResourceEntityFactory> CreateResourceEntityFactory(ResourceType res_type, const SCIVersion& version, const ResourceNum& resource_num)
{
    switch (res_type)
    {
    case ResourceType::View:
        return CreateViewResourceFactory();
    case ResourceType::Font:
        return CreateFontResourceFactory();
    case ResourceType::Cursor:
        return CreateCursorResourceFactory();
    case ResourceType::Text:
        return CreateTextResourceFactory();
    case ResourceType::Sound:
        return CreateSoundResourceFactory();
    case ResourceType::Vocab:
        return CreateVocabResourceFactory();
    case ResourceType::Pic:
        return CreatePicResourceFactory();
    case ResourceType::Palette:
        return CreatePaletteResourceFactory();
    case ResourceType::Message:
        return CreateMessageResourceFactory();
    case ResourceType::Audio:
        if (version.AudioIsWav && (!resource_num.GetBase36Opt().has_value()))
        {
            return CreateWaveAudioResourceFactory();
        }
        else
        {
            return CreateAudioResourceFactory();
        }
    case ResourceType::AudioMap:
        return CreateAudioMapResourceFactory();
    default:
        throw std::runtime_error("Unknown resource type");
    }
}

//
// Given a ResourceBlob, this creates the SCI resource represented by the data, and hands back
// a ResourceEntity.
// If there is an exception creating the resource, a default one is handed back.
//
std::unique_ptr<ResourceEntity> CreateResourceFromResourceData(const ResourceBlob &data, bool fallbackOnException)
{
    auto factory = CreateResourceEntityFactory(data.GetType(), data.GetVersion(), data.GetResourceNum());
    std::unique_ptr<ResourceEntity> pResourceReturn(factory->CreateResource(data.GetVersion()));
    try
    {
        pResourceReturn->InitFromResource(&data);
    }
    catch (std::exception)
    {
        if (!fallbackOnException)
        {
            throw;
        }

        data.AddStatusFlags(ResourceLoadStatusFlags::ResourceCreationFailed);
        pResourceReturn = factory->CreateDefaultResource(data.GetVersion());
        pResourceReturn->ResourceNumber = data.GetNumber();
        pResourceReturn->PackageNumber = data.GetPackageHint();
    }
    return pResourceReturn;
}

void CResourceMap::SetGameLanguage(LangSyntax lang)
{
    Helper().SetIniString(GameSection, LanguageKey, (lang == LangSyntaxSCI) ? LanguageValueSCI : LanguageValueStudio);
    _gameFolderHelper->SetLanguage(lang);
    _SniffGameLanguage();
}
