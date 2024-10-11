#include "PicCommandsCommon.h"

bool operator==(const PenStyle& one, const PenStyle& two)
{
    return one.fPattern == two.fPattern &&
        one.fRectangle == two.fRectangle &&
        one.fRandomNR == two.fRandomNR &&
        one.bPatternSize == two.bPatternSize &&
        one.bPatternNR == two.bPatternNR;
}

bool operator!=(const PenStyle& one, const PenStyle& two)
{
    return !(one == two);
}

//
// Helpers for converting an arbitrary index to and from pattern info.
// The arbitrary index is used for the position of a square in a dialog that
// represents the different types of patterns.
//
void PatternInfoFromIndex(uint8_t bIndex, PenStyle* pPenStyle)
{
    // Pattern size resets every 8.
    pPenStyle->bPatternSize = bIndex % 8;
    // fPattern alterates every 8, from solid->pattern->solid->pattern
    pPenStyle->fPattern = ((bIndex / 8) % 2) != 0;
    pPenStyle->bPatternNR = 0;
    // fRectangle is true for >= 16
    pPenStyle->fRectangle = (bIndex / 16) != 0;
}

uint8_t IndexFromPatternInfo(const PenStyle* pPenStyle)
{
    uint8_t bIndex = 0;
    if (pPenStyle->fRectangle)
    {
        bIndex += 16;
    }
    if (pPenStyle->fPattern)
    {
        bIndex += 8;
    }
    bIndex += pPenStyle->bPatternSize;
    return bIndex;
}