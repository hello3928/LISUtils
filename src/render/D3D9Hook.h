#pragma once

#include "core/Hook.h"

#include <Windows.h>
#include <d3d9.h>

#include <cstdint>
#include <expected>
#include <functional>
#include <string>

namespace lis::render {

enum class ResetStage : std::uint8_t { Before, After };

class D3D9Hook {
public:
    using PresentHandler = std::function<void(IDirect3DDevice9*)>;
    using ResetHandler = std::function<void(IDirect3DDevice9*, ResetStage, bool)>;

    D3D9Hook(PresentHandler onPresent, ResetHandler onReset);
    D3D9Hook(const D3D9Hook&) = delete;
    D3D9Hook& operator=(const D3D9Hook&) = delete;
    D3D9Hook(D3D9Hook&&) = delete;
    D3D9Hook& operator=(D3D9Hook&&) = delete;
    ~D3D9Hook();

    [[nodiscard]] std::expected<void, std::string> Install();

private:
    using PresentFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    using ResetFunction = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

    static HRESULT WINAPI PresentDetour(IDirect3DDevice9* device, const RECT* source, const RECT* destination,
                                        HWND window, const RGNDATA* dirtyRegion);
    static HRESULT WINAPI ResetDetour(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters);

    PresentHandler onPresent_;
    ResetHandler onReset_;
    core::ScopedHook present_;
    core::ScopedHook reset_;

    static inline D3D9Hook* current = nullptr;
};

}
