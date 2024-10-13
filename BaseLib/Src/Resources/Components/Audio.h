#pragma once

#include <vector>

#include "EnumFlags.h"
#include "Components.h"

enum class AudioFlags : uint8_t
{
    // The values here appear to be wrong: http://wiki.multimedia.cx/index.php?title=Sierra_Audio
    // This has the correct values: https://github.com/scummvm/scummvm/blob/master/engines/sci/sound/audio.cpp
    //Stereo = 0x04,      // Not supported
    //SixteenBit = 0x10,  // Not supported
    None = 0x00,
    DPCM = 0x01,
    Unknown = 0x02,
    SixteenBit = 0x04,
    Signed = 0x08,
};
DEFINE_ENUM_FLAGS(AudioFlags, uint8_t)

struct AudioComponent : public ResourceComponent
{
public:
    AudioComponent() : Frequency(0), Flags(AudioFlags::None), IsClipped(false) {}

    AudioComponent(const AudioComponent& src) = default;
    AudioComponent& operator=(const AudioComponent& src) = default;

    ResourceComponent* Clone() const override
    {
        return new AudioComponent(*this);
    }

    uint32_t GetLength() const { return (uint32_t)DigitalSamplePCM.size(); }
    uint32_t GetLengthInTicks() const;
    uint32_t GetBytesPerSecond() const;

    void ScanForClipped();

    std::vector<uint8_t> DigitalSamplePCM;
    uint16_t Frequency; // Samples per second
    AudioFlags Flags;
    bool IsClipped;
};