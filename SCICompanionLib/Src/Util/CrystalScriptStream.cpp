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
#include "CrystalScriptStream.h"
#include "format.h"

// Limit: y is inclusize, x is exclusive.
// e.g. a limit of (1, 2) would mean that we can read all of line 0, and 2 characters of line 1.

CPoint GetNaturalLimit(CCrystalTextBuffer *pBuffer)
{
    CPoint limit;
    limit.y = pBuffer->GetLineCount() - 1;
    if (limit.y >= 0)
    {
        limit.x = pBuffer->GetLineLength(limit.y);
    }
    return limit;
}

ReadOnlyTextBuffer::ReadOnlyTextBuffer(CCrystalTextBuffer *pBuffer) : ReadOnlyTextBuffer(pBuffer, GetNaturalLimit(pBuffer), 0) {}

ReadOnlyTextBuffer::ReadOnlyTextBuffer(CCrystalTextBuffer *pBuffer, CPoint limit, int extraSpace)
{
    _limit = limit;
    _extraSpace = extraSpace;

    int lineCountMinusOne = limit.y;
    _lineCount = lineCountMinusOne + 1;
    int totalCharCount = 0;
    for (int i = 0; i < lineCountMinusOne; i++)
    {
        int charCount = pBuffer->GetLineLength(i);
        totalCharCount += charCount;
    }
    totalCharCount += limit.x;

    int start = 0;
    _text.reserve(totalCharCount + extraSpace);
    _lineStartsAndLengths = std::make_unique<StartAndLength[]>(_lineCount);
    for (int i = 0; i < lineCountMinusOne; i++)
    {
        int charCount = pBuffer->GetLineLength(i);
        std::copy(pBuffer->GetLineChars(i), pBuffer->GetLineChars(i) + charCount, std::back_inserter(_text));
        _lineStartsAndLengths[i].Start = start;
        _lineStartsAndLengths[i].Length = charCount;
        start += charCount;
    }
    assert(pBuffer->GetLineLength(lineCountMinusOne) >= limit.x);
    std::copy(pBuffer->GetLineChars(lineCountMinusOne), pBuffer->GetLineChars(lineCountMinusOne) + limit.x, std::back_inserter(_text));
    _lineStartsAndLengths[lineCountMinusOne].Start = start;
    _lineStartsAndLengths[lineCountMinusOne].Length = limit.x;
}

void ReadOnlyTextBuffer::Extend(const std::string &extraChars)
{
    if ((int)extraChars.length() < _extraSpace)
    {
        std::copy(extraChars.begin(), extraChars.end(), std::back_inserter(_text));
        _lineStartsAndLengths[_lineCount - 1].Length += extraChars.length();
        _extraSpace -= extraChars.length();
        _limit.x += extraChars.length();
    }
}

int ReadOnlyTextBuffer::GetLineLength(int nLine)
{
    return _lineStartsAndLengths[nLine].Length;
}
PCTSTR ReadOnlyTextBuffer::GetLineChars(int nLine)
{
    if (_lineStartsAndLengths[nLine].Length > 0)
    {
        return &_text[_lineStartsAndLengths[nLine].Start];
    }
    return nullptr;
}

CScriptStreamLimiter::CScriptStreamLimiter(CCrystalTextBuffer *pBuffer)
{
    _pBuffer = std::make_unique<ReadOnlyTextBuffer>(pBuffer);
    _pCallback = nullptr;
    _fCancel = false;
}

CScriptStreamLimiter::CScriptStreamLimiter(CCrystalTextBuffer *pBuffer, CPoint ptLimit, int extraSpace)
{
    _pBuffer = std::make_unique<ReadOnlyTextBuffer>(pBuffer, ptLimit, extraSpace);
    _pCallback = nullptr;
    _fCancel = false;
}

// CCrystalScriptStream

// For autocomplete
std::string CScriptStreamLimiter::GetLastWord()
{
    CPoint pt = _pBuffer->GetLimit();
    PCSTR pszLine = _pBuffer->GetLineChars(pt.y);
    int nChar = pt.x - 1;
    std::string word;
    while (nChar > 0)
    {
        uint8_t theChar = (uint8_t)pszLine[nChar];
        // & and - are included for Sierra syntax (&tmp, &rest, -info-). Hopefully this doesn't mess up Studio autocomplete.
        if (isalnum(theChar) || (theChar == '_') || (theChar == '&') || (theChar == '-'))
        {
            word += theChar;
        }
        else
        {
            break;
        }
        nChar--;
    }
    std::reverse(word.begin(), word.end());
    return word;
}

// Ensure we are on the line with nChar and nLine.
bool CScriptStreamLimiter::TryGetMoreData()
{
    if (!_pCallback)
    {
        return false;
    }
    return !_pCallback->Done();
}
