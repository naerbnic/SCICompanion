#include "BaseWindowsUtil.h"

#include <string>
#include <Shlwapi.h>
#include <strsafe.h>

ScopedFile::ScopedFile(const std::string& filename, DWORD desiredAccess, DWORD shareMode, DWORD creationDisposition) : filename(filename)
{
    hFile = CreateFile(filename.c_str(), desiredAccess, shareMode, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::string details = "Opening ";
        details += filename;
        throw std::exception(GetMessageFromLastError(details).c_str());
    }
}

void ScopedFile::Write(const uint8_t* data, uint32_t length)
{
    DWORD cbWritten = 0;
    if (length > 0)
    {
        if (!WriteFile(hFile, data, length, &cbWritten, nullptr) || (cbWritten != length))
        {
            std::string details = "Writing to ";
            details += filename;
            throw std::exception(GetMessageFromLastError(details).c_str());
        }
    }
}

uint32_t ScopedFile::SeekToEnd()
{
    uint32_t position = SetFilePointer(hFile, 0, nullptr, FILE_END);
    if (position == INVALID_SET_FILE_POINTER)
    {
        throw std::exception("Can't seek to end");
    }
    return position;
}

uint32_t ScopedFile::GetLength()
{
    DWORD upperSize;
    uint32_t size = GetFileSize(hFile, &upperSize);
    if (upperSize > 0)
    {
        throw std::exception("File too large.");
    }
    return size;
}

void deletefile(const std::string& filename)
{
    if (PathFileExists(filename.c_str()))
    {
        if (!DeleteFile(filename.c_str()))
        {
            std::string details = "Deleting ";
            details += filename;
            throw std::exception(GetMessageFromLastError(details).c_str());
        }
    }
}

void movefile(const std::string& from, const std::string& to)
{
    if (!MoveFile(from.c_str(), to.c_str()))
    {
        std::string details = "Moving ";
        details += from;
        details += " to ";
        details += to;
        throw std::exception(GetMessageFromLastError(details).c_str());
    }
}

// Ugly code straight off MSDN
std::string GetMessageFromLastError(const std::string& details)
{
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)details.c_str()) + 40) * sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDisplayBuf,
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"),
        details.c_str(), dw, lpMsgBuf);

    std::string message = (LPCTSTR)lpDisplayBuf;

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);

    return message;
}
