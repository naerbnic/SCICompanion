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

#include "Components/Audio.h"
#include "AudioProcessingSettings.h"
#include "ResourceEntity.h"
#include "ResourceTypes.h"
#include "Version.h"

extern const int MaxSierraSampleRate;

void AudioComponentFromWaveFile(sci::istream stream, AudioComponent &audio, AudioProcessingSettings *audioProcessingSettings = nullptr, int maxSampleRate = MaxSierraSampleRate, bool limitTo8Bit = false);
std::unique_ptr<ResourceEntity> WaveResourceFromFilename(const std::string &filename);
std::string _NameFromFilename(PCSTR pszFilename);
AudioVolumeName GetVolumeToUse(SCIVersion version, uint32_t base36Number);
std::string GetAudioVolumePath(const std::string &gameFolder, bool bak, AudioVolumeName volumeToUse, ResourceSourceFlags *sourceFlags = nullptr);
bool IsWaveFile(PCSTR pszFileName);
void WriteWaveFile(const std::string &filename, const AudioComponent &audio, const AudioProcessingSettings *audioProcessingSettings = nullptr);
bool HasWaveHeader(const std::string &filename);
uint32_t GetWaveFileSizeIncludingHeader(sci::istream &stream);