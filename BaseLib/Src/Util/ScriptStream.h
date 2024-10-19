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

class CCrystalScriptStream
{
public:
    CCrystalScriptStream(ScriptStreamLineSource* lineSource);

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef char value_type;
        typedef std::ptrdiff_t difference_type; // ??
        typedef char& reference;
        typedef char* pointer;

        const_iterator() : _limiter(nullptr) {}
        const_iterator(ScriptStreamLineSource* lineSource, LineCol dwPos = LineCol());
        bool operator<(const const_iterator& other) const;
        std::string tostring() const;
        LineCol GetPosition() const;
        int GetLineNumber() const;
        int GetColumnNumber() const;
        bool operator==(const const_iterator& other) const;
        bool operator!=(const const_iterator& other) const;
        void Restore(const const_iterator& prev);
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
        const_iterator& operator++();
        int Compare(const const_iterator& other) const;
        ScriptStreamLineSource* _limiter;
        std::size_t _nLine;
        std::size_t _nChar;
        const char* _pszLine;
        std::size_t _nLength;
    };

    const_iterator begin() { return const_iterator(_pLimiter); }
    const_iterator get_at(LineCol dwPos) { return const_iterator(_pLimiter, dwPos); }

private:
    ScriptStreamLineSource* _pLimiter;
};

using ScriptCharIterator = CCrystalScriptStream::const_iterator;