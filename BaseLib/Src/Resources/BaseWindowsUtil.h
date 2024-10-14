#pragma once

#include <string>
#include <Windows.h>

inline HRESULT ResultFromLastError() { return HRESULT_FROM_WIN32(GetLastError()); }

struct OldScopedHandle
{
    OldScopedHandle() : hFile(INVALID_HANDLE_VALUE) {}
    void Close()
    {
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }
    }
    virtual ~OldScopedHandle()
    {
        Close();
    }

    HANDLE hFile;
};

struct OldScopedFile : public OldScopedHandle
{
    OldScopedFile(const std::string& filename, DWORD desiredAccess, DWORD shareMode, DWORD creationDisposition);
    void Write(const uint8_t* data, uint32_t length);
    uint32_t GetLength();
    uint32_t SeekToEnd();

    std::string filename;
};
void deletefile(const std::string& filename);
void movefile(const std::string& from, const std::string& to);

std::string GetMessageFromLastError(const std::string& details);