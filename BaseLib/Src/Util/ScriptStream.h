#pragma once
#include <cstddef>
#include <iterator>
#include <string>

#include "LineCol.h"

class ScriptStreamLineSource
{
public:
    virtual ~ScriptStreamLineSource() = default;

    virtual std::size_t GetLineCount() const = 0;
    virtual std::size_t GetLineLength(int nLine) const = 0;
    virtual const char* GetLineChars(int nLine) const = 0;
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

    ScriptStreamIterator() : _limiter(nullptr) {}
    ScriptStreamIterator(ScriptStreamLineSource* lineSource, LineCol dwPos = LineCol());
    bool operator<(const ScriptStreamIterator& other) const;
    std::string tostring() const;
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
    std::string GetLookAhead(int nChars);
    int CountPosition(int tabSize) const;

private:
    char operator*();
    ScriptStreamIterator& operator++();
    int Compare(const ScriptStreamIterator& other) const;
    ScriptStreamLineSource* _limiter;
    std::size_t _nLine;
    std::size_t _nChar;
    const char* _pszLine;
    std::size_t _nLength;
};

class ScriptStream
{
public:
    ScriptStream(ScriptStreamLineSource* lineSource);

    ScriptStreamIterator begin() { return ScriptStreamIterator(_pLimiter); }
    ScriptStreamIterator get_at(LineCol dwPos) { return ScriptStreamIterator(_pLimiter, dwPos); }

private:
    ScriptStreamLineSource* _pLimiter;
};