#include "ScriptStream.h"

#include <cassert>
#include <optional>

ScriptStream::ScriptStream(ScriptStreamLineSource* line_source)
{
    line_source_ = line_source;
}


ScriptStreamIterator::ScriptStreamIterator(ScriptStreamLineSource* line_source, LineCol initial_pos) : line_source_(line_source)
{
    line_index_ = initial_pos.Line();
    char_index_ = initial_pos.Column();
    if (line_index_ < line_source_->GetLineCount())
    {
        line_length_ = line_source_->GetLineLength(line_index_);
        line_ptr_ = line_source_->GetLineChars(line_index_);
    }
    else
    {
        line_length_ = 0;
    }
}

char ScriptStreamIterator::operator*() const
{
    return GetChar();
}

char ScriptStreamIterator::GetChar() const
{
    if (line_index_ == line_source_->GetLineCount())
    {
        return '\0';
    }
    return (char_index_ == line_length_) ? '\n' : line_ptr_[char_index_];
}

std::string ScriptStreamIterator::GetLookAhead(int lookahead_len)
{
    const char* pszLine = line_source_->GetLineChars(line_index_);
    std::size_t cLineChars = line_source_->GetLineLength(line_index_);
    std::string text;
    while (pszLine && (char_index_ < cLineChars) && pszLine[char_index_] && (lookahead_len > 0))
    {
        text += pszLine[char_index_++];
        --lookahead_len;
    }
    return text;
}

int ScriptStreamIterator::CountPosition(int tab_size) const
{
    int visualPosition = 0;
    std::size_t charPos = 0;
    while (charPos < char_index_)
    {
        if (line_ptr_[charPos] == '\t')
        {
            // Go to the next tab stop
            int nextStop = (visualPosition + tab_size) / tab_size * tab_size;
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
    assert(!AtEnd()); // EOF
    char_index_++;
    if ((char_index_ > line_length_) ||
        ((char_index_ >= line_length_) && (line_index_ == (line_source_->GetLineCount() - 1)))) // for == we use '\n', unless this is the last line
    {
        if (line_source_->TryGetMoreData()) {
            line_length_ = line_source_->GetLineLength(line_index_);
            line_ptr_ = line_source_->GetLineChars(line_index_);
        }
    }


    while (char_index_ > line_length_)
    {
        // Need to move to the next line
        std::size_t cLines = line_source_->GetLineCount();
        while (line_index_ < cLines)
        {
            line_index_++;

            //OutputDebugString(fmt::format("Moved to line {0} of {1}\n", nLine, cLines).c_str());

            if (line_index_ < cLines)
            {
                line_length_ = line_source_->GetLineLength(line_index_);
                line_ptr_ = line_source_->GetLineChars(line_index_);
                assert(line_ptr_ != nullptr || line_length_ == 0);
                char_index_ = 0;
                break; // done
            }
        }
        if (line_index_ == cLines)
        {
            // We ran out of data.
            line_ptr_ = "\0"; // EOF
            line_length_ = 1;
            char_index_ = 0;
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

    if (line_index_ != other.line_index_)
    {
        return line_index_ < other.line_index_ ? -1 : 1;
    }

    if (char_index_ != other.char_index_)
    {
        return char_index_ < other.char_index_ ? -1 : 1;
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

std::string ScriptStreamIterator::ToString() const
{
    char sz[50];
    sprintf_s(sz, sizeof(sz), "Line %d, column %d", line_index_ + 1, char_index_);
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
    char_index_ = 0;
}

void ScriptStreamIterator::Restore(const ScriptStreamIterator& prev)
{
    // This is a hack for autocomplete. Ideally, we should retrieve line length each time.
    // If on the same line, keep the furthest length, since it may have been modified by AC.
    int furthestLength = (prev.line_index_ == line_index_) ? line_length_ : prev.line_length_;
    (*this) = prev;
    line_length_ = furthestLength;
}

LineCol ScriptStreamIterator::GetPosition() const
{
    return LineCol(static_cast<int>(line_index_), static_cast<int>(char_index_));
}
int ScriptStreamIterator::GetLineNumber() const
{
    return static_cast<int>(line_index_);
}
int ScriptStreamIterator::GetColumnNumber() const
{
    return static_cast<int>(char_index_);
}

std::optional<std::pair<std::size_t, std::size_t>> FindNextNewline(std::string_view str, std::size_t start)
{
    std::size_t r_pos = str.find('\r', start);
    if (r_pos != std::string_view::npos)
    {
        if (r_pos + 1 < str.size() && str[r_pos + 1] == '\n') {
            return std::make_pair(r_pos, r_pos + 2);
        }
        return std::make_pair(r_pos, r_pos + 1);
    }
    std::size_t n_pos = str.find('\n', start);
    if (n_pos != std::string_view::npos)
    {
        return std::make_pair(n_pos, n_pos + 1);
    }
    return std::nullopt;
}

StringLineSource::StringLineSource(std::string_view str) : str_(str)
{
    std::size_t curr_line_start = 0;
    while (true)
    {
        auto next_newline = FindNextNewline(str, curr_line_start);
        if (!next_newline)
        {
            break;
        }
        line_offsets_.emplace_back(curr_line_start, next_newline->first);
        curr_line_start = next_newline->second;
    }
    line_offsets_.emplace_back(curr_line_start, str.size());
}

std::size_t StringLineSource::GetLineCount() const
{
    return line_offsets_.size();
}

std::size_t StringLineSource::GetLineLength(std::size_t line_index) const
{
    auto [start, end] = line_offsets_[line_index];
    return end - start;
}

const char* StringLineSource::GetLineChars(std::size_t line_index) const
{
    auto [start, end] = line_offsets_[line_index];
    return str_.data() + start;
}

bool StringLineSource::TryGetMoreData()
{
    return false;
}
