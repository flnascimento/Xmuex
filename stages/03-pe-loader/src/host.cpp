// Entrada do runtime: mapeia o exe do jogo, resolve imports (shim -> DLL do jogo -> sistema -> trap) e inicia o jogo.
#include "host.h"
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

std::wstring g_localW, g_gameRootW, g_gameDirW;
int g_gameId = 1;
LoadedModule g_exe;
IUnknown* g_coreWindowUnk = nullptr;

static std::map<std::string, LoadedModule> g_mods;
static std::vector<std::string> g_initOrder;
static std::set<std::string> g_loading;
static CRITICAL_SECTION g_modCs;

// ---------------------------------------------------------------- log
static HANDLE g_logFile = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logCs;

void HLog(const char* fmt, ...) {
    if (g_logFile == INVALID_HANDLE_VALUE) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnprintf_s(buf, sizeof(buf) - 2, _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n < 0) n = (int)strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) --n;
    char line[1100];
    int m = sprintf_s(line, "[%8llu t%05lu] %.*s\r\n", GetTickCount64(), GetCurrentThreadId(), n, buf);
    EnterCriticalSection(&g_logCs);
    DWORD w;
    WriteFile(g_logFile, line, (DWORD)m, &w, nullptr);
    FlushFileBuffers(g_logFile);
    LeaveCriticalSection(&g_logCs);
}

bool HostFlagFile(const wchar_t* name) {
    return GetFileAttributesW((g_localW + L"\\" + name).c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string HostLower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static std::wstring ToW(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// ---------------------------------------------------------------- thunks
static BYTE* g_thunkCur = nullptr;
static size_t g_thunkUsed = 0;
static const size_t kThunkRegion = 1 << 20, kThunkSize = 24;

void* HostMakeThunk(void* handler, unsigned id) {
    if (!g_thunkCur || g_thunkUsed + kThunkSize > kThunkRegion) {
        HostSealThunks();
        g_thunkCur = (BYTE*)VirtualAllocFromApp(nullptr, kThunkRegion, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        g_thunkUsed = 0;
        if (!g_thunkCur) { HLog("ERRO: VirtualAllocFromApp (thunks) falhou: %lu", GetLastError()); return nullptr; }
    }
    BYTE* p = g_thunkCur + g_thunkUsed;
    g_thunkUsed += kThunkSize;
    p[0] = 0xB9; memcpy(p + 1, &id, 4);                 // mov ecx, imm32
    p[5] = 0x48; p[6] = 0xB8; memcpy(p + 7, &handler, 8); // mov rax, imm64
    p[15] = 0xFF; p[16] = 0xE0;                          // jmp rax
    return p;
}

void HostSealThunks() {
    if (!g_thunkCur) return;
    ULONG old;
    if (!VirtualProtectFromApp(g_thunkCur, kThunkRegion, PAGE_EXECUTE_READ, &old))
        HLog("ERRO: VirtualProtectFromApp (thunks) falhou: %lu", GetLastError());
    FlushInstructionCache(GetCurrentProcess(), g_thunkCur, kThunkRegion);
    g_thunkCur = nullptr;
}

// ---------------------------------------------------------------- traps (import sem implementação)
static std::vector<std::string> g_trapNames;
static std::set<unsigned> g_trapSeen;

static UINT_PTR TrapHandler(unsigned id) {
    EnterCriticalSection(&g_modCs);
    bool first = g_trapSeen.insert(id).second;
    const char* n = id < g_trapNames.size() ? g_trapNames[id].c_str() : "?";
    LeaveCriticalSection(&g_modCs);
    if (first) HLog("TRAP: chamada a import sem implementacao: %s (retornando 0)", n);
    return 0;
}

static void* MakeTrap(const std::string& name) {
    EnterCriticalSection(&g_modCs);
    unsigned id = (unsigned)g_trapNames.size();
    g_trapNames.push_back(name);
    void* t = HostMakeThunk((void*)&TrapHandler, id);
    LeaveCriticalSection(&g_modCs);
    return t;
}

// ---------------------------------------------------------------- módulos do sistema
typedef HMODULE(WINAPI* LoadLibraryW_t)(LPCWSTR);

// Modo estrito (LocalState\\strict.txt): simula no PC um sistema só com o que certamente existe no console.
static bool StrictBlocks(const std::string& lower) {
    static const bool strict = HostFlagFile(L"strict.txt");
    if (!strict) return false;
    static const char* const allow[] = {"kernelbase.dll", "ntdll.dll", "ucrtbase.dll", "vcruntime140_app.dll", "msvcp140_app.dll", "combase.dll", "d3d11.dll", "dxgi.dll", "d3dcompiler_47.dll", "xaudio2_9.dll"};
    for (auto* a : allow)
        if (lower == a) return false;
    return true;
}

HMODULE HostSysModule(const std::string& lower) {
    if (StrictBlocks(lower)) return nullptr;
    static LoadLibraryW_t realLoad = (LoadLibraryW_t)GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "LoadLibraryW");
    std::wstring w = ToW(lower);
    HMODULE h = GetModuleHandleW(w.c_str());
    if (!h) h = LoadPackagedLibrary(w.c_str(), 0);
    if (!h && realLoad) h = realLoad(w.c_str());
    return h;
}

void* HostSysProc(const char* dll, const char* name) {
    HMODULE h = HostSysModule(dll);
    return h ? (void*)GetProcAddress(h, name) : nullptr;
}

LoadedModule* HostModuleByName(const std::string& lower) {
    auto it = g_mods.find(lower);
    if (it != g_mods.end()) return &it->second;
    if (g_exe.base && g_exe.name == lower) return &g_exe;
    return nullptr;
}

LoadedModule* HostModuleByAddr(const void* addr) {
    auto in = [&](const LoadedModule& m) { return (const BYTE*)addr >= m.base && (const BYTE*)addr < m.base + m.size; };
    if (g_exe.base && in(g_exe)) return &g_exe;
    for (auto& kv : g_mods)
        if (in(kv.second)) return &kv.second;
    return nullptr;
}

// ---------------------------------------------------------------- TLS
// O código do jogo acessa TLS via gs:[58h][_tls_index], que só o loader do SO preenche. Usamos DLLs reais com bloco TLS
// reservado (ShTlsA/B/C): o SO cria o bloco em toda thread; aqui copiamos o template do jogo e apontamos o _tls_index dele.
// Exceção: o código do .exe do jogo usa gs:[58h][0] direto (índice 0 fixo), então o exe do jogo usa o bloco TLS deste .exe.
extern "C" ULONG _tls_index;
extern "C" const IMAGE_TLS_DIRECTORY64 _tls_used;
__declspec(thread) volatile char g_gameTlsReserve[4096];

static bool SetupExeTls(LoadedModule& m) {
    auto& d = m.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (!d.Size) return true;
    auto* t = (IMAGE_TLS_DIRECTORY64*)(m.base + d.VirtualAddress);
    size_t raw = (size_t)(t->EndAddressOfRawData - t->StartAddressOfRawData), total = raw + t->SizeOfZeroFill;
    void* tmpl = (void*)_tls_used.StartAddressOfRawData;
    size_t cap = (size_t)(_tls_used.EndAddressOfRawData - _tls_used.StartAddressOfRawData);
    g_gameTlsReserve[0] = 0;
    HLog("TLS exe: _tls_index do host=%lu, reserva=%zu, jogo precisa=%zu", _tls_index, cap, total);
    if (_tls_index != 0 || total > cap) { HLog("ERRO TLS: host precisa ter indice 0 e espaco suficiente"); return false; }
    ULONG old;
    VirtualProtectFromApp(tmpl, cap, PAGE_READWRITE, &old);
    memset(tmpl, 0, cap);
    memcpy(tmpl, (void*)t->StartAddressOfRawData, raw);
    *(DWORD*)t->AddressOfIndex = 0;
    return true;
}

typedef void(__cdecl* SlotInfo_t)(ULONG*, void**, size_t*);
extern "C" __declspec(dllimport) void __cdecl SlotInfoA(ULONG*, void**, size_t*);
extern "C" __declspec(dllimport) void __cdecl SlotInfoB(ULONG*, void**, size_t*);
extern "C" __declspec(dllimport) void __cdecl SlotInfoC(ULONG*, void**, size_t*);
static const SlotInfo_t kSlots[] = {SlotInfoA, SlotInfoB, SlotInfoC};
static int g_tlsNext = 0;

static bool SetupModuleTls(LoadedModule& m) {
    auto& d = m.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (!d.Size) return true;
    auto* t = (IMAGE_TLS_DIRECTORY64*)(m.base + d.VirtualAddress);
    size_t raw = (size_t)(t->EndAddressOfRawData - t->StartAddressOfRawData), total = raw + t->SizeOfZeroFill;
    if (g_tlsNext >= (int)(sizeof(kSlots) / sizeof(kSlots[0]))) { HLog("ERRO TLS: sem slots livres para %s", m.name.c_str()); return false; }
    auto info = kSlots[g_tlsNext++];
    ULONG idx = 0; void* tmpl = nullptr; size_t cap = 0;
    info(&idx, &tmpl, &cap);
    if (total > cap) { HLog("ERRO TLS: %s precisa de %zu bytes, slot tem %zu", m.name.c_str(), total, cap); return false; }
    ULONG old;
    VirtualProtectFromApp(tmpl, cap, PAGE_READWRITE, &old);
    memset(tmpl, 0, cap);
    memcpy(tmpl, (void*)t->StartAddressOfRawData, raw);
    *(DWORD*)t->AddressOfIndex = idx;
    HLog("TLS: %s -> slot %lu (%zu bytes, template @%p)", m.name.c_str(), idx, total, tmpl);
    if (t->AddressOfCallBacks && *(ULONGLONG*)t->AddressOfCallBacks) HLog("AVISO TLS: %s tem callbacks (nao chamados)", m.name.c_str());
    return true;
}

// ---------------------------------------------------------------- resolução de imports
static std::map<std::string, void*>* g_shimIndex;

static void BuildShimIndex() {
    g_shimIndex = new std::map<std::string, void*>();
    const ShimEntry* tables[] = {g_kernelShims, g_userShims, g_steamShims, g_dxShims, g_audioShims, g_inputShims, g_miscShims, g_netShims, g_modShims};
    for (auto* t : tables)
        for (; t->dll; ++t) (*g_shimIndex)[std::string(t->dll) + "!" + t->name] = t->fn;
}

static std::string AliasDll(const std::string& dll) {
    if (dll.rfind("api-ms-win-crt-", 0) == 0) return "ucrtbase.dll";
    if (dll == "vcruntime140.dll") return "vcruntime140_app.dll";
    if (dll == "msvcp140.dll") return "msvcp140_app.dll";
    return dll;
}

static void* ResolveImport(const std::string& dllIn, const char* func, WORD ord) {
    std::string dll = HostLower(dllIn);
    std::string fname = func ? std::string(func) : "#" + std::to_string(ord);
    std::string full = dll + "!" + fname;
    auto it = g_shimIndex->find(full);
    if (it != g_shimIndex->end()) return it->second;

    if (LoadedModule* own = HostEnsureModule(dll)) {
        void* f = func ? FindExport(*own, func) : nullptr;
        if (f) return f;
        HLog("AVISO: export ausente em modulo do jogo: %s", full.c_str());
        return MakeTrap(full);
    }

    const LPCSTR proc = func ? func : MAKEINTRESOURCEA(ord);
    std::string alias = AliasDll(dll);
    HMODULE h = HostSysModule(alias);
    void* f = h ? (void*)GetProcAddress(h, proc) : nullptr;
    if (!f && (dll == "kernel32.dll" || dll == "advapi32.dll")) { h = HostSysModule("kernelbase.dll"); f = h ? (void*)GetProcAddress(h, proc) : nullptr; }
    if (!f && dll == "kernel32.dll") { h = HostSysModule("ntdll.dll"); f = h ? (void*)GetProcAddress(h, proc) : nullptr; }
    if (!f && dll == "ole32.dll") { h = HostSysModule("combase.dll"); f = h ? (void*)GetProcAddress(h, proc) : nullptr; }
    if (f) return f;
    HLog("import sem resolucao (vira trap): %s", full.c_str());
    return MakeTrap(full);
}

LoadedModule* HostEnsureModule(const std::string& lower) {
    if (LoadedModule* m = HostModuleByName(lower)) return m;
    std::wstring path = g_gameDirW + L"\\" + ToW(lower);
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
    if (!g_loading.insert(lower).second) { HLog("ERRO: dependencia circular em %s", lower.c_str()); return nullptr; }
    LoadedModule m;
    MapStats st;
    std::string err;
    bool ok = MapImage(path, ResolveImport, m, st, err);
    g_loading.erase(lower);
    if (!ok) { HLog("ERRO ao mapear %s: %s (%lu)", lower.c_str(), err.c_str(), GetLastError()); return nullptr; }
    m.name = lower;
    HLog("modulo %s @%p (imports %zu, relocs %zu)", lower.c_str(), m.base, st.imports, st.relocs);
    auto& stored = g_mods[lower] = m;
    if (!SetupModuleTls(stored)) { g_mods.erase(lower); return nullptr; }
    g_initOrder.push_back(lower);
    return &stored;
}

// ---------------------------------------------------------------- diagnóstico de exceções
static LONG g_exCount = 0;

static LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    switch (code) {
    case 0xE06D7363: case 0xE0434352: case 0x406D1388: case 0x40010006: case 0x4001000A:
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (code < 0xC0000000) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&g_exCount) > 40) return EXCEPTION_CONTINUE_SEARCH;
    void* addr = ep->ExceptionRecord->ExceptionAddress;
    CONTEXT* c = ep->ContextRecord;
    LoadedModule* m = HostModuleByAddr(addr);
    char where[160];
    if (m) sprintf_s(where, "%s+0x%llx", m->name.c_str(), (unsigned long long)((BYTE*)addr - m->base));
    else sprintf_s(where, "%p", addr);
    HLog("EXCECAO 0x%08lx em %s alvo=%p op=%llu", code, where,
         ep->ExceptionRecord->NumberParameters > 1 ? (void*)ep->ExceptionRecord->ExceptionInformation[1] : nullptr,
         ep->ExceptionRecord->NumberParameters > 0 ? (unsigned long long)ep->ExceptionRecord->ExceptionInformation[0] : 0ull);
    HLog("  rax=%llx rcx=%llx rdx=%llx rbx=%llx rsp=%llx rbp=%llx rsi=%llx rdi=%llx r8=%llx r9=%llx", c->Rax, c->Rcx, c->Rdx, c->Rbx, c->Rsp, c->Rbp, c->Rsi, c->Rdi, c->R8, c->R9);
    // Pilha: endereços de retorno prováveis dentro de módulos do jogo
    ULONGLONG* sp = (ULONGLONG*)c->Rsp;
    int shown = 0;
    for (int i = 0; i < 256 && shown < 12; ++i) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(sp + i, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) break;
        LoadedModule* rm = HostModuleByAddr((void*)sp[i]);
        if (rm) { HLog("  pilha[%d] %s+0x%llx", i, rm->name.c_str(), sp[i] - (ULONGLONG)rm->base); ++shown; }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------- watchdog: mostra onde a thread principal do jogo está
typedef DWORD(WINAPI* SuspendThread_t)(HANDLE);
typedef DWORD(WINAPI* ResumeThread_t)(HANDLE);
typedef BOOL(WINAPI* GetThreadContext_t)(HANDLE, CONTEXT*);
static HANDLE g_gameThread = nullptr;

static void DumpThread(HANDLE th, const char* label) {
    static auto sus = (SuspendThread_t)HostSysProc("kernelbase.dll", "SuspendThread");
    static auto res = (ResumeThread_t)HostSysProc("kernelbase.dll", "ResumeThread");
    static auto ctxf = (GetThreadContext_t)HostSysProc("kernelbase.dll", "GetThreadContext");
    if (!sus || !res || !ctxf) return;
    if (sus(th) == (DWORD)-1) return;
    CONTEXT c{};
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (ctxf(th, &c)) {
        LoadedModule* m = HostModuleByAddr((void*)c.Rip);
        char where[128];
        if (m) sprintf_s(where, "%s+0x%llx", m->name.c_str(), c.Rip - (ULONGLONG)m->base);
        else sprintf_s(where, "%llx", c.Rip);
        char line[900];
        int n = sprintf_s(line, "%s rip=%s pilha:", label, where);
        ULONGLONG* sp = (ULONGLONG*)c.Rsp;
        int shown = 0;
        for (int i = 0; i < 400 && shown < 10; ++i) {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery(sp + i, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) break;
            LoadedModule* rm = HostModuleByAddr((void*)sp[i]);
            if (rm && rm == &g_exe) { n += sprintf_s(line + n, sizeof(line) - n, " 0x%llx", sp[i] - (ULONGLONG)rm->base); ++shown; }
        }
        res(th);
        HLog("%s", line);
        return;
    }
    res(th);
}

static DWORD WINAPI WatchdogThread(void*) {
    bool always = HostFlagFile(L"watchdog.txt");
    for (int i = 0; always || i < 30; ++i) {
        Sleep(always ? 5000 : 10000);
        if (g_gameThread) DumpThread(g_gameThread, "watchdog principal");
    }
    return 0;
}

// ---------------------------------------------------------------- execução
typedef BOOL(WINAPI* DllMain_t)(HINSTANCE, DWORD, LPVOID);

static DWORD WINAPI GameThread(void*) {
    HLog("thread do jogo iniciada");
    for (auto& name : g_initOrder) {
        LoadedModule* m = HostModuleByName(name);
        if (m && m->entry) {
            BOOL r = ((DllMain_t)m->entry)((HINSTANCE)m->base, DLL_PROCESS_ATTACH, nullptr);
            HLog("DllMain %s -> %d", name.c_str(), r);
        }
    }
    HLog("chamando entrypoint do jogo");
    int rc = ((int(*)())g_exe.entry)();
    HLog("entrypoint retornou %d", rc);
    return (DWORD)rc;
}

int HostStart(const wchar_t* gameRoot, int game) {
    static LONG started = 0;
    if (InterlockedExchange(&started, 1)) return 1;

    InitializeCriticalSectionEx(&g_logCs, 4000, 0);
    InitializeCriticalSectionEx(&g_modCs, 4000, 0);
    g_gameRootW = gameRoot;
    g_localW = g_gameRootW.substr(0, g_gameRootW.find_last_of(L'\\'));
    g_gameId = game;
    g_gameDirW = g_gameRootW + (game == 2 ? L"\\sm2" : L"\\sm1");

    g_logFile = CreateFileW((g_localW + L"\\host.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HLog("Shenmue host v0.10 jogo=%d dir=%ls", game, g_gameDirW.c_str());

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    HLog("memoria: fisica %llu MB livres de %llu MB; limite commit %llu MB", ms.ullAvailPhys >> 20, ms.ullTotalPhys >> 20, ms.ullTotalPageFile >> 20);

    typedef BOOL(WINAPI* SetCwd_t)(LPCWSTR);
    auto setCwd = (SetCwd_t)GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "SetCurrentDirectoryW");
    BOOL cwdOk = setCwd ? setCwd(g_gameDirW.c_str()) : FALSE;
    HLog("SetCurrentDirectory -> %d (%lu)", cwdOk, GetLastError());

    AddVectoredExceptionHandler(1, VectoredHandler);
    BuildShimIndex();
    SteamShimInit();

    std::wstring exe = g_gameDirW + (game == 2 ? L"\\Shenmue2.exe" : L"\\Shenmue.exe");
    MapStats st;
    std::string err;
    if (!MapImage(exe, ResolveImport, g_exe, st, err)) {
        HLog("ERRO ao mapear exe: %s (%lu)", err.c_str(), GetLastError());
        return 2;
    }
    g_exe.name = game == 2 ? "shenmue2.exe" : "shenmue.exe";
    HLog("exe @%p entry=%p seçoes=%zu relocs=%zu imports=%zu", g_exe.base, g_exe.entry, st.sections, st.relocs, st.imports);
    if (!SetupExeTls(g_exe)) return 3;
    ModsApply(g_exe, game);
    HostSealThunks();

    size_t stack = g_exe.nt->OptionalHeader.SizeOfStackReserve;
    if (stack < (16u << 20)) stack = 16u << 20;
    HANDLE th = CreateThread(nullptr, stack, GameThread, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!th) { HLog("ERRO CreateThread: %lu", GetLastError()); return 4; }
    g_gameThread = th;
    if (HANDLE w = CreateThread(nullptr, 0, WatchdogThread, nullptr, 0, nullptr)) CloseHandle(w);
    return 0;
}
