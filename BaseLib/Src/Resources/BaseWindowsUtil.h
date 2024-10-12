#pragma once

#include <string>
#include <Windows.h>

inline HRESULT ResultFromLastError() { return HRESULT_FROM_WIN32(GetLastError()); }

struct ScopedHandle
{
    ScopedHandle() : hFile(INVALID_HANDLE_VALUE) {}
    void Close()
    {
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }
    }
    virtual ~ScopedHandle()
    {
        Close();
    }

    HANDLE hFile;
};

struct ScopedFile : public ScopedHandle
{
    ScopedFile(const std::string& filename, DWORD desiredAccess, DWORD shareMode, DWORD creationDisposition);
    void Write(const uint8_t* data, uint32_t length);
    uint32_t GetLength();
    uint32_t SeekToEnd();

    std::string filename;
};
void deletefile(const std::string& filename);
void movefile(const std::string& from, const std::string& to);

std::string GetMessageFromLastError(const std::string& details);