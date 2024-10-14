#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <Windows.h>
#include <absl/status/statusor.h>

#include "ScopedValue.h"

std::string GetExeSubFolder(const char* subFolder);


class ScopedHandle : public ScopedValue<ScopedHandle, HANDLE>
{
public:
    using ScopedValue::ScopedValue;

private:
    friend ScopedValue<ScopedHandle, HANDLE>;

    static bool IsValidImpl(HANDLE value);
    static void CleanupValue(HANDLE value);
};

class ScopedMappedMemory : public ScopedValue<ScopedMappedMemory, const uint8_t*>
{
public:
    using ScopedValue::ScopedValue;

private:
    friend ScopedValue<ScopedMappedMemory, const uint8_t*>;

    static bool IsValidImpl(const uint8_t* value);
    static void CleanupValue(const uint8_t* value);
};

class MemoryMappedFile
{
public:
    static absl::StatusOr<MemoryMappedFile> FromFilename(const std::string& filename);

    MemoryMappedFile();

    MemoryMappedFile(const MemoryMappedFile& other) = delete;
    MemoryMappedFile(MemoryMappedFile&& other) noexcept = default;

    MemoryMappedFile& operator=(const MemoryMappedFile& other) = delete;
    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept = default;

    absl::Span<const uint8_t> GetDataBuffer() const;

private:
    MemoryMappedFile(ScopedHandle file_handle, ScopedHandle mapping_handle, ScopedMappedMemory mapped_memory, uint32_t size);
    ScopedHandle file_handle_;
    ScopedHandle mapping_handle_;
    ScopedMappedMemory mapped_memory_;
    uint32_t size_;
};