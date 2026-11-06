#include "proxy/DInput8Proxy.h"

#include <dinput.h>

#include <array>
#include <filesystem>
#include <mutex>

namespace lis::proxy {

namespace {

HMODULE RealModule() noexcept {
    static std::once_flag once;
    static HMODULE module = nullptr;
    std::call_once(once, [] {
        std::array<wchar_t, MAX_PATH> directory{};
        const UINT length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
        const std::filesystem::path path =
            std::filesystem::path(std::wstring_view(directory.data(), length)) / L"dinput8.dll";
        module = LoadLibraryW(path.c_str());
    });
    return module;
}

template <typename Function>
Function RealExport(const char* name) noexcept {
    HMODULE module = RealModule();
    return module != nullptr ? reinterpret_cast<Function>(GetProcAddress(module, name)) : nullptr;
}

}

DirectInput8CreateFunction RealDirectInput8Create() noexcept {
    static const auto function = RealExport<DirectInput8CreateFunction>("DirectInput8Create");
    return function;
}

}

extern "C" {

HRESULT WINAPI ProxyDirectInput8Create(HINSTANCE instance, DWORD version, REFIID interfaceId, LPVOID* output,
                                       LPUNKNOWN outer) {
    const auto function = lis::proxy::RealDirectInput8Create();
    return function != nullptr ? function(instance, version, interfaceId, output, outer) : E_FAIL;
}

HRESULT WINAPI ProxyDllCanUnloadNow() {
    using Function = HRESULT(WINAPI*)();
    const auto function = lis::proxy::RealExport<Function>("DllCanUnloadNow");
    return function != nullptr ? function() : S_FALSE;
}

HRESULT WINAPI ProxyDllGetClassObject(REFCLSID classId, REFIID interfaceId, LPVOID* output) {
    using Function = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    const auto function = lis::proxy::RealExport<Function>("DllGetClassObject");
    return function != nullptr ? function(classId, interfaceId, output) : CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI ProxyDllRegisterServer() {
    using Function = HRESULT(WINAPI*)();
    const auto function = lis::proxy::RealExport<Function>("DllRegisterServer");
    return function != nullptr ? function() : E_FAIL;
}

HRESULT WINAPI ProxyDllUnregisterServer() {
    using Function = HRESULT(WINAPI*)();
    const auto function = lis::proxy::RealExport<Function>("DllUnregisterServer");
    return function != nullptr ? function() : E_FAIL;
}

LPCDIDATAFORMAT WINAPI ProxyGetdfDIJoystick() {
    using Function = LPCDIDATAFORMAT(WINAPI*)();
    const auto function = lis::proxy::RealExport<Function>("GetdfDIJoystick");
    return function != nullptr ? function() : nullptr;
}
}
