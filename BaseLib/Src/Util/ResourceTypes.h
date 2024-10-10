#pragma once

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