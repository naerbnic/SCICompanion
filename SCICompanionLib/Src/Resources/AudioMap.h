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

#include "Components.h"
#include "ResourceEntity.h"

bool IsMainAudioMap(AudioMapVersion version);

class ResourceEntity;

struct AudioMapTraits
{
    AudioMapTraits(const AudioMapTraits &src) = delete;
    AudioMapTraits& operator=(const AudioMapTraits &src) = delete;

    int MainAudioMapResourceNumber;

};

struct AudioMapEntry
{
    AudioMapEntry() : Number(0), Offset(0), Noun(0xff), Verb(0xff), Condition(0xff), Sequence(0xff), SyncSize(0) {}

    uint16_t Number;
    uint32_t Offset;

    // For SyncMapEarly we have these extra 6 bytes:
    uint8_t Noun;
    uint8_t Verb;
    uint8_t Condition;
    uint8_t Sequence;
    uint16_t SyncSize;
};

struct AudioMapComponent : public ResourceComponent
{
public:
    AudioMapComponent();
    AudioMapComponent(const AudioMapComponent &src) = default;
    AudioMapComponent(const AudioMapTraits &traits);
    ~AudioMapComponent() {}

    ResourceComponent *Clone() const override
    {
        return new AudioMapComponent(*this);
    }

    std::vector<AudioMapEntry> Entries;
    AudioMapVersion Version;

    const AudioMapTraits &Traits;
};

std::unique_ptr<ResourceEntityFactory> CreateAudioMapResourceFactory();
std::unique_ptr<ResourceEntity> CreateDefaultMapResource(const SCIVersion& version, int number);
