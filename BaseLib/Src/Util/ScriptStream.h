#pragma once
#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

#include "LineCol.h"

class ScriptStreamLineSource
{
public:
    virtual ~ScriptStreamLineSource() = default;

    virtual std::size_t GetLineCount() const = 0;
    virtual std::size_t GetLineLength(std::size_t line_index) const = 0;
    virtual const char* GetLineChars(std::size_t line_index) const = 0;
    virtual bool TryGetMoreData() = 0;
};

class ScriptStreamIterator
{
public:
    typedef std::forward_iterator_tag iterator_category;
    typedef char value_type;
    typedef std::ptrdiff_t difference_type; // ??
    typedef char& reference;
    typedef char* pointer;

    ScriptStreamIterator() : line_source_(nullptr), line_index_(0), char_index_(0), line_ptr_(nullptr), line_length_(0)
    {
    }

    explicit ScriptStreamIterator(ScriptStreamLineSource* line_source, LineCol initial_pos = LineCol());
    bool operator<(const ScriptStreamIterator& other) const;
    std::string ToString() const;
    LineCol GetPosition() const;
    int GetLineNumber() const;
    int GetColumnNumber() const;
    bool operator==(const ScriptStreamIterator& other) const;
    bool operator!=(const ScriptStreamIterator& other) const;
    void Restore(const ScriptStreamIterator& prev);
    void ResetLine();

    // Future API
    char GetChar() const;
    void Advance();
    bool AtEnd() const;
    char AdvanceAndGetChar();

    // For debugging
    std::string GetLookAhead(int lookahead_len);
    int CountPosition(int tab_size) const;

private:
    char operator*() const;
    ScriptStreamIterator& operator++();
    int Compare(const ScriptStreamIterator& other) const;
    ScriptStreamLineSource* line_source_;
    std::size_t line_index_;
    std::size_t char_index_;
    const char* line_ptr_;
    std::size_t line_length_;
};

class ScriptStream
{
public:
    explicit ScriptStream(ScriptStreamLineSource* line_source);

    ScriptStreamIterator Begin() const { return ScriptStreamIterator(line_source_); }
    ScriptStreamIterator GetAt(LineCol buffer_pos) const { return ScriptStreamIterator(line_source_, buffer_pos); }

private:
    ScriptStreamLineSource* line_source_;
};

class StringLineSource : public ScriptStreamLineSource
{
public:
    explicit StringLineSource(std::string_view str);

    std::size_t GetLineCount() const override;
    std::size_t GetLineLength(std::size_t line_index) const override;
    const char* GetLineChars(std::size_t line_index) const override;
    bool TryGetMoreData() override;

private:
    std::string_view str_;
    std::vector<std::pair<std::size_t, std::size_t>> line_offsets_;
};