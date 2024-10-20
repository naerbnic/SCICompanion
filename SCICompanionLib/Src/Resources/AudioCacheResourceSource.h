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

#include "ResourceContainer.h"
#include "GameFolderHelper.h"
#include "ResourceSourceImpls.h"

struct AudioMapComponent;
class ResourceEntity;
class ResourceBlob;
class CResourceMap;

extern const char* pszAudioCacheFolder;

// ResourceSource for the audio resources cached by SCI Companion
//
// The SCI Companion folder tree for audio cache files looks like this:
//  gamefolder\
//      audiocache\\
//          uptodate.bin        --> a list of resources that are up-to-date.
//          4.map
//          17.map
//          65535.map
//          4\
//              individual sync36, audio36 under here.
//          17\
//              individual sync36, audio36 under here.
//          65535\
//              individual sync36, audio36 under here.
class AudioCacheResourceSource : public ResourceSource
{
public:
    class IResourceManager;
    AudioCacheResourceSource(
        CResourceMap* resourceMap,
        const std::shared_ptr<const GameFolderHelper>& helper,
        int mapContext,
        ResourceSourceAccessFlags access);
    ~AudioCacheResourceSource() override = default;

    AudioCacheResourceSource& operator=(AudioCacheResourceSource& src) = delete;

    bool ReadNextEntry(ResourceTypeFlags typeFlags, IteratorState& state,
                       ResourceMapEntryAgnostic& entry,
                       std::vector<uint8_t>* optionalRawData =
                           nullptr) override;
    sci::istream GetHeaderAndPositionedStream(
        const ResourceMapEntryAgnostic& mapEntry,
        ResourceHeaderAgnostic& headerEntry) override;

    sci::istream GetPositionedStreamAndResourceSizeIncludingHeader(
        const ResourceMapEntryAgnostic& mapEntry, uint32_t& size,
        bool& includesHeader) override
    {
        assert(false && "Not implemented");
        return sci::istream(nullptr, 0);
    }

    void RemoveEntry(const ResourceMapEntryAgnostic& mapEntry) override;
    AppendBehavior AppendResources(
        const std::vector<const ResourceBlob*>& blobs) override;
    void RebuildResources(bool force, ResourceSource& source,
                          std::map<ResourceType, RebuildStats>& stats) override;

    // A way to call RemoveEntry directly, for more efficiency.
    void RemoveEntries(int number, const std::vector<uint32_t> tuples);
    void SaveOrRemoveNegatives(const std::vector<ResourceEntity*> negatives);
    void MaybeAddNegative(ResourceEntity& resource);

    void Clear();

private:
    std::unique_ptr<ResourceEntity> _PrepareForAddOrRemove();
    void _EnsureEnumInitialized();
    std::string GetCacheFolder() const;
    std::string GetCacheSubfolderForEnum() const;

    int _mapContext;
    ResourceSourceAccessFlags _access;
    ResourceSourceFlags _sourceFlags;

    // Enumeration
    std::vector<uint32_t> _audioTuplesPresent; // base 36
    std::unordered_set<uint32_t> _syncTuplesPresent;
    std::unique_ptr<ResourceEntity> _audioMap;
    std::vector<int> _audioFilesPresent; // regular

    std::unordered_map<uint64_t, std::unique_ptr<sci::ostream>> _streamHolders2;

    bool _enumInitialized;

    std::shared_ptr<IResourceManager> _resourceMap;
};

void SaveAudioBlobToFiles(const ResourceBlob& blob,
                          const std::string& cacheSubFolder);
