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

#include "PerfTimer.h"

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

// CRTP base class for scoped values.
template <class Derived, class T>
class ScopedValue
{
public:
    ScopedValue() = default;
    explicit ScopedValue(T value)
    {
        if (Derived::IsValidImpl(value))
        {
            value_ = std::move(value);
        }
    }
    ~ScopedValue()
    {
        if (IsValid())
        {
            Derived::CleanupValue(std::move(value_).value());
        }
    }

    ScopedValue(const ScopedValue&) = delete;
    ScopedValue(ScopedValue&& other) noexcept {
        std::swap(value_, other.value_);
    }
    ScopedValue& operator=(const ScopedValue&) = delete;
    ScopedValue& operator=(ScopedValue&& other) noexcept
    {
        std::swap(value_, other.value_);
        return *this;
    }

    const T& GetValue() const { return *value_; }

    explicit operator bool() const { return IsValid(); }

    bool IsValid() const { return value_.has_value(); }

private:
    std::optional<T> value_;
};

class ScopedHandle : public ScopedValue<ScopedHandle, HANDLE>
{
public:
    using ScopedValue::ScopedValue;

    static bool IsValidImpl(HANDLE value) { return value != nullptr && value != INVALID_HANDLE_VALUE; }
    static void CleanupValue(HANDLE value)
    {
        if (!CloseHandle(value))
        {
            throw std::exception("Failed to close handle.");
        }
    }
};

class ScopedMappedMemory : public ScopedValue<ScopedMappedMemory, const uint8_t*>
{
public:
    using ScopedValue::ScopedValue;

    static bool IsValidImpl(const uint8_t* value) { return value != nullptr; }
    static void CleanupValue(const uint8_t* value)
    {
        BOOL result = UnmapViewOfFile(value);
        if (!result)
        {
            throw std::exception("Failed to unmap memory.");
        }
    }
};

// We need at least two implementations: One for a memory buffer, another for a memory-mapped file.
class istream::Impl
{
public:
    virtual ~Impl() = default;

    virtual const uint8_t* GetDataBuffer() const = 0;
    virtual uint32_t GetDataSize() const = 0;
};

class istream::MemoryImpl : public istream::Impl
{
public:
    MemoryImpl(uint32_t size) :
        size_(size), data_(std::unique_ptr<uint8_t[]>(new uint8_t[size]))
    {
    }

    MemoryImpl(const uint8_t* data, uint32_t size) : MemoryImpl(size)
    {
        std::memcpy(data_.get(), data, size);
    }

    virtual uint8_t* GetMutableDataBuffer()
    {
        return data_.get();
    }

    const uint8_t* GetDataBuffer() const override
    {
        return data_.get();
    }

    uint32_t GetDataSize() const override
    {
        return size_;
    }

private:
    std::size_t size_;
    std::unique_ptr<uint8_t[]> data_;
};

class istream::FileImpl : public istream::Impl
{
public:
    static absl::StatusOr<std::unique_ptr<Impl>> FromFilename(const std::string& filename)
    {
        auto file_handle = ScopedHandle(CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, 0, nullptr));
        if (!file_handle.IsValid())
        {
            return absl::NotFoundError("Could not open file for reading");
        }

        // If no length specifies, then until the end of the file.
        DWORD dwSizeHigh;
        DWORD dwSize = GetFileSize(file_handle.GetValue(), &dwSizeHigh);
        if (dwSize == INVALID_FILE_SIZE)
        {
            return absl::FailedPreconditionError("Unable to get file size.");
        }
        assert(dwSizeHigh == 0);
        auto mapping_handle = ScopedHandle(CreateFileMapping(file_handle.GetValue(), nullptr, PAGE_READONLY, 0, 0,
            nullptr));
        if (!mapping_handle.IsValid())
        {
            return absl::FailedPreconditionError("Could not open file mapping.");
        }
        auto dataMemoryMapped = ScopedMappedMemory(static_cast<const uint8_t*>(MapViewOfFile(
            mapping_handle.GetValue(), FILE_MAP_READ, 0, 0, 0)));
        if (!dataMemoryMapped)
        {
            return absl::FailedPreconditionError("Could not get mapped file view.");
        }
        
        auto valid_size = dwSize;
        return std::unique_ptr<Impl>(new FileImpl(std::move(file_handle), std::move(mapping_handle), std::move(dataMemoryMapped), valid_size));
    }

    const uint8_t* GetDataBuffer() const override
    {
        return mapped_memory_.GetValue();
    }

    uint32_t GetDataSize() const override
    {
        return size_;
    }

private:
    FileImpl(ScopedHandle file_handle, ScopedHandle mapping_handle, ScopedMappedMemory mapped_memory, uint32_t size)
        : file_handle_(std::move(file_handle)), mapping_handle_(std::move(mapping_handle)), mapped_memory_(std::move(mapped_memory)), size_(size)
    {
    }
    ScopedHandle file_handle_;
    ScopedHandle mapping_handle_;
    ScopedMappedMemory mapped_memory_;
    uint32_t size_;
};
istream istream::MapFile(const std::string& filename)
{
    return istream(FileImpl::FromFilename(filename).value());
}

istream istream::ReadFromFile(HANDLE hFile, DWORD lengthToInclude)
{
    // Start from the current position:
    uint32_t dwSize = lengthToInclude;
    if (dwSize == 0)
    {
        // If no length specifies, then until the end of the file.
        DWORD dwCurrentPosition = SetFilePointer(
            hFile, 0, nullptr, FILE_CURRENT);
        DWORD dwSizeHigh = 0;
        DWORD file_size = GetFileSize(hFile, &dwSizeHigh);

        if (file_size == INVALID_FILE_SIZE)
        {
            throw std::exception("Unable to get file size.");
        }

        // Limit ourselves a little (to 4GB).
        if (dwSizeHigh != 0)
        {
            throw std::exception("File size too large.");
        }

        dwSize = file_size - dwCurrentPosition;
    }
    auto impl = std::make_shared<MemoryImpl>(dwSize);
    // = std::make_unique<uint8_t[]>(dwSize);
    // Don't use make_unique, because it will zero init the array (perf).
    DWORD dwSizeRead;
    if (!ReadFile(hFile, impl->GetMutableDataBuffer(), dwSize, &dwSizeRead, nullptr))
    {
        throw std::exception("Unable to read file data.");
    }

    if (dwSizeRead != dwSize)
    {
        throw std::exception("Unable to read beyond end of file.");
    }

    return istream(std::move(impl));
}
istream istream::ReadFromFile(const std::string& filename, DWORD lengthToInclude)
{
    auto file_handle = ScopedHandle(CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr));
    if (!file_handle.IsValid())
    {
        throw std::exception("Unable to open file.");
    }
    return ReadFromFile(file_handle.GetValue(), lengthToInclude);
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
    return _impl->GetDataSize();
}

const uint8_t* istream::GetInternalPointer() const
{
    return _impl->GetDataBuffer();
}

bool istream::_Read(uint8_t* pDataDest, uint32_t cCount)
{
    auto buffer_size = _impl->GetDataSize();
    if ((buffer_size > _iIndex) && ((buffer_size - _iIndex) >= cCount))
    {
        // we're good.
        CopyMemory(pDataDest, _impl->GetDataBuffer() + _iIndex, cCount);
        _iIndex += cCount;
        return true;
    }
    return false;
}

void istream::seekg(uint32_t dwIndex)
{
    _iIndex = dwIndex;
    if (_iIndex > _impl->GetDataSize())
    {
        _OnReadPastEnd();
    }
}

void istream::seekg(int32_t offset, std::ios_base::seekdir way)
{
    switch (way)
    {
    case std::ios_base::beg:
        seekg(offset);
        break;
    case std::ios_base::cur:
        seekg(_iIndex + offset);
        break;
    case std::ios_base::end:
        seekg(_impl->GetDataSize() + offset);
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
    uint32_t dwSave = tellg();
    if (!_ReadWord(&w))
    {
        seekg(dwSave);
        w = 0;
        _OnReadPastEnd();
    }
    return *this;
}

istream& istream::operator>>(uint8_t& b)
{
    uint32_t dwSave = tellg();
    if (!_ReadByte(&b))
    {
        seekg(dwSave);
        b = 0;
        _OnReadPastEnd();
    }
    return *this;
}

istream& istream::operator>>(std::string& str)
{
    auto buffer = _impl->GetDataBuffer();
    auto buffer_size = _impl->GetDataSize();
    uint32_t dwSave = tellg();
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
        seekg(dwSave);
        _OnReadPastEnd();
    }
    return *this;
}

void istream::skip(uint32_t cBytes)
{
    if ((_iIndex + cBytes) < _impl->GetDataSize())
    {
        _iIndex += cBytes;
    }
    else
    {
        _OnReadPastEnd();
    }
}

bool istream::peek(uint8_t& b)
{
    auto buffer = _impl->GetDataBuffer();
    if (_iIndex < _impl->GetDataSize())
    {
        b = buffer[_iIndex];
        return true;
    }
    return false;
}

bool istream::peek(uint16_t& w)
{
    auto buffer = _impl->GetDataBuffer();
    if ((_iIndex + 1) < _impl->GetDataSize())
    {
        w = *reinterpret_cast<const uint16_t*>(&buffer[_iIndex]);
        return true;
    }
    return false;
}

uint8_t istream::peek()
{
    uint8_t b;
    peek(b);
    return b;
}

void istream::getRLE(std::string& str)
{
    // Vocab files strings are run-length encoded.
    uint16_t wSize;
    uint32_t dwSave = tellg();
    *this >> wSize;
    if (wSize == 0)
    {
        str.clear();
    }
    else
    {
        while (good() && wSize)
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
    if (!good())
    {
        str.clear();
        seekg(dwSave);
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
        from.read_data(buffer, amountToTransfer);
        to.WriteBytes(buffer, amountToTransfer);
        count -= amountToTransfer;
    }
}
}
