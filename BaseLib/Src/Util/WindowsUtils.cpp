﻿#include "WindowsUtils.h"
#include <shlwapi.h>
#include <absl/strings/str_format.h>

std::string GetExeSubFolder(const char* subFolder)
{
    std::string templateFolder;
    TCHAR szPath[MAX_PATH];
    if (GetModuleFileName(nullptr, szPath, ARRAYSIZE(szPath)))
    {
        PSTR pszFileName = PathFindFileName(szPath);
        *pszFileName = 0; // null it
        templateFolder = absl::StrFormat("%s%s", szPath, subFolder);
    }
    return templateFolder;
}

bool ScopedHandle::IsValidImpl(HANDLE value)
{
    return value != nullptr && value != INVALID_HANDLE_VALUE;
}

void ScopedHandle::CleanupValue(HANDLE value)
{
    if (!CloseHandle(value))
    {
        throw std::runtime_error("Failed to close handle.");
    }
}

bool ScopedMappedMemory::IsValidImpl(const uint8_t* value)
{
    return value != nullptr;
}

void ScopedMappedMemory::CleanupValue(const uint8_t* value)
{
    BOOL result = UnmapViewOfFile(value);
    if (!result)
    {
        throw std::runtime_error("Failed to unmap memory.");
    }
}

absl::StatusOr<MemoryMappedFile> MemoryMappedFile::FromFilename(const std::string& filename)
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
    return MemoryMappedFile(std::move(file_handle), std::move(mapping_handle), std::move(dataMemoryMapped), valid_size);
}

MemoryMappedFile::MemoryMappedFile() : size_(0)
{}

absl::Span<const uint8_t> MemoryMappedFile::GetDataBuffer() const
{
    return absl::MakeSpan(mapped_memory_.GetValue(), size_);
}

MemoryMappedFile::MemoryMappedFile(ScopedHandle file_handle, ScopedHandle mapping_handle,
    ScopedMappedMemory mapped_memory, uint32_t size) : file_handle_(std::move(file_handle)), mapping_handle_(std::move(mapping_handle)), mapped_memory_(std::move(mapped_memory)), size_(size)
{
}

absl::StatusOr<std::vector<uint8_t>> ReadFileContents(std::string_view filename, std::size_t start,
    std::optional<std::size_t> size)
{
    auto file_handle = ScopedHandle(CreateFile(filename.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
    if (!file_handle.IsValid())
    {
        return absl::NotFoundError("Could not open file for reading");
    }

    return ReadFileContents(file_handle.GetValue(), start, size);
}
absl::StatusOr<std::string> ReadTextFileContents(std::string_view filename, std::size_t start,
    std::optional<std::size_t> size)
{
    auto data = ReadFileContents(filename, start, size);
    if (!data.ok())
    {
        return data.status();
    }

    return std::string(data->begin(), data->end());
}

absl::StatusOr<std::vector<uint8_t>> ReadFileContents(HANDLE file_handle, std::size_t start,
    std::optional<std::size_t> size)
{
    // If no length specifies, then until the end of the file.
    DWORD dwSizeHigh;
    DWORD dwSize = GetFileSize(file_handle, &dwSizeHigh);
    if (dwSize == INVALID_FILE_SIZE)
    {
        return absl::FailedPreconditionError("Unable to get file size.");
    }
    if (dwSizeHigh != 0)
    {
        return absl::FailedPreconditionError("File too large to read into memory.");
    }

    std::size_t end;

    if (size.has_value())
    {
        end = start + size.value();
        if (end > dwSize)
        {
            return absl::FailedPreconditionError("Requested size is too large.");
        }
    }
    else
    {
        end = dwSize;
    }

    std::size_t real_size = end - start;
    auto seekPos = SetFilePointer(file_handle, start, nullptr, FILE_BEGIN);

    if (seekPos == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
    {
        return absl::FailedPreconditionError("Failed to seek file.");
    }

    assert(seekPos == start);

    std::vector<uint8_t> data;
    data.resize(real_size);
    DWORD bytesRead;
    //
    if (!ReadFile(file_handle, data.data(), real_size, &bytesRead, nullptr))
    {
        return absl::FailedPreconditionError("Failed to read file.");
    }
    assert(bytesRead == real_size);
    return data;
}


