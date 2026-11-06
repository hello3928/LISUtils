#include "App.h"
#include "core/Paths.h"

#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        lis::core::RegisterModule(module);
        lis::App::Launch();
        break;
    case DLL_PROCESS_DETACH:
        lis::App::ReleaseOnExit();
        break;
    default:
        break;
    }
    return TRUE;
}
