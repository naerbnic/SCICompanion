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
#include "AudioCacheResourceSource.h"
#include "format.h"
#include "ResourceUtil.h"
#include "Message.h"
#include "ResourceEntity.h"
#include "ResourceContainer.h"
#include "AudioMap.h"
#include "Components/AudioNegative.h"
#include "SoundUtil.h"
#include <filesystem>
#include "ResourceBlob.h"
#include "ResourceMap.h"

namespace sys = std::filesystem;

// Tracks which cached audio files are currently up-to-date in the game's main resources.
// Prevents us from having to rebuild resource.aud/sfx unnecessarily.
class UpToDateResources
{
public:
    UpToDateResources(const std::string& cacheFolder) : _cacheTrackerFilename(
        cacheFolder + "\\uptodate.bin")
    {
        std::ifstream file;
        file.open(_cacheTrackerFilename,
                  std::ios_base::in | std::ios_base::binary);
        if (file.is_open())
        {
            int resource;
            while (file.read(reinterpret_cast<char*>(&resource),
                             sizeof(resource)))
            {
                _upToDate.insert(resource);
            }
        }
    }

    void Add(int resource) { _upToDate.insert(resource); }
    void MarkDirty(int resource) { _upToDate.erase(resource); }

    bool IsUpToDate(int resource)
    {
        return _upToDate.find(resource) != _upToDate.end();
    }

    void Save()
    {
        std::ofstream file;
        file.open(_cacheTrackerFilename,
                  std::ios_base::out | std::ios_base::trunc |
                  std::ios_base::binary);
        if (file.is_open())
        {
            for (int resource : _upToDate)
            {
                file.write(reinterpret_cast<const char*>(&resource),
                           sizeof(resource));
            }
        }
    }

private:
    std::set<int> _upToDate;
    const std::string _cacheTrackerFilename;
};

// This handles both base 36 resources and regular audio files.
// It does NOT enumerate maps currently. It does load a map internally, and permits saving maps.
// Hmm... maybe I need to reconsider that... we'll see who actually needs audio maps.
bool ExtractBase36(const char* psz, size_t count, uint16_t& result)
{
    uint16_t value = 0;
    for (size_t i = 0; i < count; i++)
    {
        value *= 36;
        char digit = psz[i];
        if ((digit >= 'A') && (digit <= 'Z'))
        {
            value += (digit - 'A' + 10);
        }
        else if ((digit >= 'a') && (digit <= 'z'))
        {
            value += (digit - 'a' + 10);
        }
        else if ((digit >= '0') && (digit <= '9'))
        {
            value += (digit - '0');
        }
        else
        {
            return false;
        }
    }
    result = value;
    return true;
}

bool ExtractTupleFromFilename(const char* pszFilename, uint32_t& tuple)
{
    assert(lstrlen(pszFilename) == 12); // 8.3

    // ?nnnNNVV.CCS
    struct
    {
        uint16_t Noun, Verb, Condition, Sequence;
    } entry;
    if (ExtractBase36(pszFilename + 4, 2, entry.Noun))
    {
        if (ExtractBase36(pszFilename + 6, 2, entry.Verb))
        {
            if (ExtractBase36(pszFilename + 9, 2, entry.Condition))
            {
                if (ExtractBase36(pszFilename + 11, 1, entry.Sequence))
                {
                    if (entry.Sequence == 0)
                    {
                        // Only 1 digit to represent this. Seq 0 not allowed, seq 36 is.
                        entry.Sequence = 36;
                    }
                    tuple = GetMessageTuple(entry);
                    return true;
                }
            }
        }
    }

    return false;
}

const char* pszAudioCacheFolder = "\\audiocache";

// This is an internal intermediate refactoring, to figure out what the intent is for the various fields of
// the various resource types. This is a no-op at the moment, but the signatures here along with its implementation
// should indicate the intended interface to the greater resource system.
class AudioCacheResourceSource::IResourceManager
{
public:
    virtual ~IResourceManager() = default;

    virtual const std::string& GetGameFolder() const = 0;
    virtual const SCIVersion& GetVersion() const = 0;
    virtual HRESULT CreateFromFile(ResourceBlob* blob, std::string_view name, std::string_view filename, int packageNum, int resourceNum) = 0;

    virtual std::unique_ptr<ResourceContainer> Resources(
        ResourceTypeFlags types,
        ResourceEnumFlags enumFlags, ResourceRecency* pRecency = nullptr,
        int mapContext = -1) const = 0;
    virtual std::unique_ptr<ResourceBlob> MostRecentResource(
        const ResourceId& resource_id, ResourceEnumFlags flags,
        int mapContext = -1) const = 0;

    virtual HRESULT AppendResource(const ResourceEntity& entity) = 0;

    // This is the equivalent of using DeferResourceAppend to wrap multiple adds of ResourceEntities reliably.
    virtual HRESULT AppendResources(const std::vector<const ResourceEntity*>& entities) = 0;
};

namespace {
    class ResourceManagerImpl : public AudioCacheResourceSource::IResourceManager
    {
    public:
        ResourceManagerImpl(
            CResourceMap* resourceMap,
            const std::shared_ptr<const GameFolderHelper>& helper)
                : _version(resourceMap->GetSCIVersion()), _helper(helper), _resourceMap(resourceMap) {}

        const std::string& GetGameFolder() const override
        {
            return _helper->GetGameFolder();
        }

        const SCIVersion& GetVersion() const override
        {
            return _version;
        }

        HRESULT CreateFromFile(ResourceBlob* blob, std::string_view name, std::string_view filename, int packageNum,
                               int resourceNum) override
        {
            return blob->CreateFromFile(std::string(name).c_str(), std::string(filename),
                _version, _helper->GetResourceSaveLocation(ResourceSaveLocation::Default),
                packageNum, resourceNum);
        }

        std::unique_ptr<ResourceContainer> Resources(ResourceTypeFlags types, ResourceEnumFlags enumFlags,
                                                     ResourceRecency* pRecency, int mapContext) const override
        {
            return _helper->Resources(_version, types, enumFlags, pRecency, mapContext);
        }

        std::unique_ptr<ResourceBlob> MostRecentResource(const ResourceId& resource_id, ResourceEnumFlags flags,
            int mapContext) const override
        {
            return _helper->MostRecentResource(_version, resource_id, flags, mapContext);
        }

        HRESULT AppendResource(const ResourceEntity& entity) override
        {
            return _resourceMap->AppendResource(entity);
        }

        HRESULT AppendResources(const std::vector<const ResourceEntity*>& entities) override
        {
            DeferResourceAppend defer(*_resourceMap);
            for (const auto* entity : entities)
            {
                _resourceMap->AppendResource(*entity);
            }
            return defer.Commit();
        }

    private:
        SCIVersion _version;

        // We go through this as much as possible. We create our own,
        // because we may be enumerated on the background thread.
        std::shared_ptr<const GameFolderHelper> _helper;

        // But we do need this sometimes...
        CResourceMap* _resourceMap;
    };
}

AudioCacheResourceSource::AudioCacheResourceSource(
    CResourceMap* resourceMap,
    const std::shared_ptr<const GameFolderHelper>& helper,
    int mapContext,
    ResourceSourceAccessFlags access) :
    _mapContext(mapContext),
    _access(access),
    _enumInitialized(false),
    _resourceMap(std::make_shared<ResourceManagerImpl>(resourceMap, helper))
{
    if (IsFlagSet(access, ResourceSourceAccessFlags::ReadWrite))
    {
        // This is important we do this, for the delete case
        _PrepareForAddOrRemove();
    }
}

void AudioCacheResourceSource::Clear()
{
    DeleteDirectory(nullptr, GetCacheFolder());
}

void AudioCacheResourceSource::_EnsureEnumInitialized()
{
    if (!_enumInitialized)
    {
        _enumInitialized = true;

        // Load the map first. This just tells us which resources are present. The only reason we really need it is that it will initially
        // take its format from the original game files.
        int mapNumber = (_mapContext == -1)
                            ? _resourceMap->GetVersion().AudioMapResourceNumber
                            : _mapContext;
        std::string mapName = fmt::format("{0}.map", mapNumber);
        std::string mapFilename = _resourceMap->GetGameFolder() + pszAudioCacheFolder + "\\";
        mapFilename += mapName;
        ResourceBlob blob;
        if (SUCCEEDED(_resourceMap->CreateFromFile(&blob, mapFilename, mapFilename,
                0, mapNumber)))
        {
            _audioMap = CreateResourceFromResourceData(blob);

            std::string audioFileSpec;
            if (_mapContext == -1)
            {
                // "global" audio resources
                audioFileSpec = GetCacheSubfolderForEnum() + "\\*.aud";
            }
            else
            {
                // base36 audio and sync resources
                // nnnNNVV.CCS
                audioFileSpec = GetCacheSubfolderForEnum() + fmt::format(
                    "\\?{0:0>3t}????.???", _mapContext);
            }

            WIN32_FIND_DATA findData = {};
            HANDLE hFind = FindFirstFile(audioFileSpec.c_str(), &findData);
            bool stillMore = (hFind != INVALID_HANDLE_VALUE);
            while (stillMore)
            {
                int length = lstrlen(findData.cFileName);
                if (findData.cFileName[0] == Base36AudioPrefix)
                {
                    // Check for length==12, because our filter will also match against msdos names.
                    // So that means @1234567.890.wav will match!
                    uint32_t tuple;
                    if ((length == 12) && ExtractTupleFromFilename(
                        findData.cFileName, tuple))
                    {
                        _audioTuplesPresent.push_back(tuple);
                    }
                }
                else if (findData.cFileName[0] == Base36SyncPrefix)
                {
                    uint32_t tuple;
                    if ((length == 12) && ExtractTupleFromFilename(
                        findData.cFileName, tuple))
                    {
                        _syncTuplesPresent.insert(tuple);
                    }
                }
                else
                {
                    // A regular audio file. TOOD: Validation/
                    _audioFilesPresent.push_back(
                        ResourceNumberFromFileName(findData.cFileName));
                }
                stillMore = !!FindNextFile(hFind, &findData);
            }
            FindClose(hFind);
        }
    }
}

const uint32_t Sync36Present = 1;
const uint32_t NoSync36Present = 0;

bool AudioCacheResourceSource::ReadNextEntry(ResourceTypeFlags typeFlags,
                                             IteratorState& state,
                                             ResourceMapEntryAgnostic& entry,
                                             std::vector<uint8_t>*
                                             optionalRawData)
{
    _EnsureEnumInitialized();

    assert(optionalRawData == nullptr);

    bool success = false;
    if (_mapContext == -1)
    {
        // Regular audio files
        if (state.lookupTableIndex < _audioFilesPresent.size())
        {
            int number = _audioFilesPresent[state.lookupTableIndex];
            entry.Number = (uint16_t)number;
            entry.Type = ResourceType::Audio;
            entry.PackageNumber = 0;
            entry.Offset = 0;
            entry.Base36Number = NoBase36;
            entry.ExtraData = 0;
            state.lookupTableIndex++;
            success = true;
        }
    }
    else
    {
        if (state.lookupTableIndex < _audioTuplesPresent.size())
        {
            uint32_t audioFileTuple = _audioTuplesPresent[state.
                lookupTableIndex];
            entry.Number = (uint16_t)_mapContext;
            entry.Type = ResourceType::Audio;
            entry.PackageNumber = 0;
            entry.Offset = 0;
            entry.Base36Number = audioFileTuple;
            entry.ExtraData = (_syncTuplesPresent.find(audioFileTuple) !=
                                  _syncTuplesPresent.end())
                                  ? Sync36Present
                                  : NoSync36Present;
            state.lookupTableIndex++;
            success = true;
        }
    }
    return success;
}

std::string AudioCacheResourceSource::GetCacheFolder() const
{
    return _resourceMap->GetGameFolder() + pszAudioCacheFolder;
}

std::string AudioCacheResourceSource::GetCacheSubfolderForEnum() const
{
    return GetCacheFolder() + fmt::format(
        "\\{0}",
        (_mapContext == -1) ? _resourceMap->GetVersion().AudioMapResourceNumber : _mapContext);
}

uint64_t _GetLookupKey(int number, uint32_t base36)
{
    return number + ((uint64_t)base36 << 32);
}

uint64_t _GetLookupKey(const ResourceMapEntryAgnostic& mapEntry)
{
    return _GetLookupKey(mapEntry.Number, mapEntry.Base36Number);
}

sci::istream AudioCacheResourceSource::GetHeaderAndPositionedStream(
    const ResourceMapEntryAgnostic& mapEntry,
    ResourceHeaderAgnostic& headerEntry)
{
    // REVIEW: Is the offset right?

    std::string fileName = GetFileNameFor(mapEntry.GetResourceId(), _resourceMap->GetVersion());
    assert(!fileName.empty());
    std::string fullPath = GetCacheSubfolderForEnum() + "\\" + fileName;

    // Now fill in the headerEntry
    headerEntry.Number = mapEntry.Number;
    headerEntry.Base36Number = mapEntry.Base36Number;
    headerEntry.Type = mapEntry.Type;
    headerEntry.CompressionMethod = 0;
    headerEntry.Version = _resourceMap->GetVersion();
    headerEntry.SourceFlags = ResourceSourceFlags::AudioCache;
    headerEntry.PackageHint = 0; // No package.

    if ((_mapContext == -1) || (mapEntry.ExtraData == NoSync36Present))
    {
        // Regular audio files, or audio36 files with no companion sync36
        sci::istream readStream = sci::istream::ReadFromFile(fullPath);
        // Keep alive during enumeration

        readStream.SeekAbsolute(mapEntry.Offset);
        headerEntry.cbDecompressed = readStream.GetBytesRemaining();
        headerEntry.cbCompressed = readStream.GetBytesRemaining();

        return readStream;
    }
    else
    {
        std::string fileNameSync = GetFileNameFor(
            mapEntry.GetResourceId().WithType(ResourceType::Sync),
            _resourceMap->GetVersion());
        std::string fullPathSync = GetCacheSubfolderForEnum() + "\\" + fileNameSync;

        // Audio36 plus maybe sync36. We want to combine these two into a single stream,
        // with the Sync36 data coming first, and the sync36 data indicated by BlobKey::LipSyncDataSize
        // We'll use our support for memory mapped files, since a regular streamOwner reads everything in,
        // and we'll need to combine both streams, and we don't want to copy twice.
        auto streamSync36 = sci::istream::MapFile(fullPathSync);
        auto streamAudio36 = sci::istream::MapFile(fullPath);

        std::unique_ptr<sci::ostream> combinedStream = std::make_unique<
            sci::ostream>();
        combinedStream->EnsureCapacity(
            streamAudio36.GetDataSize() + streamSync36.GetDataSize());

        // First comes the sync data
        sci::transfer(streamSync36, *combinedStream,
                      streamSync36.GetDataSize());
        headerEntry.PropertyBag[BlobKey::LipSyncDataSize] = combinedStream->
            tellp();
        // Then the audio
        sci::transfer(streamAudio36, *combinedStream,
            streamAudio36.GetDataSize());

        headerEntry.cbDecompressed = combinedStream->GetDataSize();
        headerEntry.cbCompressed = headerEntry.cbDecompressed;

        sci::istream readStream = sci::istream_from_ostream(*combinedStream);
        _streamHolders2[_GetLookupKey(mapEntry)] = std::move(combinedStream);
        // Keep alive
        return readStream;
    }

    return sci::istream(nullptr, 0); // Empty stream....
}

// Split a resource blob into audio and sync files (if appropriate)
void SaveAudioBlobToFiles(const ResourceBlob& blob,
                          const std::string& cacheSubFolder)
{
    uint32_t lipSyncDataSize = 0;
    auto it = blob.GetPropertyBag().find(BlobKey::LipSyncDataSize);
    if (it != blob.GetPropertyBag().end())
    {
        lipSyncDataSize = it->second;
    }

    std::string syncFileName = GetFileNameFor(blob.GetResourceId().WithType(ResourceType::Sync),
                                              blob.GetVersion());
    std::string syncFullPath = cacheSubFolder + "\\" + syncFileName;
    if (lipSyncDataSize)
    {
        OldScopedFile syncFile(syncFullPath, GENERIC_WRITE, 0, CREATE_ALWAYS);
        syncFile.Write(blob.GetData(), lipSyncDataSize);
    }
    else
    {
        // If we *don't* have a sync component, we should delete this file. Otherwise it'll keep coming back when we rebuild or enumerate.
        deletefile(syncFullPath);
    }

    std::string audioFileName = GetFileNameFor(blob.GetResourceId().WithType(ResourceType::Audio),
                                               blob.GetVersion());
    OldScopedFile syncFile(cacheSubFolder + "\\" + audioFileName, GENERIC_WRITE, 0,
                        CREATE_ALWAYS);
    syncFile.Write(blob.GetData() + lipSyncDataSize,
                   blob.GetDecompressedLength() - lipSyncDataSize);
}

void FirstTimeAudioExtraction(AudioCacheResourceSource::IResourceManager const* resourceMap,
                              const std::string& cacheFolder,
                              const std::string& cacheSubFolder,
                              int mapContext)
{
    int resourceNumber = (mapContext == -1)
                             ? resourceMap->GetVersion().AudioMapResourceNumber
                             : mapContext;
    std::unique_ptr<ResourceBlob> audioMapBlob = resourceMap->MostRecentResource(
        ResourceId(ResourceType::AudioMap, ResourceNum::FromNumber(resourceNumber)),
        ResourceEnumFlags::None);
    if (audioMapBlob)
    {
        // First the audio map.
        std::string audioMapPath = cacheFolder + "\\" + GetFileNameFor(
            *audioMapBlob);
        OldScopedFile file(audioMapPath, GENERIC_WRITE, 0, CREATE_ALWAYS);
        audioMapBlob->SaveToHandle(file.hFile, true);

        // Now all audio and sync resources.
        auto resourceContainer = resourceMap->Resources(
            ResourceTypeFlags::Audio, ResourceEnumFlags::None, nullptr,
            mapContext);
        for (auto& blob : *resourceContainer)
        {
            SaveAudioBlobToFiles(*blob, cacheSubFolder);
        }
    }
}

// Call this before adding or remove resources. It will return the audio map that we need to modify.
std::unique_ptr<ResourceEntity>
AudioCacheResourceSource::_PrepareForAddOrRemove()
{
    std::unique_ptr<ResourceEntity> audioMap;

    // First, ensure our cache folder exists
    EnsureFolderExists(GetCacheFolder());
    EnsureFolderExists(GetCacheSubfolderForEnum());

    // Now, we need to write the existing audio map. This audio map *could* come from us, or it could come from the original.
    // If it came from the original, we need to extract all the files for the first time.
    int resourceNumber = (_mapContext == -1)
                             ? _resourceMap->GetVersion().AudioMapResourceNumber
                             : _mapContext;
    std::unique_ptr<ResourceBlob> audioMapBlobTest = _resourceMap->
        MostRecentResource(ResourceId(ResourceType::AudioMap, ResourceNum::FromNumber(resourceNumber)),
                           ResourceEnumFlags::IncludeCacheFiles |
                           ResourceEnumFlags::MostRecentOnly);
    if (audioMapBlobTest)
    {
        if (!IsFlagSet(audioMapBlobTest->GetSourceFlags(),
                       ResourceSourceFlags::AudioMapCache))
        {
            FirstTimeAudioExtraction(_resourceMap.get(), GetCacheFolder(),
                                     GetCacheSubfolderForEnum(),
                                     _mapContext);
            audioMapBlobTest = _resourceMap->MostRecentResource(
                ResourceId(ResourceType::AudioMap, ResourceNum::FromNumber(resourceNumber)),
                ResourceEnumFlags::IncludeCacheFiles |
                ResourceEnumFlags::MostRecentOnly);
        }

        // Verify we're reading the one from the audio cache.
        assert(
            IsFlagSet(audioMapBlobTest->GetSourceFlags(), ResourceSourceFlags::
                AudioMapCache));

        // Now, modify the audio map as per the resourceblobs provided.
        audioMap = CreateResourceFromResourceData(*audioMapBlobTest, false);
    }
    else
    {
        // Make a default empty audio map. This is used when we create new audio maps (e.g. when creating new message resources)
        audioMap = CreateDefaultMapResource(_resourceMap->GetVersion(), resourceNumber);
        audioMap->SourceFlags = ResourceSourceFlags::AudioMapCache;
    }
    return audioMap;
}

void AudioCacheResourceSource::RemoveEntry(
    const ResourceMapEntryAgnostic& mapEntry)
{
    std::vector<uint32_t> tuples = {mapEntry.Base36Number};
    RemoveEntries(mapEntry.Number, tuples);
}

void AudioCacheResourceSource::SaveOrRemoveNegatives(
    const std::vector<ResourceEntity*> negatives)
{
    try
    {
        for (ResourceEntity* resource : negatives)
        {
            std::string fullPath = GetCacheSubfolderForEnum() + "\\" +
                GetFileNameFor(resource->GetResourceId().WithType(ResourceType::Audio), _resourceMap->GetVersion()) + ".wav";
            const AudioNegativeComponent* neg = resource->TryGetComponent<
                AudioNegativeComponent>();
            if (neg)
            {
                WriteWaveFile(fullPath, neg->Audio, &neg->Settings);
            }
            else
            {
                // Remove any negative, there's none associated with this guy.
                deletefile(fullPath);
            }
        }
    }
    catch (...)
    {
    }
}

void AudioCacheResourceSource::MaybeAddNegative(ResourceEntity& resource)
{
    try
    {
        std::string fullPath = GetCacheSubfolderForEnum() + "\\" + GetFileNameFor(
            resource.GetResourceId().WithType(ResourceType::Audio),
            _resourceMap->GetVersion()) + ".wav";
        if (PathFileExists(fullPath.c_str()))
        {
            std::unique_ptr<AudioNegativeComponent> negative = std::make_unique<
                AudioNegativeComponent>();
            AudioComponentFromWaveFile(sci::istream::ReadFromFile(fullPath), negative->Audio,
                                       &negative->Settings);
            resource.AddComponent(std::move(negative));
        }
    }
    catch (...)
    {
    }
}

void AudioCacheResourceSource::RemoveEntries(int number,
                                             const std::vector<uint32_t> tuples)
{
    try
    {
        std::unique_ptr<ResourceEntity> audioMap = _PrepareForAddOrRemove();
        AudioMapComponent& audioMapComponent = audioMap->GetComponent<
            AudioMapComponent>();
        bool audioMapModified = false;
        for (uint32_t tuple : tuples)
        {
            // Find the matching entry and remove it 
            auto itFind = std::find_if(audioMapComponent.Entries.begin(),
                                       audioMapComponent.Entries.end(),
                                       [number, tuple
                                       ](const AudioMapEntry& amEntry)
                                       {
                                           return amEntry.Number == number &&
                                               GetMessageTuple(amEntry) ==
                                               tuple;
                                       });
            if (itFind != audioMapComponent.Entries.end())
            {
                audioMapComponent.Entries.erase(itFind);
                audioMapModified = true;
            }

            auto resource_num = ResourceNum::CreateWithBase36(number, tuple);

            // Now we need to delete any files associated with it.
            std::string fullPath = GetCacheSubfolderForEnum() + "\\" +
                GetFileNameFor(ResourceId(ResourceType::Sync, resource_num), _resourceMap->GetVersion());
            deletefile(fullPath);
            fullPath += ".wav";
            deletefile(fullPath); // The negative, if it exists.
            fullPath = GetCacheSubfolderForEnum() + "\\" + GetFileNameFor(
                ResourceId(ResourceType::Sync, resource_num), _resourceMap->GetVersion());
            deletefile(fullPath);
        }

        // And finally, save the modified audiomap. We *should* just be able to go through the resource map again,
        // and it should route it to the "patch files" resource source, under the audiocache folder.
        if (audioMapModified)
        {
            assert(
                IsFlagSet(audioMap->SourceFlags, ResourceSourceFlags::
                    AudioMapCache));
            _resourceMap->AppendResource(*audioMap);

            // This is no longer up-to-date.
            UpToDateResources upToDate(GetCacheFolder());
            upToDate.MarkDirty(audioMap->ResourceNumber);
            upToDate.Save();
        }
    }
    catch (std::exception)
    {
        // REVIEW
    }
}

AppendBehavior AudioCacheResourceSource::AppendResources(
    const std::vector<const ResourceBlob*>& blobs)
{
    try
    {
        std::unique_ptr<ResourceEntity> audioMap = _PrepareForAddOrRemove();
        AudioMapComponent& audioMapComponent = audioMap->GetComponent<
            AudioMapComponent>();

        // If there is no matching entry, add one. We don't currently care about offsets and sync sizes,
        // since those are only relevant when the resources exist in the official audio map.
        for (const ResourceBlob* blobToBeSaved : blobs)
        {
            int number = blobToBeSaved->GetNumber();
            uint32_t tuple = blobToBeSaved->GetBase36();
            auto itFind = std::find_if(audioMapComponent.Entries.begin(),
                                       audioMapComponent.Entries.end(),
                                       [number, tuple
                                       ](const AudioMapEntry& amEntry)
                                       {
                                           return amEntry.Number == number &&
                                               GetMessageTuple(amEntry) ==
                                               tuple;
                                       });
            if (itFind == audioMapComponent.Entries.end())
            {
                AudioMapEntry newEntry = {};
                SetMessageTuple(newEntry, tuple);
                newEntry.Number = number;
                audioMapComponent.Entries.push_back(newEntry);
            }

            // Meanwhile, save this blob to files
            SaveAudioBlobToFiles(*blobToBeSaved, GetCacheSubfolderForEnum());
        }

        // And finally, serialize the audiomap and save it. We *should* just be able to go through the resource map again,
        // and it should route it to the "patch files" resource source, under the audiocache folder.
        assert(
            IsFlagSet(audioMap->SourceFlags, ResourceSourceFlags::AudioMapCache
            ));
        _resourceMap->AppendResource(*audioMap);

        // This is no longer up-to-date.
        UpToDateResources upToDate(GetCacheFolder());
        upToDate.MarkDirty(audioMap->ResourceNumber);
        upToDate.Save();
    }
    catch (std::exception)
    {
    }

    return AppendBehavior::Replace;
}

void RebuildFromResources(const SCIVersion& version,
                          const std::string& gameFolder,
                          AudioMapComponent& audioMap, int number,
                          std::ostream& writeStream)
{
    auto helper = GameFolderHelper::Create();
    helper->SetGameFolder(gameFolder);

    // First, cache all the blobs
    std::unordered_map<uint64_t, std::unique_ptr<ResourceBlob>> blobs;
    int mapContext = (number == version.AudioMapResourceNumber) ? -1 : number;
    auto resourceContainer = helper->Resources(version, ResourceTypeFlags::Audio,
                                              ResourceEnumFlags::MostRecentOnly
                                              | ResourceEnumFlags::AddInDefaultEnumFlags,
                                              nullptr, mapContext);
    for (auto& blob : *resourceContainer)
    {
        uint64_t key = _GetLookupKey(blob->GetNumber(), blob->GetBase36());
        blobs[key] = std::move(blob);
    }

    std::vector<AudioMapEntry> newEntries;
    for (auto& entry : audioMap.Entries)
    {
        uint32_t tuple = GetMessageTuple(entry);
        uint64_t key = _GetLookupKey(entry.Number, tuple);
        auto itFind = blobs.find(key);
        if (itFind != blobs.end())
        {
            ResourceBlob& blob = *itFind->second;
            entry.Offset = static_cast<uint32_t>(writeStream.tellp());
            entry.SyncSize = 0;
            auto props = blob.GetPropertyBag();
            auto itProp = props.find(BlobKey::LipSyncDataSize);
            if (itProp != props.end())
            {
                entry.SyncSize = itProp->second;
            }
            newEntries.push_back(entry);

            // Transfer data
            if (blob.GetDecompressedLength() > 0)
            {
                writeStream.write(reinterpret_cast<const char*>(blob.GetData()),
                                  blob.GetDecompressedLength());
            }
            blob.GetReadStream();
        }
        // else leave it out
    }

    // Assign the new ones...
    std::swap(audioMap.Entries, newEntries);
}

void RebuildFromAudioCacheFiles(SCIVersion version,
                                const std::string& cacheSubfolder,
                                AudioMapComponent& audioMap, int number,
                                std::ostream& writeStream)
{
    bool isMain = number == version.AudioMapResourceNumber;

    std::vector<AudioMapEntry> newEntries;
    for (auto& entry : audioMap.Entries)
    {
        uint32_t tuple = isMain ? NoBase36 : GetMessageTuple(entry);
        auto resource_num = ResourceNum::CreateWithBase36(entry.Number, tuple);
        std::string fullPathAudio = cacheSubfolder + "\\" + GetFileNameFor(
            ResourceId(ResourceType::Audio, resource_num), version);
        std::string fullPathSync;
        if (!isMain)
        {
            fullPathSync = cacheSubfolder + "\\" + GetFileNameFor(
                ResourceId(ResourceType::Sync, resource_num), version);
        }

        entry.Offset = static_cast<uint32_t>(writeStream.tellp());
        entry.SyncSize = 0;
        // If it exists, open sync file and write it. Then take note of size. Then write audio.
        if (sys::exists(sys::path(fullPathAudio)))
        {
            if (!fullPathSync.empty() && sys::exists(sys::path(fullPathSync)))
            {
                std::ifstream syncFile;
                syncFile.exceptions(
                    std::ifstream::failbit | std::ifstream::badbit);
                syncFile.open(fullPathSync,
                              std::ios_base::binary | std::ios_base::in);
                writeStream << syncFile.rdbuf();
                entry.SyncSize = (uint32_t)writeStream.tellp() - entry.Offset;
            }

            std::ifstream audFile;
            audFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            audFile.open(fullPathAudio,
                         std::ios_base::binary | std::ios_base::in);
            writeStream << audFile.rdbuf();

            newEntries.push_back(entry);
        }
        // If we didn't find it, skip (TODO: log this isue)
    }

    // Assign the new ones...
    std::swap(audioMap.Entries, newEntries);
}

std::ostream* _ChooseBakOutputStream(const AudioCacheResourceSource::IResourceManager* resourceMap, int number,
                                     std::ofstream& audStream,
                                     std::ofstream& sfxStream)
{
    AudioVolumeName volumeName;
    if (number == resourceMap->GetVersion().AudioMapResourceNumber)
    {
        volumeName = GetVolumeToUse(resourceMap->GetVersion(), NoBase36);
    }
    else
    {
        volumeName = GetVolumeToUse(resourceMap->GetVersion(), number);
    }
    std::ofstream* toUse = (volumeName == AudioVolumeName::Aud)
                               ? &audStream
                               : &sfxStream;

    if (!toUse->is_open())
    {
        std::string path = GetAudioVolumePath(resourceMap->GetGameFolder(), true, volumeName);
        toUse->exceptions(std::ofstream::failbit | std::ofstream::badbit);
        toUse->open(
            path, std::ios_base::out | std::ios_base::trunc |
            std::ios_base::binary);
    }

    return toUse;
}

void AudioCacheResourceSource::RebuildResources(
    bool force, ResourceSource& source,
    std::map<ResourceType, RebuildStats>& stats)
{
    UpToDateResources upToDate(GetCacheFolder());

    // 1) Find all the audio maps. We'll use the message resources to do this. This is so we don't include audio that no longer
    // has a place (e.g. no matching message resource - suppose it was deleted)
    std::set<int> audioMapNumbers;
    audioMapNumbers.insert(_resourceMap->GetVersion().AudioMapResourceNumber);
    // The default audio map.
    {
        auto resourceContainer = _resourceMap->Resources(
            ResourceTypeFlags::Message,
            ResourceEnumFlags::MostRecentOnly |
            ResourceEnumFlags::AddInDefaultEnumFlags);
        // Use iterator directly so we don't instantiate the blobs
        for (auto it = resourceContainer->begin(); it != resourceContainer->
             end(); ++it)
        {
            audioMapNumbers.insert(it.GetResourceNumber());
        }
    }

    // 2) Accumulate the audio maps... we can just do a regular enumeration, which should do them in this order:
    //      a) audio cache folder
    //      b) patch files (is this even necessary?)
    //      c) std resource map
    bool allCacheFilesUpToDate = true;
    std::map<int, std::unique_ptr<ResourceEntity>> audioMaps;
    {
        auto resourceContainer = _resourceMap->Resources(
            ResourceTypeFlags::AudioMap,
            ResourceEnumFlags::MostRecentOnly |
            ResourceEnumFlags::IncludeCacheFiles |
            ResourceEnumFlags::AddInDefaultEnumFlags);
        for (auto& blob : *resourceContainer)
        {
            // If this audio map is sourced from the cache, then if it's out of date we definitely need to rebuild.
            if (ResourceSourceFlags::AudioMapCache == blob->GetSourceFlags())
            {
                allCacheFilesUpToDate = allCacheFilesUpToDate && upToDate.
                    IsUpToDate(blob->GetNumber());
            }
            audioMaps[blob->GetNumber()] = std::move(
                CreateResourceFromResourceData(*blob));
        }
    }

    // We can avoid doing anything here if we know that all cached files have up-to-date versions built into the game's resources.
    bool needToRebuild = !allCacheFilesUpToDate || force;
    if (needToRebuild)
    {
        // 3) Based on the information in the audio maps, write the necessary audio resources into the audio volume files (resource.aud/resource.sfx, as appropriate)
        std::ofstream audStream;
        std::ofstream sfxStream;
        for (int amNumber : audioMapNumbers)
        {
            auto itAudioMapPair = audioMaps.find(amNumber);
            if (itAudioMapPair != audioMaps.end())
            {
                std::ostream& streamToUse = *_ChooseBakOutputStream(
                    _resourceMap.get(), itAudioMapPair->first, audStream,
                    sfxStream);

                ResourceEntity* audioMapResource = itAudioMapPair->second.get();
                // Rebuilding from files is about 3x slower in release mode (about 100x slower in debug due to the way we copy data),
                // vs just copying out of the original resources.
                // So let's avoid it if we can.
                bool rebuildFromCacheFiles = false;
                if ((audioMapResource->SourceFlags ==
                    ResourceSourceFlags::AudioMapCache))
                {
                    rebuildFromCacheFiles = force || !upToDate.IsUpToDate(
                        audioMapResource->ResourceNumber);
                    // But it *will* be up-to-date when we're done with it:
                    upToDate.Add(audioMapResource->ResourceNumber);
                }
                if (rebuildFromCacheFiles)
                {
                    //  Copy all the cache files directly into this stream...
                    std::string cacheSubfolder = GetCacheFolder() + fmt::format(
                        "\\{0}", audioMapResource->ResourceNumber);
                    RebuildFromAudioCacheFiles(_resourceMap->GetVersion(), cacheSubfolder,
                                               audioMapResource->GetComponent<
                                                   AudioMapComponent>(),
                                               audioMapResource->ResourceNumber,
                                               streamToUse);
                }
                else
                {
                    // Copy straight from the resources.
                    RebuildFromResources(_resourceMap->GetVersion(), _resourceMap->GetGameFolder(),
                                         audioMapResource->GetComponent<
                                             AudioMapComponent>(),
                                         audioMapResource->ResourceNumber,
                                         streamToUse);
                }
            }
        }

        // 4) Test that we can open the non bak versions for writing
        if (audStream.is_open())
        {
            testopenforwrite(
                GetAudioVolumePath(_resourceMap->GetGameFolder(), false, AudioVolumeName::Aud));
        }
        if (sfxStream.is_open())
        {
            testopenforwrite(
                GetAudioVolumePath(_resourceMap->GetGameFolder(), false, AudioVolumeName::Sfx));
        }

        // 5) If that's good, then save the audio maps *TO THE RESOURCE MAP*
        {
            std::vector<const ResourceEntity*> addedAudioMaps;
            for (auto& audioMap : audioMaps)
            {
                audioMap.second->SourceFlags = ResourceSourceFlags::ResourceMap;
                addedAudioMaps.push_back(audioMap.second.get());
            }
            _resourceMap->AppendResources(addedAudioMaps);
        }

        // 6) And then if that's good, then replace the audio files.
        if (audStream.is_open())
        {
            audStream.close();
            std::string finalPath = GetAudioVolumePath(
                _resourceMap->GetGameFolder(), false, AudioVolumeName::Aud);
            deletefile(finalPath);
            movefile(
                GetAudioVolumePath(_resourceMap->GetGameFolder(), true, AudioVolumeName::Aud),
                finalPath);
        }
        if (sfxStream.is_open())
        {
            sfxStream.close();
            std::string finalPath = GetAudioVolumePath(
                _resourceMap->GetGameFolder(), false, AudioVolumeName::Sfx);
            deletefile(finalPath);
            movefile(
                GetAudioVolumePath(_resourceMap->GetGameFolder(), true, AudioVolumeName::Sfx),
                finalPath);
        }

        upToDate.Save();
    }
    // TODO: Check if there are audio maps that have no representation in a message resource?
}
