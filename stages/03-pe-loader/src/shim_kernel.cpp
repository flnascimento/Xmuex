// kernel32 e CRT: faz o jogo enxergar os módulos mapeados manualmente como se fossem carregados pelo SO.
#include "host.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <exception>

static std::string Narrow(const std::wstring& w) { return std::string(w.begin(), w.end()); }

static LoadedModule* ModByNameA(const char* n) {
    std::string s = HostLower(n);
    size_t p = s.find_last_of("\\/");
    if (p != std::string::npos) s = s.substr(p + 1);
    if (s.find('.') == std::string::npos) s += ".dll";
    return HostModuleByName(s);
}

static HMODULE WINAPI S_GetModuleHandleA(LPCSTR n) {
    if (!n) return (HMODULE)g_exe.base;
    if (auto* m = ModByNameA(n)) return (HMODULE)m->base;
    return GetModuleHandleA(n);
}

static HMODULE WINAPI S_GetModuleHandleW(LPCWSTR n) {
    if (!n) return (HMODULE)g_exe.base;
    if (auto* m = ModByNameA(Narrow(n).c_str())) return (HMODULE)m->base;
    return GetModuleHandleW(n);
}

static BOOL WINAPI S_GetModuleHandleExW(DWORD flags, LPCWSTR n, HMODULE* out) {
    if (flags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS) {
        if (auto* m = HostModuleByAddr(n)) { *out = (HMODULE)m->base; return TRUE; }
    } else {
        if (!n) { *out = (HMODULE)g_exe.base; return TRUE; }
        if (auto* m = ModByNameA(Narrow(n).c_str())) { *out = (HMODULE)m->base; return TRUE; }
    }
    return GetModuleHandleExW(flags, n, out);
}

static std::wstring ModulePath(HMODULE h, bool& ours) {
    ours = true;
    if (!h || (BYTE*)h == g_exe.base) return g_exe.path;
    if (auto* m = HostModuleByAddr(h)) return m->path;
    ours = false;
    return L"";
}

static DWORD WINAPI S_GetModuleFileNameW(HMODULE h, LPWSTR buf, DWORD size) {
    bool ours;
    std::wstring p = ModulePath(h, ours);
    if (!ours) return GetModuleFileNameW(h, buf, size);
    wcsncpy_s(buf, size, p.c_str(), _TRUNCATE);
    return (DWORD)wcslen(buf);
}

static DWORD WINAPI S_GetModuleFileNameA(HMODULE h, LPSTR buf, DWORD size) {
    bool ours;
    std::wstring p = ModulePath(h, ours);
    if (!ours) return GetModuleFileNameA(h, buf, size);
    strncpy_s(buf, size, Narrow(p).c_str(), _TRUNCATE);
    return (DWORD)strlen(buf);
}

// Handle falso devolvido para DLLs que substituímos por shims (o jogo só testa != NULL).
static BYTE g_fakeModule[16];

static bool IsReplacedDll(const std::string& s) {
    return s == "xaudio2_7.dll" || s == "xaudio2_7" || s == "xapofx1_5.dll" || s == "xinput1_3.dll" || s == "steam_api64.dll" || s == "d3dcompiler_43.dll";
}

static HMODULE LoadCommon(const std::string& nameA, const std::wstring& nameW) {
    std::string s = HostLower(nameA);
    size_t p = s.find_last_of("\\/");
    if (p != std::string::npos) s = s.substr(p + 1);
    HLog("LoadLibrary(%s)", s.c_str());
    if (IsReplacedDll(s)) return (HMODULE)g_fakeModule;
    if (auto* m = HostEnsureModule(s)) { HostSealThunks(); return (HMODULE)m->base; }
    HMODULE h = HostSysModule(s);
    if (!h) HLog("  LoadLibrary falhou: %s", s.c_str());
    (void)nameW;
    return h;
}

static HMODULE WINAPI S_LoadLibraryA(LPCSTR n) { return LoadCommon(n, L""); }
static HMODULE WINAPI S_LoadLibraryW(LPCWSTR n) { return LoadCommon(Narrow(n), n); }
static HMODULE WINAPI S_LoadLibraryExA(LPCSTR n, HANDLE, DWORD) { return LoadCommon(n, L""); }
static HMODULE WINAPI S_LoadLibraryExW(LPCWSTR n, HANDLE, DWORD) { return LoadCommon(Narrow(n), n); }

static FARPROC WINAPI S_GetProcAddress(HMODULE h, LPCSTR name) {
    if ((BYTE*)h == g_fakeModule) { HLog("GetProcAddress em DLL substituida: %s", HIWORD(name) ? name : "#ord"); return nullptr; }
    if (auto* m = HostModuleByAddr(h)) return HIWORD(name) ? (FARPROC)FindExport(*m, name) : nullptr;
    return GetProcAddress(h, name);
}

static BOOL WINAPI S_FreeLibrary(HMODULE h) {
    if ((BYTE*)h == g_fakeModule || HostModuleByAddr(h)) return TRUE;
    return FreeLibrary(h);
}

static LPSTR WINAPI S_GetCommandLineA() { static char c[] = "Shenmue.exe"; return c; }
static LPWSTR WINAPI S_GetCommandLineW() { static wchar_t c[] = L"Shenmue.exe"; return c; }

static BOOL WINAPI S_CreateProcessA(LPCSTR app, LPSTR cmd, void*, void*, BOOL, DWORD, void*, LPCSTR, void*, void*) {
    HLog("CreateProcessA bloqueado: %s %s", app ? app : "", cmd ? cmd : "");
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

static LPVOID WINAPI S_VirtualAlloc(LPVOID addr, SIZE_T size, DWORD type, DWORD prot) {
    if (prot & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) {
        LPVOID p = VirtualAllocFromApp(addr, size, type, PAGE_READWRITE);
        ULONG old;
        if (p) VirtualProtectFromApp(p, size, prot, &old);
        return p;
    }
    return VirtualAllocFromApp(addr, size, type, prot);
}

static BOOL WINAPI S_VirtualProtect(LPVOID addr, SIZE_T size, DWORD prot, PDWORD old) {
    ULONG o = 0;
    BOOL r = VirtualProtectFromApp(addr, size, prot, &o);
    if (old) *old = o;
    return r;
}

static void WINAPI S_OutputDebugStringA(LPCSTR s) { if (s) HLog("dbg: %s", s); }
static void WINAPI S_OutputDebugStringW(LPCWSTR s) { if (s) HLog("dbg: %ls", s); }

static void WINAPI S_ExitProcess(UINT code) {
    HLog("ExitProcess(%u)", code);
    TerminateProcess(GetCurrentProcess(), code);
}

static BOOL WINAPI S_TerminateProcess(HANDLE h, UINT code) {
    HLog("TerminateProcess(%u)", code);
    return TerminateProcess(h, code);
}

static LPTOP_LEVEL_EXCEPTION_FILTER g_gameFilter;
static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI S_SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER f) {
    auto old = g_gameFilter;
    g_gameFilter = f;
    return old; // mantém o filtro do processo; o VEH do host já registra as falhas
}

// stdout/stderr do jogo vão para o log.
static int __cdecl S_vfprintf(unsigned __int64 opt, FILE* stream, const char* fmt, _locale_t loc, va_list args) {
    if (stream == __acrt_iob_func(1) || stream == __acrt_iob_func(2)) {
        char buf[900];
        vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
        HLog("jogo: %s", buf);
        return (int)strlen(buf);
    }
    return __stdio_common_vfprintf(opt, stream, fmt, loc, args);
}

// O original usa RtlPcToFileHeader para achar a base da imagem, que não conhece módulos mapeados manualmente.
static void __stdcall S_CxxThrowException(void* obj, void* info) {
    ULONG_PTR a[4];
    LoadedModule* m = info ? HostModuleByAddr(info) : nullptr;
    bool pure = info && (*(DWORD*)info & 0x8);
    a[0] = pure ? 0x01994000 : 0x19930520;
    a[1] = (ULONG_PTR)obj;
    a[2] = (ULONG_PTR)info;
    a[3] = m ? (ULONG_PTR)m->base : 0;
    RaiseException(0xE06D7363, EXCEPTION_NONCONTINUABLE, 4, a);
}

static void __cdecl S_exit(int code) {
    HLog("exit(%d)", code);
    exit(code);
}

static void __cdecl S__exit(int code) {
    HLog("_exit(%d)", code);
    _exit(code);
}

static void __cdecl S_terminate() {
    HLog("terminate()");
    terminate();
}

static void __cdecl S_wassert(const wchar_t* msg, const wchar_t* file, unsigned line) {
    HLog("assert: %ls (%ls:%u)", msg ? msg : L"", file ? file : L"", line);
}

static LPSTR WINAPI S_lstrcpyA(LPSTR dst, LPCSTR src) { return strcpy(dst, src); }
static BOOL WINAPI S_VerifyVersionInfoA(void*, DWORD, DWORDLONG) { return TRUE; } // o console é Windows 10+
static HANDLE WINAPI S_CreateSemaphoreA(LPSECURITY_ATTRIBUTES a, LONG init, LONG max, LPCSTR) { return CreateSemaphoreExW(a, init, max, nullptr, 0, SEMAPHORE_ALL_ACCESS); }

#define K(n) {"kernel32.dll", #n, (void*)&S_##n}
const ShimEntry g_kernelShims[] = {
    K(GetModuleHandleA), K(GetModuleHandleW), K(GetModuleHandleExW), K(GetModuleFileNameA), K(GetModuleFileNameW),
    K(LoadLibraryA), K(LoadLibraryW), K(LoadLibraryExA), K(LoadLibraryExW), K(GetProcAddress), K(FreeLibrary),
    K(GetCommandLineA), K(GetCommandLineW), K(CreateProcessA), K(VirtualAlloc), K(VirtualProtect),
    K(OutputDebugStringA), K(OutputDebugStringW), K(ExitProcess), K(TerminateProcess), K(SetUnhandledExceptionFilter),
    K(lstrcpyA), K(VerifyVersionInfoA), K(CreateSemaphoreA),
    {"api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vfprintf", (void*)&S_vfprintf},
    {"api-ms-win-crt-runtime-l1-1-0.dll", "exit", (void*)&S_exit},
    {"api-ms-win-crt-runtime-l1-1-0.dll", "_exit", (void*)&S__exit},
    {"api-ms-win-crt-runtime-l1-1-0.dll", "terminate", (void*)&S_terminate},
    {"api-ms-win-crt-runtime-l1-1-0.dll", "_wassert", (void*)&S_wassert},
    {"vcruntime140.dll", "_CxxThrowException", (void*)&S_CxxThrowException},
    {nullptr, nullptr, nullptr},
};
