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
//
// Pic Command classes
//
#pragma once

#include <cstdint>

// Pic opcodes
const uint8_t PIC_OP_SET_COLOR = 0xf0;
const uint8_t  PIC_OP_DISABLE_VISUAL = 0xf1;
const uint8_t  PIC_OP_SET_PRIORITY = 0xf2;
const uint8_t  PIC_OP_DISABLE_PRIORITY = 0xf3;
const uint8_t  PIC_OP_RELATIVE_PATTERNS = 0xf4;
const uint8_t  PIC_OP_RELATIVE_MEDIUM_LINES = 0xf5;
const uint8_t  PIC_OP_RELATIVE_LONG_LINES = 0xf6;
const uint8_t  PIC_OP_RELATIVE_SHORT_LINES = 0xf7;
const uint8_t  PIC_OP_FILL = 0xf8;
const uint8_t  PIC_OP_SET_PATTERN = 0xf9;
const uint8_t  PIC_OP_ABSOLUTE_PATTERNS = 0xfa;
const uint8_t  PIC_OP_SET_CONTROL = 0xfb;
const uint8_t  PIC_OP_DISABLE_CONTROL = 0xfc;
const uint8_t  PIC_OP_RELATIVE_MEDIUM_PATTERNS = 0xfd;
const uint8_t  PIC_OP_OPX = 0xfe;
const uint8_t  PIC_END = 0xff;

const uint8_t  PIC_OPX_SET_PALETTE_ENTRY = 0x00;
const uint8_t  PIC_OPX_SET_PALETTE = 0x01;

const uint8_t  PIC_OPXSC1_DRAW_BITMAP = 0x01;
const uint8_t  PIC_OPXSC1_SET_PALETTE = 0x02;
const uint8_t  PIC_OPXSC1_SET_PRIORITY_BARS = 0x04;

struct PenStyle
{
    PenStyle() : bPatternSize(0), bPatternNR(0), fRandomNR(true), fRectangle(false), fPattern(false) {}
    bool fPattern;
    bool fRectangle;
    bool fRandomNR;
    uint8_t bPatternSize;
    uint8_t bPatternNR;
};

bool operator==(const PenStyle &one, const PenStyle &two);
bool operator!=(const PenStyle &one, const PenStyle &two);

void PatternInfoFromIndex(uint8_t bIndex, PenStyle *pPenStyle);
uint8_t IndexFromPatternInfo(const PenStyle *pPenStyle);

