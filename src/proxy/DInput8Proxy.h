#pragma once

#include <Windows.h>

#include <Unknwn.h>

namespace lis::proxy {

using DirectInput8CreateFunction = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

[[nodiscard]] DirectInput8CreateFunction RealDirectInput8Create() noexcept;

}
