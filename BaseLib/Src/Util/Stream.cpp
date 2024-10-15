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
#include "Stream.h"

#include <algorithm>
#include <optional>
#include <absl/status/statusor.h>

#include "DataBuffer.h"
#include "WindowsUtils.h"

namespace sci
{
ostream::ostream()
{
    _cbReserved = 500;
    _pData = std::make_unique<uint8_t[]>(_cbReserved);
    _cbSizeValid = 0;
    _iIndex = 0;
    _state = std::ios_base::goodbit;
}

void ostream::reset()
{
    _cbSizeValid = 0;
    _iIndex = 0;
    _state = std::ios_base::goodbit;
}

void ostream::EnsureCapacity(uint32_t size)
{
    if (size > _cbReserved)
    {
        _Grow(size - _cbReserved);
    }
}

void ostream::_Grow(uint32_t cbGrowMin)
{
    // Grow by 50% or cbGrowMin
    uint32_t cbSizeNew = max(_cbReserved + (_cbReserved / 3 * 2),
                             _cbReserved + cbGrowMin);
    std::unique_ptr<uint8_t[]> pDataNew = std::make_unique<uint8_t
        []>(cbSizeNew);
    memcpy(pDataNew.get(), _pData.get(), _cbReserved);
    std::swap(_pData, pDataNew);
    _cbReserved = cbSizeNew;
}

void ostream::WriteByte(uint8_t b)
{
    if (_iIndex >= _cbReserved)
    {
        _Grow(1);
    }
    _pData[_iIndex] = b;
    _iIndex++;
    _cbSizeValid = max(_cbSizeValid, _iIndex);
}

void ostream::WriteWord(uint16_t w)
{
    WriteByte((uint8_t)(w & 0xff));
    WriteByte((uint8_t)(w >> 8));
}


void ostream::WriteBytes(const uint8_t* pDataIn, int cCount)
{
    if ((_iIndex + cCount) >= _cbReserved)
    {
        _Grow((uint32_t)cCount);
    }

    memcpy(&_pData[_iIndex], pDataIn, (size_t)cCount);
    _iIndex += (uint32_t)cCount;
    _cbSizeValid = max(_cbSizeValid, _iIndex);
}

void ostream::WriteBytes(absl::Span<const uint8_t> data)
{
    WriteBytes(data.data(), data.size());
}

void ostream::FillByte(uint8_t value, int cCount)
{
    if ((_iIndex + cCount) >= _cbReserved)
    {
        _Grow((uint32_t)cCount);
    }
    memset(&_pData[_iIndex], value, (size_t)cCount);
    _iIndex += (uint32_t)cCount;
    _cbSizeValid = max(_cbSizeValid, _iIndex);
}

void ostream::seekp(uint32_t newPosition)
{
    if (newPosition > _cbSizeValid)
    {
        throw std::exception("Attempt to seek past end of stream.");
    }
    else
    {
        _iIndex = newPosition;
    }
}

void ostream::seekp(int32_t offset, std::ios_base::seekdir way)
{
    switch (way)
    {
    case std::ios_base::beg:
        seekp(offset);
        break;
    case std::ios_base::cur:
        assert(false); // I think this is broken
        if (offset > (int32_t)_iIndex)
        {
            throw std::exception(
                "Attempt to seek past the beginning of the stream.");
        }
        seekp(_iIndex - offset);
        break;
    case std::ios_base::end:
        if ((offset > (int32_t)_cbSizeValid) || (offset > 0))
        {
            throw std::exception("Attempt to seek outside stream.");
        }
        seekp(_cbSizeValid + offset);
        break;
    }
}

// We need at least two implementations: One for a memory buffer, another for a memory-mapped file.
class istream::Impl
{
public:
    virtual ~Impl() = default;

    virtual absl::Span<const uint8_t> GetDataBuffer() const = 0;
};

class istream::MemoryImpl : public istream::Impl
{
public:
    MemoryImpl(MemoryBuffer buffer) :
        buffer_(std::move(buffer))
    {
    }

    MemoryImpl(const uint8_t* pData, uint32_t cbSize)
    {
        buffer_ = MemoryBuffer::CreateFromVector(std::vector<uint8_t>(pData, pData + cbSize));
    }

    absl::Span<const uint8_t> GetDataBuffer() const override
    {
        return buffer_.GetAllData();
    }

private:
    MemoryBuffer buffer_;
};

class istream::FileImpl : public istream::Impl
{
public:
    static absl::StatusOr<std::unique_ptr<Impl>> FromFilename(const std::string& filename)
    {
        auto mapped_file = MemoryMappedFile::FromFilename(filename);
        if (!mapped_file.ok())
        {
            return mapped_file.status();
        }
        return std::unique_ptr<FileImpl>(new FileImpl(std::move(mapped_file).value()));
    }

    absl::Span<const uint8_t> GetDataBuffer() const override
    {
        return mapped_file_.GetDataBuffer();
    }

private:
    FileImpl(MemoryMappedFile mapped_file)
        : mapped_file_(std::move(mapped_file))
    {
    }
    MemoryMappedFile mapped_file_;
};

istream istream::MapFile(const std::string& filename)
{
    return istream(FileImpl::FromFilename(filename).value());
}

istream istream::ReadFromFile(HANDLE hFile, DWORD lengthToInclude)
{
    auto file_contents = lengthToInclude > 0 ? ReadFileContents(hFile, 0, lengthToInclude) : ReadFileContents(hFile);
    if (!file_contents.ok())
    {
        throw std::runtime_error(absl::StrFormat("Error reading file: %v", file_contents.status()));
    }
    return istream(
        std::make_shared<MemoryImpl>(MemoryBuffer::CreateFromVector(std::move(std::move(file_contents).value()))));
}
istream istream::ReadFromFile(const std::string& filename, DWORD lengthToInclude)
{
    auto file_contents = lengthToInclude > 0 ? ReadFileContents(filename, 0, lengthToInclude) : ReadFileContents(filename);
    if (!file_contents.ok())
    {
        throw std::runtime_error("Unable to open file.");
    }
    return istream(
        std::make_shared<MemoryImpl>(MemoryBuffer::CreateFromVector(std::move(std::move(file_contents).value()))));
}

istream istream::ReadFromMemory(MemoryBuffer memory_buffer)
{
    return istream(std::make_shared<MemoryImpl>(std::move(memory_buffer)));
}

istream::istream(const uint8_t* pData, uint32_t cbSize) : 
istream(std::make_shared<MemoryImpl>(pData, cbSize))
{
}


istream::istream() :
    _impl(nullptr), _iIndex(0), _throwExceptions(false), _state(std::ios_base::goodbit)
{
}

istream::istream(std::shared_ptr<Impl> impl) :
    _impl(std::move(impl)), _iIndex(0), _throwExceptions(false), _state(std::ios_base::goodbit)
{
    assert(_impl);
}

istream::istream(const istream& original,
                 uint32_t absoluteOffset) : istream(original)
{
    _iIndex = absoluteOffset;
}

uint32_t istream::GetDataSize() const
{
    return _impl->GetDataBuffer().size();
}

const uint8_t* istream::GetInternalPointer() const
{
    return _impl->GetDataBuffer().data();
}

bool istream::_Read(uint8_t* pDataDest, uint32_t cCount)
{
    auto buffer_size = GetDataSize();
    if ((buffer_size > _iIndex) && ((buffer_size - _iIndex) >= cCount))
    {
        // we're good.
        CopyMemory(pDataDest, GetInternalPointer() + _iIndex, cCount);
        _iIndex += cCount;
        return true;
    }
    return false;
}

void istream::SeekAbsolute(uint32_t dwIndex)
{
    _iIndex = dwIndex;
    if (_iIndex > GetDataSize())
    {
        _OnReadPastEnd();
    }
}

void istream::Seek(int32_t offset, std::ios_base::seekdir way)
{
    switch (way)
    {
    case std::ios_base::beg:
        SeekAbsolute(offset);
        break;
    case std::ios_base::cur:
        SeekAbsolute(_iIndex + offset);
        break;
    case std::ios_base::end:
        SeekAbsolute(GetDataSize() + offset);
        break;
    }
}

void istream::_OnReadPastEnd()
{
    _state = std::ios_base::eofbit | std::ios_base::failbit;
    if (_throwExceptions)
    {
        throw std::exception("Read past end of stream.");
    }
}

istream& istream::operator>>(uint16_t& w)
{
    uint32_t dwSave = GetAbsolutePosition();
    if (!_ReadWord(&w))
    {
        SeekAbsolute(dwSave);
        w = 0;
        _OnReadPastEnd();
    }
    return *this;
}

istream& istream::operator>>(uint8_t& b)
{
    uint32_t dwSave = GetAbsolutePosition();
    if (!_ReadByte(&b))
    {
        SeekAbsolute(dwSave);
        b = 0;
        _OnReadPastEnd();
    }
    return *this;
}

istream& istream::operator>>(std::string& str)
{
    auto buffer = _impl->GetDataBuffer();
    auto buffer_size = GetDataSize();
    uint32_t dwSave = GetAbsolutePosition();
    str.clear();
    // Read a null terminated string.
    bool fDone = false;
    while (_iIndex < buffer_size)
    {
        char x = (char)buffer[_iIndex];
        ++_iIndex;
        if (x)
        {
            str += x;
        }
        else
        {
            fDone = true;
            break;
        }
    }
    if (!fDone && (_iIndex == buffer_size))
    {
        // Failure
        str.clear();
        SeekAbsolute(dwSave);
        _OnReadPastEnd();
    }
    return *this;
}

void istream::SkipBytes(uint32_t cBytes)
{
    if ((_iIndex + cBytes) < GetDataSize())
    {
        _iIndex += cBytes;
    }
    else
    {
        _OnReadPastEnd();
    }
}

bool istream::PeekByte(uint8_t& b)
{
    auto buffer = _impl->GetDataBuffer();
    if (_iIndex < GetDataSize())
    {
        b = buffer[_iIndex];
        return true;
    }
    return false;
}

bool istream::PeekWord(uint16_t& w)
{
    auto buffer = _impl->GetDataBuffer();
    if ((_iIndex + 1) < GetDataSize())
    {
        w = *reinterpret_cast<const uint16_t*>(&buffer[_iIndex]);
        return true;
    }
    return false;
}

uint8_t istream::PeekByte()
{
    uint8_t b;
    PeekByte(b);
    return b;
}

void istream::getRLE(std::string& str)
{
    // Vocab files strings are run-length encoded.
    uint16_t wSize;
    uint32_t dwSave = GetAbsolutePosition();
    *this >> wSize;
    if (wSize == 0)
    {
        str.clear();
    }
    else
    {
        while (IsGood() && wSize)
        {
            uint8_t b;
            *this >> b;
            if (b)
            {
                str += (char)b;
                --wSize;
            }
            else
            {
                // Be very careful here... in addition to being RLE, these strings
                // are null-terminated.  We don't want to go adding a null character to
                // str, because it will not compare == when it should.
                break;
            }
        }
    }
    if (!IsGood())
    {
        str.clear();
        SeekAbsolute(dwSave);
    }
}

// A bit sketchy because we're using tellp, not "end"
istream istream_from_ostream(ostream& src)
{
    return istream(src.GetInternalPointer(), src.tellp());
}

void transfer(istream& from, ostream& to, uint32_t count)
{
    to.EnsureCapacity(to.GetDataSize() + count);

    uint8_t buffer[1024];
    while (count > 0)
    {
        uint32_t amountToTransfer = min(count, ARRAYSIZE(buffer));
        absl::Span buffer_span(buffer, amountToTransfer);
        from.read_data(buffer_span);
        to.WriteBytes(buffer_span);
        count -= amountToTransfer;
    }
}
}
