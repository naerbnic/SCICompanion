#pragma once

#include "Components.h"

// We re-use TextComponent

enum class MessageChangeHint
{
    None = 0,
    ItemChanged = 0x00000040,           // Something about a particular item changed
    Selection = 0x00000080,         // Current selection changed
    Changed = 0x00000100,           // The resource overall changed (e.g. items added or removed)
    NounsChanged = 0x00000200,
    ConditionsChanged = 0x00000400,
    VerbsChanged = 0x00000800,
    TalkersChanged = 0x00001000,
    AllMessageFiles = 0x00001e00
};

DEFINE_ENUM_FLAGS(MessageChangeHint, uint32_t)

ResourceEntity *CreateMessageResource(SCIVersion version);
ResourceEntity *CreateDefaultMessageResource(SCIVersion version);