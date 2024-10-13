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
#include "SoundOperations.h"
#include "format.h"

std::string GetSoundLength(const SoundComponent &sound)
{
    DWORD ticks = sound.GetTotalTicks();
    uint16_t tempo = max(1, sound.GetTempo());
    int seconds = (int)round((double)(ticks * 30) / (double)(60 * SCI_PPQN));
    return fmt::format("{0:02}:{1:02}", seconds / 60, seconds % 60);
}
