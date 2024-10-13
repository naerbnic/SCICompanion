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
#include "Components/Audio.h"

// http://wiki.multimedia.cx/index.php?title=Sierra_Audio
#include <pshpack1.h>
struct AudioHeader
{
    uint8_t resourceType;
    uint8_t headerSize;
    uint32_t audioType; // SOL
    uint16_t sampleRate;
    AudioFlags flags;
    // REVIEW this is actually 16 bits:
    int32_t sizeExcludingHeader;
};
#include <poppack.h>

std::unique_ptr<ResourceEntityFactory> CreateAudioResourceFactory();
std::unique_ptr<ResourceEntityFactory> CreateWaveAudioResourceFactory();
uint32_t AudioEstimateSize(const ResourceEntity &resource);
std::string GetAudioLength(const AudioComponent &audio);