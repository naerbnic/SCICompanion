/***************************************************************************
Copyright (c) 2017 Philip Fortier

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
#include "Components/Sound.h"

void PopulateComboWithDevicesHelper(SCIVersion version, CComboBox &combo);
DeviceType GetDeviceFromComboHelper(SCIVersion version, CComboBox &combo);
void SelectDeviceInComboHelper(SCIVersion version, CComboBox &combo, DeviceType);
void SelectFirstDeviceWithChannels(SCIVersion version, CComboBox &combo, const SoundComponent &sound);
// Required here as it depends on AppState
std::unique_ptr<ResourceEntity> WaveResourceFromFilename(const std::string& filename);
