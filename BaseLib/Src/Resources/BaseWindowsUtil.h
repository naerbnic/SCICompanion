#pragma once

#include <Windows.h>

inline HRESULT ResultFromLastError() { return HRESULT_FROM_WIN32(GetLastError()); }