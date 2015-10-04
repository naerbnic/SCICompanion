
#pragma once
#include "PaletteOperations.h"
#include "GameFolderHelper.h"
#include "CompiledScript.h"
#include "ResourceEntity.h"
#include "TalkerToViewMap.h"

class DebuggerThread;
struct Vocab000;
struct AudioMapComponent;
class SCIClassBrowser;
class MessageSource;
class MessageHeaderFile;

// FWD declaration
class ResourceContainer;
class ResourceBlob;
class ResourceRecency;
class ResourceEntity;
class GlobalCompiledScriptLookups;
class IResourceMapEvents;

std::string GetIniString(const std::string &iniFileName, std::string &sectionName, const std::string &keyName, PCSTR pszDefault);
HRESULT RebuildResources(BOOL fShowUI);

//
// REVIEW: CResourceMap needs to be protected with a critical section
//

//
// This lets you operate on anything involving the resource.map, and is basically
// where we go to learn information about the currently loaded game.
//
class CResourceMap
{
public:
    CResourceMap();
    ~CResourceMap();

    // ResourceBlob: the raw resource bits already in a ready-to-save format.
    // ResourceEntity: a runtime version of a resource that we can edit.
    HRESULT AppendResource(const ResourceBlob &resource);
    HRESULT AppendResourceAskForNumber(ResourceBlob &resource, bool warnOnOverwrite);
    void AppendResourceAskForNumber(ResourceEntity &resource);
    void AppendResourceAskForNumber(ResourceEntity &resource, const std::string &name, bool warnOnOverwrite = false);
    bool AppendResource(const ResourceEntity &resource, int *pChecksum = nullptr);
    bool AppendResource(const ResourceEntity &resource, int packageNumber, int resourceNumber, uint32_t base36Header = NoBase36, int *pChecksum = nullptr);

    int SuggestResourceNumber(ResourceType type);
    void AssignName(const ResourceBlob &resource);
    void AssignName(ResourceType iType, int iResourceNumber, uint32_t base36Number, PCTSTR pszName);

    // The main functions for enumerating resources.
    std::unique_ptr<ResourceContainer> Resources(ResourceTypeFlags types, ResourceEnumFlags flags, int mapContext = -1);
    std::unique_ptr<ResourceBlob> MostRecentResource(ResourceType type, int number, bool getName, uint32_t base36Number = NoBase36, int mapContext = -1);
    bool DoesResourceExist(ResourceType type, int number, std::string *retrieveName = nullptr);

    void PurgeUnnecessaryResources();

    void AddSync(IResourceMapEvents *pSync);
    void RemoveSync(IResourceMapEvents *pSync);
    void NotifyToReloadResourceType(ResourceType iType);

    void DeleteResource(const ResourceBlob *pResource);

    void SetGameFolder(const std::string &gameFolder);

    TalkerToViewMap &GetTalkerToViewMap();

    std::string GetGameFolder() const;
    std::string GetIncludeFolder();
    std::string GetIncludePath(const std::string &includeFileName);
#ifdef DOCSUPPORT
    std::string GetDocPath(const std::string &fileName);
#endif
    std::string GetTemplateFolder();
    std::string GetSamplesFolder();
    std::string GetObjectsFolder();
    std::string GetDecompilerFolder();
    bool IsGameLoaded() { return !_gameFolderHelper.GameFolder.empty(); }
    HRESULT GetGameIni(PTSTR pszBuf, size_t cchBuf);
    HRESULT GetScriptNumber(ScriptId script, WORD &wScript);
    SCIVersion &GetSCIVersion();
    const GameFolderHelper &Helper() { return _gameFolderHelper; }
    const Vocab000 *GetVocab000();
    const PaletteComponent *GetPalette999();
    void SaveAudioMap65535(const AudioMapComponent &newAudioMap, int mapContext);
    GlobalCompiledScriptLookups *GetCompiledScriptLookups();
    std::vector<int> GetPaletteList();
    std::unique_ptr<PaletteComponent> GetPalette(int fallbackPalette);
    std::unique_ptr<PaletteComponent> GetMergedPalette(const ResourceEntity &resource, int fallbackPalette);
    ResourceEntity *GetVocabResourceToEdit();
    void ClearVocab000();
    SCIClassBrowser *GetClassBrowser() { return _classBrowser.get(); }
    std::unique_ptr<ResourceEntity> CreateResourceFromNumber(ResourceType type, int wNumber, uint32_t base36Number = NoBase36, int mapContext = -1);
    void GetAllScripts(std::vector<ScriptId> &scripts);
	void GetNumberToNameMap(std::unordered_map<WORD, std::string> &scos);
    void SetScriptLanguage(ScriptId script, LangSyntax language);
    LangSyntax GetGameLanguage();
    void SetGameLanguage(LangSyntax language);
    void RemoveScriptFromGame(WORD wScript);
    void SetIncludeFolderForTest(const std::string &folder) { _includeFolderOverride = folder; }
    bool CanSaveResourcesToMap();
    void SkipNextVersionSniff() { _skipVersionSniffOnce = true; }

    MessageSource *GetVerbsMessageSource(bool reload = false);
    MessageSource *GetTalkersMessageSource(bool reload = false);

    bool IsResourceCompatible(const ResourceBlob &resource);

    void StartDebuggerThread(int optionalResourceNumber);
    void AbortDebuggerThread();

private:
    ViewFormat _DetectViewVGAVersion();
    ResourcePackageFormat _DetectPackageFormat();
    ResourceMapFormat _DetectMapFormat();
    bool _HasEarlySCI0Scripts();
    bool _DetectLofsaFormat();
    void _SniffSCIVersion();

    void BeginDeferAppend();
    HRESULT EndDeferAppend();
    void AbandonAppend();
    friend class DeferResourceAppend;

    // Member variables

    TalkerToViewMap _talkerToView;

    std::vector<IResourceMapEvents*> _syncs;

    // Useful resources to cache
    std::unique_ptr<ResourceEntity> _pVocab000;
    std::unique_ptr<ResourceEntity> _pPalette999;
    PaletteComponent _emptyPalette;
    std::vector<int> _paletteList;
    bool _paletteListNeedsUpdate;

    std::unique_ptr<MessageHeaderFile> _verbsHeaderFile;
    std::unique_ptr<MessageHeaderFile> _talkersHeaderFile;


    std::unique_ptr<GlobalCompiledScriptLookups> _globalCompiledScriptLookups;

    // Defer appending resources when you are appending a lot (E.g. during compiling).
    BOOL _cDeferAppend;
    std::vector<ResourceBlob> _deferredResources;

    std::unique_ptr<SCIClassBrowser> _classBrowser;

    GameFolderHelper _gameFolderHelper;

    bool _skipVersionSniffOnce;                     // Skip version sniffing when loading a game the next time.

    std::string _includeFolderOverride;             // For unit-testing

    std::shared_ptr<DebuggerThread> _debuggerThread;
};

// TODO REVIEW: Remove this from header file
HRESULT OpenResourceMap(const std::string &gameFolder, bool fWrite, HANDLE *pHandle);

//
// This class is just a CObject class that wraps a resource type number
//
class CResourceTypeWrap : public CObject
{
public:
    CResourceTypeWrap(ResourceType iType) { _iType = iType; }
    ResourceType GetType() { return _iType; }

private:
    ResourceType _iType;
};

#define RESOURCEMAPTYPE_SCI0 0
#define RESOURCEMAPTYPE_SCI1 1
HRESULT GetResourceMapType(HANDLE hFile, DWORD *pdwType);

//
// Defer the actual writing of resources so it happens in one big batch at the end.
//
class DeferResourceAppend
{
public:
    DeferResourceAppend(CResourceMap &map, bool fDoIt = true) : _map(map)
    {
        if (fDoIt)
        {
            _map.BeginDeferAppend();
        }
        _fDoIt = fDoIt;
    }
    HRESULT Commit()
    {
        if (_fDoIt)
        {
            return _map.EndDeferAppend();
        }
        else
        {
            return S_OK;
        }
    }
    ~DeferResourceAppend()
    {
        if (_fDoIt)
        {
            _map.AbandonAppend();
        }
    }
private:
    CResourceMap &_map;
    bool _fDoIt;
};
