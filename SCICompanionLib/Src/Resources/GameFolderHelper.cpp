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
#include "ResourceContainer.h"
#include "GameFolderHelper.h"
#include "ResourceBlob.h"
#include "ResourceMapOperations.h"
#include "format.h"
#include "ResourceEntity.h"
#include "AudioResourceSource.h"
#include "AudioCacheResourceSource.h"
#include "PatchResourceSource.h"
#include "AppState.h"

using namespace std;

const std::string GameSection = "Game";
const std::string LanguageKey = "Language";
const std::string LanguageValueStudio = "sc";
const std::string LanguageValueSCI = "sci";
const std::string AspectRatioKey = "UseSierraAspectRatio";
const std::string UnditherKey = "UnditherEGA";
const std::string PatchFileKey = "SaveToPatchFiles";
const std::string TrueValue = "true";
const std::string FalseValue = "false";
const std::string GenerateDebugInfoKey = "GenerateDebugInfo";

namespace
{
class FileGameConfigStore : public GameConfigStore
{
public:
    explicit
    FileGameConfigStore(std::string config_file_path) : config_file_path_(
        std::move(config_file_path))
    {
    }

    [[nodiscard]] std::string GetIniString(const std::string& sectionName,
                                           const std::string& keyName,
                                           std::string_view pszDefault)
    const override
    {
        std::string strRet;
        char sz[200];
        if (GetPrivateProfileString(sectionName.c_str(), keyName.c_str(),
                                    nullptr, sz, (DWORD)ARRAYSIZE(sz),
                                    config_file_path_.c_str()))
        {
            strRet = sz;
        }
        else
        {
            strRet = pszDefault;
        }
        return strRet;
    }

    [[nodiscard]] bool GetIniBool(const std::string& sectionName,
                                  const std::string& keyName,
                                  bool value) const override
    {
        return GetIniString(sectionName, keyName,
                            value ? TrueValue : FalseValue) == TrueValue;
    }

    void SetIniString(const std::string& sectionName,
                      const std::string& keyName,
                      const std::string& value) const override
    {
        WritePrivateProfileString(sectionName.c_str(), keyName.c_str(),
                                  value.empty() ? nullptr : value.c_str(),
                                  config_file_path_.c_str());
    }

    void SetIniBool(const std::string& sectionName,
                    const std::string& keyName, bool value) const override
    {
        SetIniString(sectionName, keyName, value ? TrueValue : FalseValue);
    }

    bool DoesSectionExistWithEntries(
        const std::string& sectionName) const override
    {
        char sz[200];
        return (GetPrivateProfileSection(sectionName.c_str(), sz,
                                         (DWORD)ARRAYSIZE(sz),
                                         config_file_path_.c_str()) > 0);
    }

    std::map<std::string, std::string> GetSectionEntries(
        const std::string& sectionName) const override
    {
        constexpr std::size_t INITIAL_SIZE = 32 * 1024;
        constexpr std::size_t MAX_SIZE = 1 * 1024 * 1024;

        auto curr_size = INITIAL_SIZE;

        std::unique_ptr<TCHAR[]> buffer;
        while (true)
        {
            if (curr_size > MAX_SIZE)
            {
                throw std::runtime_error("Section too large");
            }
            buffer = std::make_unique<char[]>(INITIAL_SIZE);
            auto read_size = GetPrivateProfileSection(
                sectionName.c_str(), buffer.get(), curr_size,
                config_file_path_.c_str());
            if (read_size < curr_size - 2)
            {
                break;
            }
            curr_size *= 2;
        }

        std::map<std::string, std::string> entries;

        const TCHAR* curr_ptr = buffer.get();
        const TCHAR* end_ptr = curr_ptr + curr_size;
        while (curr_ptr < end_ptr && *curr_ptr)
        {
            const auto* entry_end = static_cast<const TCHAR*>(std::memchr(curr_ptr, '\0', end_ptr - curr_ptr));
            if (!entry_end)
            {
                throw std::runtime_error("Invalid section buffer");
            }
            std::size_t entry_length = entry_end - curr_ptr;
            const auto* key_end = static_cast<const TCHAR*>(std::memchr(curr_ptr, '=', entry_end - curr_ptr));
            if (!key_end)
            {
                throw std::runtime_error("Could not find key in entry");
            }
            std::string_view key(curr_ptr, key_end - curr_ptr);
            std::string_view value(key_end + 1, entry_end - key_end - 1);
            entries.emplace(std::string(key), std::string(value));
            curr_ptr = entry_end + 1;
        }

        if (curr_ptr >= end_ptr)
        {
            throw std::runtime_error("Invalid section buffer");
        }

        return entries;
    }

private:
    std::string config_file_path_;
};
}

// Returns "n004" for input of 4
std::string default_reskey(int iNumber, uint32_t base36Number)
{
    if (base36Number == NoBase36)
    {
        return fmt::format("n{0:0>3}", iNumber);
    }
    else
    {
        // NNNNNVV.CCS
        return fmt::format("{0:0>3t}{1:0>2t}{2:0>2t}.{3:0>2t}{4:0>1t}",
                           iNumber,
                           (base36Number >> 8) & 0xff,
                           (base36Number >> 16) & 0xff,
                           (base36Number >> 0) & 0xff,
                           ((base36Number >> 24) & 0xff) % 36
                           // If this number (sequence) is 36, we want it to wrap 0 zero. Seq 0 not allowed.
        );
    }
}

std::unique_ptr<GameConfigStore> GameConfigStore::FromFilePath(
    std::string_view config_file)
{
    return std::make_unique<FileGameConfigStore>(std::string(config_file));
}

// GameConfig

GameConfig::GameConfig(const GameConfigStore* store) : store_(store)
{
}

void GameConfig::SetResourceEntry(ResourceType resource_type, int resource_number, uint32_t base36_number, const std::string& resource_name) const
{
    // Assign the name of the item.
    std::string keyName = default_reskey(resource_number, base36_number);
    if (!resource_name.empty() && (0 != lstrcmpi(keyName.c_str(), resource_name.c_str())))
    {
        store_->SetIniString(g_resourceInfo[(int)resource_type].pszTitleDefault, keyName, resource_name);
    }
}

void GameConfig::SetLanguage(LangSyntax lang) const
{
    store_->SetIniString(GameSection, LanguageKey, (lang == LangSyntaxSCI) ? LanguageValueSCI : LanguageValueStudio);
}

void GameConfig::SetUseSierraAspectRatio(bool use_sierra) const
{
    store_->SetIniBool(GameSection, AspectRatioKey, use_sierra);
}

void GameConfig::SetUndither(bool undither) const
{
    store_->SetIniBool(GameSection, UnditherKey, undither);
}

void GameConfig::SetResourceSaveLocation(
    ResourceSaveLocation location) const
{
    store_->SetIniBool(GameSection, PatchFileKey,
        location == ResourceSaveLocation::Patch);
}

// ResourceLoader

ResourceLoader::ResourceLoader(const GameFolderHelper* parent) : parent_(parent)
{
}

std::unique_ptr<ResourceContainer> ResourceLoader::Resources(
    const SCIVersion& version, ResourceTypeFlags types,
    ResourceEnumFlags enumFlags, ResourceRecency* pRecency,
    int mapContext) const
{
    if (IsFlagSet(enumFlags, ResourceEnumFlags::AddInDefaultEnumFlags))
    {
        enumFlags |= parent_->GetDefaultEnumFlags();
    }

    // If audio or sync resources are requested, we can't also have maps.
    if (IsFlagSet(types, ResourceTypeFlags::Audio))
    {
        ClearFlag(types, ResourceTypeFlags::AudioMap);
    }

    // Resources can come from various sources.
    std::unique_ptr<ResourceSourceArray> mapAndVolumes = std::make_unique<
        ResourceSourceArray>();

    if (!parent_->GetGameFolder().empty())
    {
        if (IsFlagSet(enumFlags, ResourceEnumFlags::IncludeCacheFiles))
        {
            // Our audio cache files take precedence
            if (IsFlagSet(types, ResourceTypeFlags::Audio))
            {
                mapAndVolumes->push_back(make_unique<AudioCacheResourceSource>(
                    &appState->GetResourceMap(), parent_->shared_from_this(),
                    mapContext, ResourceSourceAccessFlags::Read));
            }

            // Audiomaps can come from the cache files folder too... but we can re-use PatchFilesResourceSource for this
            if (IsFlagSet(types, ResourceTypeFlags::AudioMap))
            {
                mapAndVolumes->push_back(
                    std::make_unique<PatchFilesResourceSource>(
                        ResourceTypeFlags::AudioMap, version,
                        parent_->GetGameFolder() + pszAudioCacheFolder,
                        ResourceSourceFlags::AudioMapCache));
            }
        }

        // First, any stray files...
        if (!IsFlagSet(enumFlags, ResourceEnumFlags::ExcludePatchFiles) && (
            mapContext == -1))
        {
            mapAndVolumes->push_back(std::make_unique<PatchFilesResourceSource>(
                types, version, parent_->GetGameFolder(),
                ResourceSourceFlags::PatchFile));
        }

        // Add readers for message map files, if requested
        if (IsFlagSet(types, ResourceTypeFlags::Message) && !IsFlagSet(
            enumFlags,
            ResourceEnumFlags::ExcludePackagedFiles) && (mapContext == -1))
        {
            FileDescriptorBase* fd = nullptr;
            FileDescriptorMessageMap messageMap(parent_->GetGameFolder());
            FileDescriptorAltMap altMap(parent_->GetGameFolder());
            if (version.MessageMapSource == MessageMapSource::MessageMap)
            {
                fd = &messageMap;
            }
            else if (version.MessageMapSource == MessageMapSource::AltResMap)
            {
                fd = &altMap;
            }
            if (fd && fd->DoesMapExist())
            {
                mapAndVolumes->push_back(CreateResourceSource(
                    version, ResourceTypeFlags::Message,
                    parent_->shared_from_this(), fd->SourceFlags));
            }
        }

        if (IsFlagSet(types, ResourceTypeFlags::Audio) && !IsFlagSet(
            enumFlags, ResourceEnumFlags::ExcludePackagedFiles))
        {
            mapAndVolumes->push_back(make_unique<AudioResourceSource>(
                version, parent_->shared_from_this(), mapContext,
                ResourceSourceAccessFlags::Read));
        }

        // Now the standard resource maps
        if (!IsFlagSet(enumFlags, ResourceEnumFlags::ExcludePackagedFiles) && (
            mapContext == -1))
        {
            mapAndVolumes->push_back(CreateResourceSource(
                version, types, parent_->shared_from_this(),
                ResourceSourceFlags::ResourceMap));
        }
    }

    std::unique_ptr<ResourceContainer> resourceContainer(
        new ResourceContainer(
            parent_->GetGameFolder(),
            move(mapAndVolumes),
            types,
            enumFlags,
            pRecency)
    );

    return resourceContainer;
}

std::unique_ptr<ResourceBlob> ResourceLoader::MostRecentResource(
    const SCIVersion& version, ResourceType type, int number,
    ResourceEnumFlags flags, uint32_t base36Number, int mapContext) const
{
    std::unique_ptr<ResourceBlob> returnBlob;
    ResourceEnumFlags enumFlags = flags | ResourceEnumFlags::MostRecentOnly;
    auto resourceContainer = Resources(version, ResourceTypeToFlag(type),
                                       enumFlags, nullptr, mapContext);
    for (auto blobIt = resourceContainer->begin(); blobIt != resourceContainer->
         end(); ++blobIt)
    {
        if ((blobIt.GetResourceNumber() == number) && (blobIt.
            GetResourceHeader().Base36Number == base36Number))
        {
            returnBlob = move(*blobIt);
            break;
        }
    }
    return returnBlob;
}

bool ResourceLoader::DoesResourceExist(const SCIVersion& version,
                                       ResourceType type, int number,
                                       std::string* retrieveName,
                                       ResourceSaveLocation location) const
{
    ResourceEnumFlags enumFlags = (parent_->GetResourceSaveLocation(location) ==
                                      ResourceSaveLocation::Package)
                                      ? ResourceEnumFlags::ExcludePatchFiles
                                      : ResourceEnumFlags::ExcludePackagedFiles;

    if (retrieveName)
    {
        enumFlags |= ResourceEnumFlags::NameLookups;
    }
    auto resourceContainer = Resources(version, ResourceTypeToFlag(type),
                                       enumFlags);
    for (auto blobIt = resourceContainer->begin(); blobIt != resourceContainer->
         end(); ++blobIt)
    {
        if (blobIt.GetResourceNumber() == number)
        {
            if (retrieveName)
            {
                *retrieveName = (*blobIt)->GetName();
            }
            return true;
        }
    }
    return false;
}

// GameFolderHelper

std::shared_ptr<GameFolderHelper> GameFolderHelper::Create()
{
    return std::shared_ptr<GameFolderHelper>(new GameFolderHelper());
}

std::shared_ptr<GameFolderHelper> GameFolderHelper::Create(
    const std::string& game_folder)
{
    auto helper = std::shared_ptr<GameFolderHelper>(new GameFolderHelper());
    helper->SetGameFolder(game_folder);
    return helper;
}

GameFolderHelper::GameFolderHelper()
    : Language(LangSyntaxUnknown),
      resource_loader_(std::make_unique<ResourceLoader>(this))
{
}

std::string GameFolderHelper::GetScriptFileName(WORD wScript) const
{
    std::string filename;
    std::string scriptTitle = GetIniString("Script",
                                           default_reskey(wScript, NoBase36),
                                           default_reskey(wScript, NoBase36).
                                           c_str());
    if (!scriptTitle.empty())
    {
        filename = GetScriptFileName(scriptTitle);
    }
    return filename;
}

//
// Given something like "main", returns "c:\foobar\mygame\src\main.sc"
//
std::string GameFolderHelper::GetScriptFileName(const std::string& name) const
{
    string filename = this->GameFolder;
    filename += "\\src\\";
    filename += name;
    // Append the default extension
    filename += ".sc";
    return filename;
}

std::string GameFolderHelper::GetScriptObjectFileName(
    const std::string& title) const
{
    std::string filename = this->GameFolder;
    if (!filename.empty())
    {
        filename += "\\src\\";
        filename += title;
        filename += ".sco";
    }
    return filename;
}

std::string GameFolderHelper::GetScriptDebugFileName(uint16_t wScript) const
{
    std::string debugFolder = _GetSubfolder("debug");
    EnsureFolderExists(debugFolder, false);
    return fmt::format("{0}\\{1:03d}.scd", debugFolder, wScript);
}

std::string GameFolderHelper::GetScriptObjectFileName(WORD wScript) const
{
    std::string filename;
    std::string scriptTitle = GetIniString("Script",
                                           default_reskey(wScript, NoBase36),
                                           default_reskey(wScript, NoBase36).
                                           c_str());
    if (!scriptTitle.empty())
    {
        filename = GetScriptObjectFileName(scriptTitle);
    }
    return filename;
}

std::string GameFolderHelper::GetIncludeFolder()
{
    string includeFolder;
    // The normal case...
    TCHAR szPath[MAX_PATH];
    if (GetModuleFileName(nullptr, szPath, ARRAYSIZE(szPath)))
    {
        PSTR pszFileName = PathFindFileName(szPath);
        *pszFileName = 0; // null it
        includeFolder += szPath;
    }
    includeFolder += "include";
    return includeFolder;
}

std::string GameFolderHelper::GetHelpFolder()
{
    string helpFolder;
    TCHAR szPath[MAX_PATH];
    if (GetModuleFileName(nullptr, szPath, ARRAYSIZE(szPath)))
    {
        PSTR pszFileName = PathFindFileName(szPath);
        *pszFileName = 0; // null it
        helpFolder += szPath;
    }
    helpFolder += "Help";
    return helpFolder;
}


//
// Returns an empty string (or pszDefault) if there is no key
//
std::string GameFolderHelper::GetIniString(const std::string& sectionName,
                                           const std::string& keyName,
                                           PCSTR pszDefault) const
{
    return config_store_->GetIniString(sectionName, keyName, pszDefault);
}

bool GameFolderHelper::GetIniBool(const std::string& sectionName,
                                  const std::string& keyName, bool value) const
{
    return config_store_->GetIniBool(sectionName, keyName, value);
}

bool GameFolderHelper::DoesSectionExistWithEntries(
    const std::string& sectionName) const
{
    return config_store_->DoesSectionExistWithEntries(sectionName);
}

std::string GameFolderHelper::GetSubFolder(const std::string& subFolder) const
{
    return _GetSubfolder(subFolder.c_str());
}

std::string GameFolderHelper::_GetSubfolder(const char* key,
                                            const std::string* prefix) const
{
    std::string srcFolder = this->GameFolder;
    if (!srcFolder.empty())
    {
        srcFolder += "\\";
        if (prefix)
        {
            srcFolder += *prefix;
            srcFolder += "\\";
        }
        srcFolder += key;
    }
    return srcFolder;
}

std::string GameFolderHelper::GetSrcFolder(const std::string* prefix) const
{
    return _GetSubfolder("src", prefix);
}

std::string GameFolderHelper::GetLipSyncFolder() const
{
    return _GetSubfolder("lipsync");
}

std::string GameFolderHelper::GetMsgFolder(const std::string* prefix) const
{
    return _GetSubfolder("msg", prefix);
}

std::string GameFolderHelper::GetPicClipsFolder() const
{
    return _GetSubfolder("picClips");
}

std::string GameFolderHelper::GetPolyFolder(const std::string* prefix) const
{
    return _GetSubfolder("poly", prefix);
}

//
// Returns the script identifier for something "main", or "rm001".
//
ScriptId GameFolderHelper::GetScriptId(const std::string& name) const
{
    return ScriptId(GetScriptFileName(name).c_str());
}

bool GameFolderHelper::GetUseSierraAspectRatio(bool defaultValue) const
{
    std::string value = GetIniString(GameSection, AspectRatioKey,
                                     defaultValue
                                         ? TrueValue.c_str()
                                         : FalseValue.c_str());
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == TrueValue;
}

void GameFolderHelper::SetUseSierraAspectRatio(bool useSierra) const
{
    config_->SetUseSierraAspectRatio(useSierra);
}

bool GameFolderHelper::GetUndither() const
{
    std::string value = GetIniString(GameSection, UnditherKey,
                                     FalseValue.c_str()); // False by default
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == TrueValue;
}

void GameFolderHelper::SetUndither(bool undither) const
{
    config_->SetUndither(undither);
}

bool GameFolderHelper::GetGenerateDebugInfo() const
{
    std::string value = GetIniString(GameSection, GenerateDebugInfoKey,
                                     FalseValue.c_str());
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return (value == TrueValue);
}

ResourceSaveLocation GameFolderHelper::GetResourceSaveLocation(
    ResourceSaveLocation location) const
{
    if (location == ResourceSaveLocation::Default)
    {
        std::string value = GetIniString(GameSection, PatchFileKey,
                                         FalseValue.c_str());
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == TrueValue)
                   ? ResourceSaveLocation::Patch
                   : ResourceSaveLocation::Package;
    }
    return location;
}

void GameFolderHelper::SetResourceSaveLocation(
    ResourceSaveLocation location) const
{
    config_->SetResourceSaveLocation(location);
}

ResourceEnumFlags GameFolderHelper::GetDefaultEnumFlags() const
{
    ResourceSaveLocation saveLocation = GetResourceSaveLocation(
        ResourceSaveLocation::Default);
    return (saveLocation == ResourceSaveLocation::Patch)
               ? ResourceEnumFlags::ExcludePackagedFiles
               : ResourceEnumFlags::None;
}

ResourceSourceFlags GameFolderHelper::GetDefaultSaveSourceFlags() const
{
    ResourceSaveLocation saveLocation = GetResourceSaveLocation(
        ResourceSaveLocation::Default);
    return (saveLocation == ResourceSaveLocation::Patch)
               ? ResourceSourceFlags::PatchFile
               : ResourceSourceFlags::ResourceMap;
}

void GameFolderHelper::SetGameFolder(const std::string& gameFolder)
{
    GameFolder = gameFolder;
    // To be safe, reset the game config before dropping the config store.
    config_.reset();
    // Need to recreate the config store if the game folder changes.
    config_store_ = GameConfigStore::FromFilePath(gameFolder + "\\game.ini");
    config_ = std::make_unique<GameConfig>(config_store_.get());
}

std::optional<LangSyntax> GameFolderHelper::GetConfiguredLanguage() const
{
    std::string languageValue = config_store_->GetIniString(GameSection, LanguageKey);
    if (languageValue == "scp")
    {
        // We have left this turd in from old game.inis. We don't support cpp as a default game language,
        // so let's just convert it to Studio.
        return LangSyntaxStudio;
    }
    else if (languageValue == LanguageValueSCI)
    {
        return LangSyntaxSCI;
    }
    else if (languageValue == LanguageValueStudio)
    {
        return LangSyntaxStudio;
    }
    return std::nullopt;
}

//
// Perf: we're opening and closing the file each time.  We could do this once.
//
std::string GameFolderHelper::FigureOutName(ResourceType type, int iNumber,
                                            uint32_t base36Number) const
{
    std::string name;
    if ((size_t)type < ARRAYSIZE(g_resourceInfo))
    {
        std::string keyName = default_reskey(iNumber, base36Number);
        name = GetIniString(GetResourceInfo(type).pszTitleDefault, keyName,
                            keyName.c_str());
    }
    return name;
}

std::unique_ptr<ResourceContainer> GameFolderHelper::Resources(
    const SCIVersion& version, ResourceTypeFlags types,
    ResourceEnumFlags enumFlags, ResourceRecency* pRecency,
    int mapContext) const
{
    return resource_loader_->Resources(version, types, enumFlags, pRecency,
                                       mapContext);
}

std::unique_ptr<ResourceBlob> GameFolderHelper::MostRecentResource(
    const SCIVersion& version, ResourceType type, int number,
    ResourceEnumFlags flags, uint32_t base36Number, int mapContext) const
{
    return resource_loader_->MostRecentResource(version, type, number, flags,
                                                base36Number, mapContext);
}

bool GameFolderHelper::DoesResourceExist(const SCIVersion& version,
                                         ResourceType type, int number,
                                         std::string* retrieveName,
                                         ResourceSaveLocation location) const
{
    return resource_loader_->DoesResourceExist(version, type, number,
                                               retrieveName, location);
}
