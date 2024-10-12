#pragma once

#include <cstdint>

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

enum class ResourceTypeFlags
{
    None = 0,
    View = 1 << (int)ResourceType::View,
    Pic = 1 << (int)ResourceType::Pic,
    Script = 1 << (int)ResourceType::Script,
    Text = 1 << (int)ResourceType::Text,
    Sound = 1 << (int)ResourceType::Sound,
    Memory = 1 << (int)ResourceType::Memory,
    Vocab = 1 << (int)ResourceType::Vocab,
    Font = 1 << (int)ResourceType::Font,
    Cursor = 1 << (int)ResourceType::Cursor,
    Patch = 1 << (int)ResourceType::Patch,
    Bitmap = 1 << (int)ResourceType::Bitmap,
    Palette = 1 << (int)ResourceType::Palette,
    CDAudio = 1 << (int)ResourceType::CDAudio,
    Audio = 1 << (int)ResourceType::Audio,
    // NOTE: Sync resources are included in Audio resources.
    // Sync = 1 << (int)ResourceType::Sync,
    Message = 1 << (int)ResourceType::Message,
    AudioMap = 1 << (int)ResourceType::AudioMap,
    Heap = 1 << (int)ResourceType::Heap,

    All = 0x3fffffff,

    AllCreatable = View | Font | Cursor | Text | Sound | Vocab | Pic | Palette | Message | Audio | AudioMap,
};
DEFINE_ENUM_FLAGS(ResourceTypeFlags, uint32_t)

ResourceTypeFlags ResourceTypeToFlag(ResourceType dwType);
ResourceType ResourceFlagToType(ResourceTypeFlags dwType);

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

enum class ResourceSaveLocation : uint16_t
{
    Default,
    Package,
    Patch,
};

class IResourceIdentifier
{
public:
    virtual ~IResourceIdentifier() = default;

    virtual int GetPackageHint() const = 0;
    virtual int GetNumber() const = 0;
    virtual ResourceType GetType() const = 0;
    virtual int GetChecksum() const = 0;
    virtual uint32_t GetBase36() const = 0;
};

static const uint32_t NoBase36 = 0xffffffff;
static const int NumResourceTypes = 18;
