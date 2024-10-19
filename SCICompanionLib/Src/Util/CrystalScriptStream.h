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
#pragma once

#include "CCrystalTextBuffer.h"
#include "CompileInterfaces.h"
#include "ScriptStream.h"

class ISyntaxParserCallback
{
public:
    // Returns true if we should continue, or false if we should bail
    virtual bool Done() = 0;
};

class ReadOnlyTextBuffer
{
public:
    ReadOnlyTextBuffer(CCrystalTextBuffer *pBuffer);
    ReadOnlyTextBuffer(CCrystalTextBuffer *pBuffer, CPoint limit, int extraSpace);

    int GetLineCount() { return _lineCount; }
    int GetLineLength(int nLine);
    PCTSTR GetLineChars(int nLine);
    CPoint GetLimit() { return _limit; }
    void Extend(const std::string &extraChars);

private:
    struct StartAndLength
    {
        int Start;
        int Length;
    };

    int _lineCount;
    std::unique_ptr<StartAndLength[]> _lineStartsAndLengths;
    std::vector<char> _text;
    int _extraSpace;
    CPoint _limit;
};

//
// Limits the text buffer to a particular line/char
// (used for autocomplete and tooltips)
//
class CScriptStreamLimiter : public ScriptStreamLineSource
{
public:
    CScriptStreamLimiter(CCrystalTextBuffer *pBuffer);
    CScriptStreamLimiter(CCrystalTextBuffer *pBuffer, CPoint ptLimit, int extraSpace);

    // ReSharper disable once CppMemberFunctionMayBeConst
    //
    // Modifies the internal buffer.
    void Extend(const std::string &extraChars) { _pBuffer->Extend(extraChars); }

    CPoint GetLimit() const
    {
        return _pBuffer->GetLimit();
    }

    void SetCallback(ISyntaxParserCallback *pCallback)
    {
        _pCallback = pCallback;
    }

    // Reflect some methods on CCrystalTextBuffer:
	std::size_t GetLineCount() const override
    {
        return _pBuffer->GetLineCount();
    }

	std::size_t GetLineLength(int nLine) const override
    {
        return _pBuffer->GetLineLength(nLine);
    }

    LPCTSTR GetLineChars(int nLine) const override { return _pBuffer->GetLineChars(nLine); }
    bool TryGetMoreData() override;
    std::string GetLastWord();

private:
    std::unique_ptr<ReadOnlyTextBuffer> _pBuffer;
    bool _fCancel;

    // Or tooltips
    ISyntaxParserCallback *_pCallback;
};


