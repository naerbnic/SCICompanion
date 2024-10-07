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
#pragma once

#include <optional>

class ResourceContainer;
enum class ResourceEnumFlags : uint16_t;
enum class ResourceSaveLocation : uint16_t;
enum class ResourceSourceFlags : int;

// Encapsulates operations that only depend on gamefolder and SCI version.
// We can carry this to a background thread, for instace, and not have to
// interact with the ResourceMap object.

class ResourceRecency;
class ResourceBlob;

// fwd decl for defs in this file
class GameFolderHelper;

extern const std::string GameSection;
extern const std::string LanguageKey;
extern const std::string LanguageValueStudio;
extern const std::string LanguageValueSCI;

enum class ResourceSaveLocation : uint16_t
{
    Default,
    Package,
    Patch,
};

class GameConfigStore
{
public:
    static std::unique_ptr<GameConfigStore> FromFilePath(
        std::string_view config_file);

    virtual ~GameConfigStore() = default;
    [[nodiscard]] virtual std::string GetIniString(
        const std::string& sectionName, const std::string& keyName,
        std::string_view pszDefault = "") const = 0;
    [[nodiscard]] virtual bool GetIniBool(const std::string& sectionName,
                                          const std::string& keyName,
                                          bool value = false) const = 0;
    virtual void SetIniString(const std::string& sectionName,
                              const std::string& keyName,
                              const std::string& value) const = 0;
    virtual void SetIniBool(const std::string& sectionName,
                            const std::string& keyName, bool value) const = 0;
    virtual bool DoesSectionExistWithEntries(
        const std::string& sectionName) const = 0;
    virtual std::map<std::string, std::string> GetSectionEntries(
        const std::string& sectionName) const = 0;
};

class GameConfig
{
public:
    explicit GameConfig(const GameConfigStore* store);

    void SetResourceEntry(ResourceType resource_type, int resource_number,
                          uint32_t base36_number,
                          const std::string& resource_name) const;
    void SetLanguage(LangSyntax language) const;
    void SetUseSierraAspectRatio(bool use_sierra) const;
    void SetUndither(bool undither) const;
    void SetResourceSaveLocation(ResourceSaveLocation location) const;
    std::string GetScriptTitleForNumber(uint16_t script_number) const;
    bool GetUseSierraAspectRatio(bool default_value) const;
    bool GetUndither() const;
    ResourceSaveLocation ResolveSaveLocationOption(ResourceSaveLocation location) const;
    std::string FigureOutName(ResourceType type, int iNumber, uint32_t base36_number) const;
    bool GetGenerateDebugInfo() const;

private:
    GameConfigStore const* store_;
};

class ResourceLoader
{
public:
    explicit ResourceLoader(const GameFolderHelper* parent);

    std::unique_ptr<ResourceContainer> Resources(
        const SCIVersion& version, ResourceTypeFlags types,
        ResourceEnumFlags enumFlags, ResourceRecency* pRecency = nullptr,
        int mapContext = -1) const;
    std::unique_ptr<ResourceBlob> MostRecentResource(
        const SCIVersion& version, ResourceType type, int number,
        ResourceEnumFlags flags, uint32_t base36Number = NoBase36,
        int mapContext = -1) const;
    bool DoesResourceExist(const SCIVersion& version, ResourceType type,
                           int number, std::string* retrieveName,
                           ResourceSaveLocation location) const;

private:
    // The owning object
    const GameFolderHelper* parent_;
};

class GameFolderHelper : public std::enable_shared_from_this<GameFolderHelper>
{
public:
    static std::shared_ptr<GameFolderHelper> Create();
    static std::shared_ptr<GameFolderHelper> Create(
        const std::string& game_folder);

    GameFolderHelper(const GameFolderHelper& orig) = delete;
    GameFolderHelper(GameFolderHelper&& orig) = delete;
    GameFolderHelper& operator=(const GameFolderHelper& other) = delete;
    GameFolderHelper& operator=(GameFolderHelper&& other) = delete;

    std::string GetScriptFileName(const std::string& name) const;
    std::string GetScriptFileName(uint16_t wScript) const;
    std::string GetScriptObjectFileName(const std::string& title) const;
    std::string GetScriptObjectFileName(uint16_t wScript) const;
    std::string GetScriptDebugFileName(uint16_t wScript) const;
    std::string GetSrcFolder(const std::string* prefix = nullptr) const;
    std::string GetMsgFolder(const std::string* prefix = nullptr) const;
    std::string GetPicClipsFolder() const;
    std::string GetSubFolder(const std::string& subFolder) const;
    std::string GetLipSyncFolder() const;
    std::string GetPolyFolder(const std::string* prefix = nullptr) const;
    bool GetIniBool(const std::string& sectionName, const std::string& keyName,
                    bool value = false) const;
    bool DoesSectionExistWithEntries(const std::string& sectionName) const;
    static std::string GetIncludeFolder();
    static std::string GetHelpFolder();
    LangSyntax GetDefaultGameLanguage() const { return Language; }
    ScriptId GetScriptId(const std::string& name) const;
    std::string FigureOutName(ResourceType type, int iResourceNum,
                              uint32_t base36Number) const;
    std::unique_ptr<ResourceContainer> Resources(
        const SCIVersion& version, ResourceTypeFlags types,
        ResourceEnumFlags enumFlags, ResourceRecency* pRecency = nullptr,
        int mapContext = -1) const;
    std::unique_ptr<ResourceBlob> MostRecentResource(
        const SCIVersion& version, ResourceType type, int number,
        ResourceEnumFlags flags, uint32_t base36Number = NoBase36,
        int mapContext = -1) const;
    bool DoesResourceExist(const SCIVersion& version, ResourceType type,
                           int number, std::string* retrieveName,
                           ResourceSaveLocation location) const;

    bool GetUseSierraAspectRatio(bool defaultValue) const;
    void SetUseSierraAspectRatio(bool useSierra) const;
    bool GetUndither() const;
    void SetUndither(bool undither) const;

    bool GetGenerateDebugInfo() const;

    ResourceSaveLocation GetResourceSaveLocation(
        ResourceSaveLocation location) const;
    void SetResourceSaveLocation(ResourceSaveLocation location) const;
    ResourceEnumFlags GetDefaultEnumFlags() const;
    ResourceSourceFlags GetDefaultSaveSourceFlags() const;

    const std::string& GetGameFolder() const { return GameFolder; }

    void SetGameFolder(const std::string& gameFolder);

    // Returns the language that is defined in our configuration, or std::nullopt if
    // it is not defined.
    std::optional<LangSyntax> GetConfiguredLanguage() const;

    LangSyntax GetLanguage() const { return Language; }
    void SetLanguage(LangSyntax language) { Language = language; }

    const GameConfigStore& GetConfigStore() const
    {
        return *config_store_;
    }

    const GameConfig& GetConfig() const
    {
        return *config_;
    }

    const ResourceLoader& GetResourceLoader() const
    {
        return *resource_loader_;
    }

private:
    GameFolderHelper();

    std::string _GetSubfolder(const char* key,
                              const std::string* prefix = nullptr) const;

    // Members
    std::string GameFolder;
    LangSyntax Language;
    std::unique_ptr<const GameConfigStore> config_store_;
    std::unique_ptr<const GameConfig> config_;
    std::unique_ptr<const ResourceLoader> resource_loader_;
};
