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

#include <cstdint>

struct NoiseSettings
{
    uint32_t AttackTimeMS;
    uint32_t ReleaseTimeMS;
    uint32_t HoldTimeMS;
    int32_t OpenThresholdDB;
    int32_t CloseThresholdDB;
};

struct AudioProcessingSettings
{
    AudioProcessingSettings() : 
        Version(0),
        TrimLeftMS(0),
        TrimRightMS(0),
        AutoGain(false),
        DetectStartEnd(false),
        AudioDither(false),
        Compression(false),
        Noise()
    {
    }

    uint32_t Version;

    uint32_t TrimLeftMS;
    uint32_t TrimRightMS;
    bool AutoGain;
    bool DetectStartEnd;
    bool AudioDither;
    bool Compression;

    // Noise settings
    NoiseSettings Noise;
};