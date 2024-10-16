#pragma once
#include <memory>
#include <string>

#include "DataBuffer.h"
#include "LineCol.h"

// Opaque type for the internal implementation.

class ParserBufferImpl;

class LineSource
{
public:
    virtual ~LineSource() = default;

    virtual int GetLineCount() const = 0;
    virtual std::string_view GetLine(int nLine) const = 0;
    virtual bool TryGetMoreData() const;
};

class ParserBufferIterator
{
public:
    ParserBufferIterator();
    char operator*();
    ParserBufferIterator& operator++();
    bool operator<(const ParserBufferIterator& other) const;
    // ReSharper disable once IdentifierTypo
    // Needed to match the signature of the original method.
    std::string tostring() const;
    LineCol GetPosition() const;
    int GetLineNumber() const;
    int GetColumnNumber() const;
    bool operator==(const ParserBufferIterator& other) const;
    bool operator!=(const ParserBufferIterator& other) const;
    void Restore(const ParserBufferIterator& prev);
    void ResetLine();

    // For debugging
    std::string GetLookAhead(int nChars) const;
    int CountPosition(int tabSize) const;

private:
    friend class ParserBuffer;

    ParserBufferIterator(LineSource* lineSource, int nLine, std::size_t nChar);

    bool AtEnd() const;

    // Contains the main contents of the buffer.
    LineSource* _lineSource;
    int _nLine;
    std::size_t _nChar;
    std::string_view _line;
};

class ParserBuffer
{
public:
    ParserBuffer() = default;
    ParserBuffer(LineSource* lineSource);

    ParserBufferIterator begin() const;
    ParserBufferIterator get_at(LineCol dwPos) const;

private:
    LineSource* _lineSource;
};