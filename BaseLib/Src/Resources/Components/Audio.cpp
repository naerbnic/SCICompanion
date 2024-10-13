#include "Components/Audio.h"

#include <algorithm>

#include "Components.h"
#include "SciConstants.h"


uint32_t AudioComponent::GetLengthInTicks() const
{
    int bytePerSecond = std::max(1, static_cast<int>(Frequency));
    if (IsFlagSet(Flags, AudioFlags::SixteenBit))
    {
        bytePerSecond *= 2;
    }
    return SCITicksPerSecond * DigitalSamplePCM.size() / bytePerSecond;
}

uint32_t AudioComponent::GetBytesPerSecond() const
{
    return Frequency * (IsFlagSet(Flags, AudioFlags::SixteenBit) ? 2 : 1);
}


void AudioComponent::ScanForClipped()
{
    IsClipped = false;
    if (IsFlagSet(Flags, AudioFlags::SixteenBit))
    {
        if (!DigitalSamplePCM.empty())
        {
            auto data = reinterpret_cast<const int16_t*>(DigitalSamplePCM.data());
            size_t samples = DigitalSamplePCM.size() / 2;
            for (size_t i = 0; i < samples; i++)
            {
                int16_t value = data[i];
                if ((value == -32768) || (value == 32767))
                {
                    IsClipped = true;
                    return;
                }
            }
        }
    }
    else
    {
        for (uint8_t value : DigitalSamplePCM)
        {
            if ((value == 0) || (value == 255))
            {
                IsClipped = true;
                return;
            }
        }
    }
}