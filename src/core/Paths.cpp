#include "core/Paths.h"

#include <ShlObj.h>
#include <array>

namespace lis::core {

namespace {

HMODULE moduleHandle = nullptr;

}

void RegisterModule(HMODULE module) noexcept {
    moduleHandle = module;
}

HMODULE ModuleHandle() noexcept {
    return moduleHandle;
}

std::filesystem::path ModuleDirectory() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(moduleHandle, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring_view(buffer.data(), length)).parent_path();
}

std::filesystem::path GameRootDirectory() {
    return ModuleDirectory().parent_path().parent_path();
}

std::filesystem::path UserDataDirectory() {
    PWSTR documents = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents))) {
        result = std::filesystem::path(documents) / L"My Games" / L"Life Is Strange";
    }
    CoTaskMemFree(documents);
    return result;
}

}
