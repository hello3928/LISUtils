#pragma once

#include <Windows.h>

#include <filesystem>

namespace lis::core {

void RegisterModule(HMODULE module) noexcept;

[[nodiscard]] HMODULE ModuleHandle() noexcept;
[[nodiscard]] std::filesystem::path ModuleDirectory();
[[nodiscard]] std::filesystem::path GameRootDirectory();
[[nodiscard]] std::filesystem::path UserDataDirectory();

}
