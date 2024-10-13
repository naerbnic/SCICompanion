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
#include "Sound.h"
#include "ResourceEntity.h"
#include "format.h"
#include "Components/Sound.h"

#pragma comment( lib, "winmm.lib" )

using namespace std;

SoundEvent::Command _GetSoundCommand(uint8_t bStatus)
{
    return (SoundEvent::Command)(bStatus & 0xF0);
}

uint8_t _GetChannel(uint8_t bStatus)
{
    return bStatus & 0x0F; // Lower nibble
}

void SoundWriteTo_SCI1(const ResourceEntity &resource, sci::ostream &byteStream, std::map<BlobKey, uint32_t> &propertyBag)
{
    const SoundComponent &sound = resource.GetComponent<SoundComponent>();
    SoundWriteToWorker_SCI1(sound, resource.TryGetComponent<AudioComponent>(), byteStream);
}

bool ValidateSoundResource(const ResourceEntity &resource)
{
    bool save = true;
    const SoundComponent &sound = resource.GetComponent<SoundComponent>();
    
    if (!sound.GetCuePoints().empty() && (sound.GetLoopPoint() != SoundComponent::LoopPointNone))
    {
        save = (IDYES == AfxMessageBox("Sound contains a loop point and at least one cue. Cues won't do anything if a loop point is specified. Save anyway?", MB_ICONWARNING | MB_YESNO));
    }

    if (save)
    {
        for (const auto &cuePoint : sound.GetCuePoints())
        {
            if ((cuePoint.GetTickPos() != 0) && (cuePoint.GetValue() == 0))
            {
                save = (IDYES == AfxMessageBox("Sound contains a cumulative cue with a zero value after time 0. This won't trigger anything. Save anyway?", MB_ICONWARNING | MB_YESNO));
                break;
            }
        }
    }

    if (save)
    {
        if (sound.GetTrackInfos().empty())
        {
            save = (IDYES == AfxMessageBox("No channels have been enabled for any sound devices, so no music will play. Save anyway?", MB_ICONWARNING | MB_YESNO));
        }
    }

    if (save && resource.TryGetComponent<AudioComponent>())
    {
        if (!std::any_of(sound.GetTrackInfos().begin(), sound.GetTrackInfos().end(), [](const TrackInfo &track) { return track.HasDigital; }))
        {
            save = (IDYES == AfxMessageBox("This sound has a digital channel but it is not used by any devices. It will be lost on saving. Save anyway?", MB_ICONWARNING | MB_YESNO));
        }
    }

    return save;
}

ResourceTraits soundResTraits =
{
    ResourceType::Sound,
    &SoundReadFrom_SCI0,
    &SoundWriteTo,
    &ValidateSoundResource,
    nullptr
};

ResourceTraits soundResTraitsSCI1 =
{
    ResourceType::Sound,
    &SoundReadFrom_SCI1,
    &SoundWriteTo_SCI1,
    &ValidateSoundResource,
    nullptr
};

class SoundResourceFactory : public ResourceEntityFactory
{
public:
    std::unique_ptr<ResourceEntity> CreateResource(
        const SCIVersion& version) const override
    {
        SoundTraits* ptraits = &soundTraitsSCI0;
        if (version.SoundFormat == SoundFormat::SCI1)
        {
            ptraits = &soundTraitsSCI1;
        }
        std::unique_ptr<ResourceEntity> pResource = std::make_unique<ResourceEntity>((version.SoundFormat == SoundFormat::SCI0) ? soundResTraits : soundResTraitsSCI1);
        pResource->AddComponent(move(make_unique<SoundComponent>(*ptraits)));
        return pResource;
    }
};

std::unique_ptr<ResourceEntityFactory> CreateSoundResourceFactory()
{
    return std::make_unique<SoundResourceFactory>();
}