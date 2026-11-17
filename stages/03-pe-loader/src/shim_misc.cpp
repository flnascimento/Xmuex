// DLLs auxiliares que não existem (ou não importam) no console: respostas neutras.
#include "host.h"

static HIMC WINAPI S_ImmGetContext(HWND) { return nullptr; }
static BOOL WINAPI S_ImmSetCompositionWindow(HIMC, void*) { return TRUE; }
static HRESULT WINAPI S_DwmGetWindowAttribute(HWND, DWORD, PVOID, DWORD) { return E_FAIL; }
static BOOL WINAPI S_MiniDumpWriteDump(void*, DWORD, void*, int, void*, void*, void*) { return FALSE; }
static BOOL WINAPI S_SymInitialize(HANDLE, LPCSTR, BOOL) { return FALSE; }
static BOOL WINAPI S_SymFromAddr(HANDLE, DWORD64, PDWORD64, void*) { return FALSE; }
static BOOL WINAPI S_SymCleanup(HANDLE) { return TRUE; }
static BOOL WINAPI S_SymGetLineFromAddr64(HANDLE, DWORD64, PDWORD, void*) { return FALSE; }
static BOOL WINAPI S_SymGetSymFromAddr64(HANDLE, DWORD64, PDWORD64, void*) { return FALSE; }
static DWORD WINAPI S_SymSetOptions(DWORD o) { return o; }
static int WINAPI S_IdnToAscii(DWORD, LPCWSTR, int, LPWSTR, int) { return 0; }
static LSTATUS WINAPI S_RegOpenKeyExA(HKEY, LPCSTR k, DWORD, REGSAM, PHKEY out) {
    HLog("RegOpenKeyExA(%s) -> nao encontrado", k ? k : "");
    if (out) *out = nullptr;
    return ERROR_FILE_NOT_FOUND;
}
static LSTATUS WINAPI S_RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) { return ERROR_FILE_NOT_FOUND; }

static LPWSTR* WINAPI S_CommandLineToArgvW(LPCWSTR, int* argc) {
    static wchar_t arg0[] = L"Shenmue.exe";
    static LPWSTR argv[] = {arg0, nullptr};
    *argc = 1;
    return argv;
}
static DWORD WINAPI S_timeGetTime() { return (DWORD)GetTickCount64(); }
static UINT WINAPI S_timeBeginPeriod(UINT) { return 0; }
static UINT WINAPI S_timeEndPeriod(UINT) { return 0; }

const ShimEntry g_miscShims[] = {
    {"shell32.dll", "CommandLineToArgvW", (void*)&S_CommandLineToArgvW},
    {"winmm.dll", "timeGetTime", (void*)&S_timeGetTime},
    {"winmm.dll", "timeBeginPeriod", (void*)&S_timeBeginPeriod},
    {"winmm.dll", "timeEndPeriod", (void*)&S_timeEndPeriod},
    {"imm32.dll", "ImmGetContext", (void*)&S_ImmGetContext},
    {"imm32.dll", "ImmSetCompositionWindow", (void*)&S_ImmSetCompositionWindow},
    {"dwmapi.dll", "DwmGetWindowAttribute", (void*)&S_DwmGetWindowAttribute},
    {"dbghelp.dll", "MiniDumpWriteDump", (void*)&S_MiniDumpWriteDump},
    {"dbghelp.dll", "SymInitialize", (void*)&S_SymInitialize},
    {"dbghelp.dll", "SymFromAddr", (void*)&S_SymFromAddr},
    {"dbghelp.dll", "SymCleanup", (void*)&S_SymCleanup},
    {"dbghelp.dll", "SymGetLineFromAddr64", (void*)&S_SymGetLineFromAddr64},
    {"dbghelp.dll", "SymGetSymFromAddr64", (void*)&S_SymGetSymFromAddr64},
    {"dbghelp.dll", "SymSetOptions", (void*)&S_SymSetOptions},
    {"normaliz.dll", "IdnToAscii", (void*)&S_IdnToAscii},
    {"advapi32.dll", "RegOpenKeyExA", (void*)&S_RegOpenKeyExA},
    {"advapi32.dll", "RegQueryValueExA", (void*)&S_RegQueryValueExA},
    {nullptr, nullptr, nullptr},
};
