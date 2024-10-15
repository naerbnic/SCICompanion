#pragma once
#include <intsafe.h>

//
// Represents a position in a script.
//
class LineCol
{
public:
    LineCol() : _dwPos(0) {}
    LineCol(int line, int column) : _dwPos(((((DWORD)(line)) << 16) + (DWORD)(column))) {}
    int Line() const { return static_cast<int>((((_dwPos) >> 16) & 0xffff)); }
    int Column() const { return static_cast<int>(((_dwPos) & 0x0000ffff)); }
    bool operator<(const LineCol& _Right) const
    {
        return _dwPos < _Right._dwPos;
    }
    bool operator<=(const LineCol& _Right) const
    {
        return _dwPos <= _Right._dwPos;
    }
private:
    DWORD _dwPos;
};