#pragma once

#include <cstdint>
#include <optional>

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

inline constexpr int NumResourceTypes = static_cast<int>(ResourceType::Max);

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

inline constexpr uint32_t NoBase36 = 0xffffffff;

class ResourceNum
{
public:
    static ResourceNum FromNumber(int resource_number)
    {
        return ResourceNum(resource_number);
    }

    static ResourceNum WithBase36(int resource_number, std::optional<uint32_t> base36_number)
    {
        if (!base36_number.has_value() || base36_number == NoBase36)
        {
            return ResourceNum(resource_number);
        }
        return ResourceNum(resource_number, base36_number);
    }

    ResourceNum() = default;

    int GetNumber() const
    {
        return resource_number_;
    }

    uint32_t GetBase36() const
    {
        return base36_number_.value_or(NoBase36);
    }

    std::optional<uint32_t> GetBase36Opt() const
    {
        return base36_number_;
    }

private:
    explicit ResourceNum(int resource_number, std::optional<uint32_t> base36_number = std::nullopt)
        : resource_number_(resource_number), base36_number_(base36_number)
    {
    }

    int resource_number_;
    std::optional<uint32_t> base36_number_;
};

// A general resource ID. This uniquely determines the resource within a game. This gives no indication on how to
// locate the resource, only the unique identifier.
class ResourceId
{
public:
    ResourceId() = default;

    ResourceId(ResourceType resource_type, const ResourceNum& resource_num)
        : resource_type_(resource_type), resource_num_(resource_num)
    {
    }

    ResourceType GetType() const
    {
        return resource_type_;
    }

    const ResourceNum& GetResourceNum() const
    {
        return resource_num_;
    }

    int GetNumber() const
    {
        return resource_num_.GetNumber();
    }

    uint32_t GetBase36() const
    {
        return resource_num_.GetBase36();
    }

    std::optional<uint32_t> GetBase36Opt() const
    {
        return resource_num_.GetBase36Opt();
    }

private:
    ResourceType resource_type_;
    ResourceNum resource_num_;
};

// A description of the location of a resource in the final game. This includes it's ID, and the package it is located
// in.
class ResourceLocation
{
public:
    ResourceLocation() = default;

    ResourceLocation(int package_hint, const ResourceId& resource_id)
        : package_hint_(package_hint), resource_id_(resource_id)
    {
    }

    ResourceType GetType() const
    {
        return resource_id_.GetType();
    }

    int GetNumber() const
    {
        return resource_id_.GetResourceNum().GetNumber();
    }

    uint32_t GetBase36() const
    {
        return resource_id_.GetResourceNum().GetBase36();
    }

    std::optional<uint32_t> GetBase36Opt() const
    {
        return resource_id_.GetResourceNum().GetBase36Opt();
    }

    int GetPackageHint() const
    {
        return package_hint_;
    }

    const ResourceId& GetResourceId() const
    {
        return resource_id_;
    }

private:
    int package_hint_;
    ResourceId resource_id_;
};

// A resource identifier that specifies a particular resource within a package.
//
// This includes its resource ID, which package it should be contained in, and a checksum to verify the resource.
class ResourceDescriptor
{
public:
    ResourceDescriptor() = default;

    ResourceDescriptor(const ResourceLocation& resource_location, int checksum)
        : resource_location_(resource_location), checksum_(checksum)
    {
    }

    ResourceType GetType() const
    {
        return resource_location_.GetType();
    }

    int GetNumber() const
    {
        return GetResourceId().GetResourceNum().GetNumber();
    }

    uint32_t GetBase36() const
    {
        return GetResourceId().GetResourceNum().GetBase36();
    }

    std::optional<uint32_t> GetBase36Opt() const
    {
        return GetResourceId().GetResourceNum().GetBase36Opt();
    }

    ResourceLocation GetResourceLocation() const
    {
        return resource_location_;
    }

    int GetPackageHint() const
    {
        return resource_location_.GetPackageHint();
    }

    const ResourceId& GetResourceId() const
    {
        return resource_location_.GetResourceId();
    }

    int GetChecksum() const
    {
        return checksum_;
    }

private:
    ResourceLocation resource_location_;
    int checksum_;
};

class IResourceIdentifier
{
public:
    virtual ~IResourceIdentifier() = default;

    virtual ResourceDescriptor GetResourceDescriptor() const = 0;
    int GetPackageHint() const
    {
        return GetResourceLocation().GetPackageHint();
    }

    int GetNumber() const
    {
        return GetResourceNum().GetNumber();
    }

    uint32_t GetBase36() const
    {
        return GetResourceNum().GetBase36();
    }

    ResourceType GetType() const
    {
        return GetResourceId().GetType();
    }

    int GetChecksum() const
    {
        return GetResourceDescriptor().GetChecksum();
    }

    ResourceNum GetResourceNum() const
    {
        return GetResourceId().GetResourceNum();
    }

    ResourceId GetResourceId() const
    {
        return GetResourceLocation().GetResourceId();
    }

    ResourceLocation GetResourceLocation() const
    {
        return GetResourceDescriptor().GetResourceLocation();
    }
};
