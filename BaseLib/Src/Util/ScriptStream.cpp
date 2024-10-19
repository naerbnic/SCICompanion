#include "ScriptStream.h"

#include <cassert>

CCrystalScriptStream::CCrystalScriptStream(ScriptStreamLineSource* pLimiter)
{
    _pLimiter = pLimiter;
}


CCrystalScriptStream::const_iterator::const_iterator(ScriptStreamLineSource* limiter, LineCol dwPos) : _limiter(limiter)
{
    _nLine = dwPos.Line();
    _nChar = dwPos.Column();
    if (_nLine < _limiter->GetLineCount())
    {
        _nLength = _limiter->GetLineLength(_nLine);
        _pszLine = _limiter->GetLineChars(_nLine);
    }
    else
    {
        _nLength = 0;
    }
}

char CCrystalScriptStream::const_iterator::operator*()
{
    return GetChar();
}

char CCrystalScriptStream::const_iterator::GetChar() const
{
    return (_nChar == _nLength) ? '\n' : _pszLine[_nChar];
}

std::string CCrystalScriptStream::const_iterator::GetLookAhead(int nChars)
{
    const char* pszLine = _limiter->GetLineChars(_nLine);
    std::size_t cLineChars = _limiter->GetLineLength(_nLine);
    std::string text;
    while (pszLine && (_nChar < cLineChars) && pszLine[_nChar] && (nChars > 0))
    {
        text += pszLine[_nChar++];
        --nChars;
    }
    return text;
}

int CCrystalScriptStream::const_iterator::CountPosition(int tabSize) const
{
    int visualPosition = 0;
    std::size_t charPos = 0;
    while (charPos < _nChar)
    {
        if (_pszLine[charPos] == '\t')
        {
            // Go to the next tab stop
            int nextStop = (visualPosition + tabSize) / tabSize * tabSize;
            visualPosition = nextStop;
        }
        else
        {
            visualPosition++;
        }
        charPos++;
    }
    return visualPosition;
}

void CCrystalScriptStream::const_iterator::Advance()
{
    assert((_pszLine == nullptr) || (*_pszLine != 0)); // EOF
    _nChar++;
    if ((_nChar > _nLength) ||
        ((_nChar >= _nLength) && (_nLine == (_limiter->GetLineCount() - 1)))) // for == we use '\n', unless this is the last line
    {
        if (_limiter->TryGetMoreData()) {
            _nLength = _limiter->GetLineLength(_nLine);
            _pszLine = _limiter->GetLineChars(_nLine);
        }
    }


    while (_nChar > _nLength)
    {
        // Need to move to the next line
        std::size_t cLines = _limiter->GetLineCount();
        while (_nLine < cLines)
        {
            _nLine++;

            //OutputDebugString(fmt::format("Moved to line {0} of {1}\n", nLine, cLines).c_str());

            if (_nLine < cLines)
            {
                _nLength = _limiter->GetLineLength(_nLine);
                _pszLine = _limiter->GetLineChars(_nLine);
                assert(_pszLine != nullptr || _nLength == 0);
                _nChar = 0;
                break; // done
            }
        }
        if (_nLine == cLines)
        {
            // We ran out of data.
            _pszLine = "\0"; // EOF
            _nLength = 1;
            _nChar = 0;
        }
        // else we're all good.
    }
}

bool CCrystalScriptStream::const_iterator::AtEnd() const
{
    return GetChar() == '\0';
    //return _pszLine == nullptr || *_pszLine == '\0' || (_nChar == _nLength && _nLine == (_limiter->GetLineCount() - 1));
}

char CCrystalScriptStream::const_iterator::AdvanceAndGetChar()
{
    Advance();
    return GetChar();
}

int CCrystalScriptStream::const_iterator::Compare(const const_iterator& other) const
{
    if (AtEnd())
    {
        return other.AtEnd() ? 0 : 1;
    }

    if (other.AtEnd())
    {
        return -1;
    }

    if (_nLine != other._nLine)
    {
        return _nLine < other._nLine ? -1 : 1;
    }

    if (_nChar != other._nChar)
    {
        return _nChar < other._nChar ? -1 : 1;
    }

    return 0;
}

CCrystalScriptStream::const_iterator& CCrystalScriptStream::const_iterator::operator++()
{
    Advance();
    return *this;
}

bool CCrystalScriptStream::const_iterator::operator<(const const_iterator& other) const
{
    return Compare(other) < 0;
}

std::string CCrystalScriptStream::const_iterator::tostring() const
{
    char sz[50];
    sprintf_s(sz, sizeof(sz), "Line %d, column %d", _nLine + 1, _nChar);
    return sz;
}

bool CCrystalScriptStream::const_iterator::operator==(const const_iterator& other) const
{
    return Compare(other) == 0;
}

bool CCrystalScriptStream::const_iterator::operator!=(const const_iterator& other) const
{
    return Compare(other) != 0;
}

void CCrystalScriptStream::const_iterator::ResetLine()
{
    _nChar = 0;
}

void CCrystalScriptStream::const_iterator::Restore(const const_iterator& prev)
{
    // This is a hack for autocomplete. Ideally, we should retrieve line length each time.
    // If on the same line, keep the furthest length, since it may have been modified by AC.
    int furthestLength = (prev._nLine == _nLine) ? _nLength : prev._nLength;
    (*this) = prev;
    _nLength = furthestLength;
}

LineCol CCrystalScriptStream::const_iterator::GetPosition() const
{
    return LineCol(_nLine, _nChar);
}
int CCrystalScriptStream::const_iterator::GetLineNumber() const
{
    return _nLine;
}
int CCrystalScriptStream::const_iterator::GetColumnNumber() const
{
    return _nChar;
}
