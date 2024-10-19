#include "ScriptStream.h"

#include <cassert>

ScriptStream::ScriptStream(ScriptStreamLineSource* pLimiter)
{
    _pLimiter = pLimiter;
}


ScriptStreamIterator::ScriptStreamIterator(ScriptStreamLineSource* limiter, LineCol dwPos) : _limiter(limiter)
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

char ScriptStreamIterator::operator*()
{
    return GetChar();
}

char ScriptStreamIterator::GetChar() const
{
    return (_nChar == _nLength) ? '\n' : _pszLine[_nChar];
}

std::string ScriptStreamIterator::GetLookAhead(int nChars)
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

int ScriptStreamIterator::CountPosition(int tabSize) const
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

void ScriptStreamIterator::Advance()
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

bool ScriptStreamIterator::AtEnd() const
{
    return GetChar() == '\0';
    //return _pszLine == nullptr || *_pszLine == '\0' || (_nChar == _nLength && _nLine == (_limiter->GetLineCount() - 1));
}

char ScriptStreamIterator::AdvanceAndGetChar()
{
    Advance();
    return GetChar();
}

int ScriptStreamIterator::Compare(const ScriptStreamIterator& other) const
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

ScriptStreamIterator& ScriptStreamIterator::operator++()
{
    Advance();
    return *this;
}

bool ScriptStreamIterator::operator<(const ScriptStreamIterator& other) const
{
    return Compare(other) < 0;
}

std::string ScriptStreamIterator::tostring() const
{
    char sz[50];
    sprintf_s(sz, sizeof(sz), "Line %d, column %d", _nLine + 1, _nChar);
    return sz;
}

bool ScriptStreamIterator::operator==(const ScriptStreamIterator& other) const
{
    return Compare(other) == 0;
}

bool ScriptStreamIterator::operator!=(const ScriptStreamIterator& other) const
{
    return Compare(other) != 0;
}

void ScriptStreamIterator::ResetLine()
{
    _nChar = 0;
}

void ScriptStreamIterator::Restore(const ScriptStreamIterator& prev)
{
    // This is a hack for autocomplete. Ideally, we should retrieve line length each time.
    // If on the same line, keep the furthest length, since it may have been modified by AC.
    int furthestLength = (prev._nLine == _nLine) ? _nLength : prev._nLength;
    (*this) = prev;
    _nLength = furthestLength;
}

LineCol ScriptStreamIterator::GetPosition() const
{
    return LineCol(_nLine, _nChar);
}
int ScriptStreamIterator::GetLineNumber() const
{
    return _nLine;
}
int ScriptStreamIterator::GetColumnNumber() const
{
    return _nChar;
}
