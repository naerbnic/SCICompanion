#include "ParserStream.h"

#include <cassert>

#include "DataBuffer.h"


class CScriptStreamLimiter;

namespace
{
    struct CharRange
    {
        std::size_t start;
        std::size_t end;
    };
    std::string_view DataAsStringView(absl::Span<const uint8_t> data)
    {
        return std::string_view(reinterpret_cast<const char*>(data.data()), data.size());
    }
    // Returns the range of characters around the next newline character(s). Returns
    // an empty optional if no newline is found.
    std::optional<CharRange> FindNextNewline(absl::Span<const uint8_t> data, std::size_t start)
    {
        auto text_data = DataAsStringView(data);
        // Search for platfrom_specific combinations.
        auto next_char = text_data.find_first_of("\r\n", start);
        if (next_char == std::string_view::npos)
        {
            return std::nullopt;
        }
        auto range_start = next_char;
        std::size_t range_end;
        if (text_data[next_char] == '\r' && next_char + 1 < text_data.size() && text_data[next_char + 1] == '\n')
        {
            range_end = range_start + 2;
        }
        else
        {
            range_end = range_start + 1;
        }

        return CharRange{range_start, range_end };
    }
}

class ParserBufferImpl
{
public:
    explicit ParserBufferImpl(sci::MemoryBuffer buffer) : _buffer(std::move(buffer))
    {
        auto data_span = _buffer.GetAllData();

        std::size_t line_start = 0;

        while (true)
        {
            auto newline_range = FindNextNewline(data_span, line_start);
            if (!newline_range.has_value())
            {
                break;
            }
            _lines.push_back(CharRange{ line_start, newline_range->start });
            line_start = newline_range->end;
        }

        _lines.push_back(CharRange{ line_start, data_span.size() });
    }

    std::string_view GetLineChars(std::size_t nLine) const
    {
        assert(nLine < _lines.size());
        auto line = _lines[nLine];
        auto data_span = DataAsStringView(_buffer.GetAllData());
        return data_span.substr(line.start, line.end - line.start);
    }

    std::size_t GetLineCount() const
    {
        return _lines.size();
    }

private:
    sci::MemoryBuffer _buffer;
    std::vector<CharRange> _lines;
};

bool LineSource::TryGetMoreData() const
{
    return true;
}

ParserBufferIterator::ParserBufferIterator() : _lineSource(nullptr), _nLine(0), _nChar(0)
{
}

//ParserBufferIterator::ParserBufferIterator(CScriptStreamLimiter* limiter, LineCol dwPos) : _limiter(limiter), _pidStream(limiter->GetIdPtr())
//{
//    _nLine = dwPos.Line();
//    _nChar = dwPos.Column();
//    if (_nLine < _limiter->GetLineCount())
//    {
//        _nLength = _limiter->GetLineLength(_nLine);
//        _pszLine = _limiter->GetLineChars(_nLine);
//    }
//    else
//    {
//        _nLength = 0;
//    }
//    _id = *_pidStream;
//}

char ParserBufferIterator::operator*()
{
    return (_nChar == _line.size()) ? '\n' : _line[_nChar];
}

std::string ParserBufferIterator::GetLookAhead(int nChars) const
{
    return std::string(_lineSource->GetLine(_nLine).substr(_nChar, nChars));
}

int ParserBufferIterator::CountPosition(int tabSize) const
{
    int visualPosition = 0;
    int charPos = 0;
    while (charPos < _nChar)
    {
        if (_line[charPos] == '\t')
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

ParserBufferIterator::ParserBufferIterator(LineSource* lineSource, int nLine, std::size_t nChar) :
    _lineSource(lineSource), _nLine(nLine), _nChar(nChar)
{
    if (nLine < _lineSource->GetLineCount())
    {
        _line = _lineSource->GetLine(nLine);
    }
}

bool ParserBufferIterator::AtEnd() const
{
    if (!_lineSource)
    {
        return true;
    }

    return _nLine >= _lineSource->GetLineCount();
}

ParserBufferIterator& ParserBufferIterator::operator++()
{
    assert(_line.data() == nullptr); // EOF
    _nChar++;
    bool at_end_of_current_data = _nChar == _line.size() && _nLine == _lineSource->GetLineCount() - 1;

    if (at_end_of_current_data) {
        if (_lineSource->TryGetMoreData())
        {
            // We have added more data, so get the same line again. It should now be longer.
            _line = _lineSource->GetLine(_nLine);
        }
    }

    if (_nChar > _line.size()) // for == we use '\n', unless this is the last line
    {
        if (_nLine < _lineSource->GetLineCount())
        {
            _line = _lineSource->GetLine(_nLine);
        }
        else
        {
            _line = std::string_view();
        }
        _nChar = 0;
        _nLine++;
    }
    return *this;
}

bool ParserBufferIterator::operator<(const ParserBufferIterator& other) const
{
    return (_nLine < other._nLine) || ((_nLine == other._nLine) && ((_nChar < other._nChar)));
}

std::string ParserBufferIterator::tostring() const
{
    return absl::StrFormat("Line %d, column %d", _nLine + 1, _nChar);
}

bool ParserBufferIterator::operator==(const ParserBufferIterator& other) const
{
    if (AtEnd()) {
        return other.AtEnd();
    }

    if (other.AtEnd()) {
        return false;
    }

    return _nLine == other._nLine && _nChar == other._nChar;
}

bool ParserBufferIterator::operator!=(const ParserBufferIterator& other) const
{
    return !(*this == other);
}

void ParserBufferIterator::ResetLine()
{
    _nChar = 0;
}

void ParserBufferIterator::Restore(const ParserBufferIterator& prev)
{
    // We should be referencing the same backing object.
    assert(prev._lineSource== _lineSource);
    (*this) = prev;
}

LineCol ParserBufferIterator::GetPosition() const
{
    return LineCol(_nLine, _nChar);
}
int ParserBufferIterator::GetLineNumber() const
{
    return _nLine;
}
int ParserBufferIterator::GetColumnNumber() const
{
    return _nChar;
}