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

#include <Windows.h>
#include <cstdint>
#include <ios>
#include <absl/types/span.h>

#include "DataBuffer.h"

namespace sci
{
// STL doesn't have a memory stream (other than stringstream), so we'll implement our own
// simple one.
class ostream
{
public:
    ostream();
    ostream(const ostream& src) = delete;
    ostream& operator=(const ostream& src) = delete;

    void WriteWord(uint16_t w);
    void WriteByte(uint8_t b);
    void WriteBytes(const uint8_t* pData, int cCount);
    void WriteBytes(absl::Span<const uint8_t> data);
    void FillByte(uint8_t value, int cCount);

    void reset();
    uint32_t tellp() const { return _cbSizeValid; }

    void seekp(uint32_t newPosition);
    void seekp(int32_t offset, std::ios_base::seekdir way);

    // Use with caution!
    uint8_t* GetInternalPointer() { return _pData.get(); }
    const uint8_t* GetInternalPointer() const { return _pData.get(); }

    template <class _T>
    ostream& operator<<(const _T& t)
    {
        WriteBytes(reinterpret_cast<const uint8_t*>(&t), sizeof(t));
        return *this;
    }

    ostream& operator<<(const std::string aString)
    {
        WriteBytes(reinterpret_cast<const uint8_t*>(aString.c_str()),
                   (int)aString.length() + 1);
        return *this;
    }

    uint32_t GetDataSize() const { return _cbSizeValid; }

    void EnsureCapacity(uint32_t size);

private:
    void _Grow(uint32_t cbGrowMin);

    std::unique_ptr<uint8_t[]> _pData; // Our data
    uint32_t _iIndex; // Current index into it.
    uint32_t _cbReserved; // The size in bytes of pData
    uint32_t _cbSizeValid; // Size that contains valid data.
    std::ios_base::iostate _state;
};

//
// Class used to read/write data from a resource
// It's a bit messy now with whom owns the internal byte stream. This needs some work.
//
class istream
{
public:
    static istream MapFile(const std::string& filename);
    static istream ReadFromFile(HANDLE hFile, DWORD lengthToInclude = 0);
    static istream ReadFromFile(const std::string& filename, DWORD lengthToInclude = 0);
    static istream ReadFromMemory(MemoryBuffer memory_buffer);
    istream(const uint8_t* pData, uint32_t cbSize);
    istream(const istream& original, uint32_t absoluteOffset);
    istream();

    uint32_t GetDataSize() const;

    // Use with caution!
    const uint8_t* GetInternalPointer() const;

    // New api
    bool IsGood()
    {
        return (_state & (std::ios_base::failbit | std::ios_base::eofbit)) == 0;
    }

    bool HasMoreData() { return GetAbsolutePosition() < GetDataSize(); }
    istream& operator>>(uint16_t& w);
    istream& operator>>(uint8_t& b);
    istream& operator>>(std::string& str);
    void getRLE(std::string& str);

    void read_data(uint8_t* pBuffer, uint32_t cbBytes)
    {
        if (!_Read(pBuffer, cbBytes))
        {
            _state = std::ios_base::eofbit | std::ios_base::failbit;
        }
    }
    void read_data(absl::Span<uint8_t> buffer)
    {
        read_data(buffer.data(), buffer.size());
    }

    template <class _T>
    istream& operator>>(_T& t)
    {
        uint32_t dwSave = GetAbsolutePosition();
        if (!_Read(reinterpret_cast<uint8_t*>(&t), sizeof(t)))
        {
            SeekAbsolute(dwSave);
            memset(&t, 0, sizeof(t));
            _state = std::ios_base::eofbit | std::ios_base::failbit;
        }
        return *this;
    }

    void SeekAbsolute(uint32_t dwIndex);
    void Seek(int32_t offset, std::ios_base::seekdir way);
    uint32_t GetAbsolutePosition() const { return _iIndex; }

    uint32_t GetBytesRemaining() const
    {
        return (GetDataSize() > _iIndex) ? (GetDataSize() - _iIndex) : 0;
    }

    void SkipBytes(uint32_t cBytes);
    bool PeekByte(uint8_t& b);
    bool PeekWord(uint16_t& w);
    uint8_t PeekByte();

    void setThrowExceptions(bool shouldThrow)
    {
        _throwExceptions = shouldThrow;
    }

private:
    class Impl;
    class MemoryImpl;
    class FileImpl;
    class DataBufferImpl;

    istream(std::shared_ptr<Impl> impl);

    bool _ReadWord(uint16_t* pw) { return _Read((uint8_t*)pw, 2); }
    bool _Read(uint8_t* pData, uint32_t cCount);
    bool _ReadByte(uint8_t* pb) { return _Read(pb, 1); }
    void _OnReadPastEnd();

    std::shared_ptr<Impl> _impl;
    uint32_t _iIndex;
    bool _throwExceptions;
    std::ios_base::iostate _state;
};

istream istream_from_ostream(ostream& src);

void transfer(istream& from, ostream& to, uint32_t count);

}
