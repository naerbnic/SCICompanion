#pragma once

#include "EnumFlags.h"

//
// Resource types
// The numbers here are important, they map to the
// actual values that SCI uses, and are used for array lookups.
//
enum class ResourceType
{
    None = -1,
    View = 0,
    Pic = 1,
    Script = 2,
    Text = 3,
    Sound = 4,
    Memory = 5,
    Vocab = 6,
    Font = 7,
    Cursor = 8,
    Patch = 9,
    // SCI1 and beyond....
    Bitmap = 10,
    Palette = 11,
    CDAudio = 12,
    Audio = 13,
    Sync = 14,          // Included with Audio resources, not a separate type
    Message = 15,
    AudioMap = 16,
    Heap = 17,

    Max = 18
};

enum class ResourceSourceFlags
{
    Invalid = 0,
    MessageMap = 0x0001,
    PatchFile = 0x0002,
    Aud = 0x0004,
    Sfx = 0x0008,
    AudioCache = 0x0010,    // Our special audio folder
    AudioMapCache = 0x0020, // 
    AltMap = 0x0040,
    ResourceMap = 0x0080,
};

enum class ResourceLoadStatusFlags : uint8_t
{
    None = 0x00,
    DecompressionFailed = 0x01,
    ResourceCreationFailed = 0x02,
    Corrupted = 0x04,
    Delayed = 0x08,
};

enum class BlobKey
{
    LipSyncDataSize,
};

DEFINE_ENUM_FLAGS(ResourceLoadStatusFlags, uint8_t)