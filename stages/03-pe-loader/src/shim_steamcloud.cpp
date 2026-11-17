// ISteamRemoteStorage014 (Steam Cloud) sobre arquivos locais. É por aqui que o jogo grava saves e opções (inclusive o
// idioma das legendas). Mesmo formato da pasta remote da Steam no PC (nomes como "sm1saves/SHENMUE1_000"), em
// LocalState\cloud, compartilhada pelos dois jogos (mesmo appid) e preservada ao apagar um jogo no launcher.
// Também implementa os "call results" da Steam, usados pelas versões assíncronas (FileWriteAsync/FileReadAsync).
#include "host.h"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

typedef unsigned long long SteamAPICall;

static SRWLOCK g_cloudLock = SRWLOCK_INIT;
struct CloudLock {
    CloudLock() { AcquireSRWLockExclusive(&g_cloudLock); }
    ~CloudLock() { ReleaseSRWLockExclusive(&g_cloudLock); }
};

// ---------------------------------------------------------------- call results
struct CallResult { SteamAPICall h; std::vector<BYTE> data; };
static SRWLOCK g_cbLock = SRWLOCK_INIT;
static std::vector<CallResult> g_done;                         // resultados prontos
static std::vector<std::pair<void*, SteamAPICall>> g_waiting;  // objetos CCallResult registrados
static SteamAPICall g_nextCall = 0x1000;

static SteamAPICall NewCall() {
    AcquireSRWLockExclusive(&g_cbLock);
    SteamAPICall h = ++g_nextCall;
    ReleaseSRWLockExclusive(&g_cbLock);
    return h;
}

static void CompleteCall(SteamAPICall h, const void* data, size_t size) {
    AcquireSRWLockExclusive(&g_cbLock);
    g_done.push_back({h, std::vector<BYTE>((const BYTE*)data, (const BYTE*)data + size)});
    ReleaseSRWLockExclusive(&g_cbLock);
}

void SteamRegisterCallResult(void* cb, SteamAPICall h) {
    AcquireSRWLockExclusive(&g_cbLock);
    g_waiting.push_back({cb, h});
    ReleaseSRWLockExclusive(&g_cbLock);
}

void SteamUnregisterCallResult(void* cb, SteamAPICall h) {
    AcquireSRWLockExclusive(&g_cbLock);
    for (auto it = g_waiting.begin(); it != g_waiting.end(); ++it)
        if (it->first == cb && it->second == h) { g_waiting.erase(it); break; }
    ReleaseSRWLockExclusive(&g_cbLock);
}

// Chamado em SteamAPI_RunCallbacks. CCallbackBase no MSVC: vtable[0] = Run(void* param, bool ioFailure, SteamAPICall h)
// (sobrecargas ficam em ordem inversa; conferido no exe do Shenmue).
typedef void(__fastcall* RunResult_t)(void* self, void* param, bool ioFailure, SteamAPICall h);

void SteamDispatchCallResults() {
    std::vector<std::pair<void*, CallResult>> ready;
    AcquireSRWLockExclusive(&g_cbLock);
    for (auto w = g_waiting.begin(); w != g_waiting.end();) {
        auto d = g_done.begin();
        while (d != g_done.end() && d->h != w->second) ++d;
        if (d == g_done.end()) { ++w; continue; }
        ready.push_back({w->first, std::move(*d)});
        g_done.erase(d);
        w = g_waiting.erase(w);
    }
    ReleaseSRWLockExclusive(&g_cbLock);
    for (auto& r : ready) {
        auto run = (RunResult_t)(*(void***)r.first)[0];
        run(r.first, r.second.data.data(), false, r.second.h);
    }
}

// ---------------------------------------------------------------- arquivos
static std::wstring CloudDir() { return g_localW + L"\\cloud"; }

// ".\sm1saves\X" -> "sm1saves/X" (como a Steam guarda). Vazio se o nome for inválido.
static std::string CloudName(const char* in) {
    if (!in) return "";
    std::string s;
    for (const char* c = in; *c; ++c) {
        char ch = *c == '\\' ? '/' : *c;
        if (ch == '/' && !s.empty() && s.back() == '/') continue;
        s += ch;
    }
    while (s.rfind("./", 0) == 0) s.erase(0, 2);
    while (!s.empty() && s[0] == '/') s.erase(0, 1);
    if (s.empty() || s.find("..") != std::string::npos || s.find(':') != std::string::npos) return "";
    return s;
}

static std::wstring CloudPath(const std::string& name) {
    std::wstring w = CloudDir() + L"\\";
    for (char c : name) w += c == '/' ? L'\\' : (wchar_t)(unsigned char)c;
    return w;
}

static void MakeParents(const std::wstring& path) {
    for (size_t p = CloudDir().size(); (p = path.find(L'\\', p + 1)) != std::wstring::npos;)
        CreateDirectoryW(path.substr(0, p).c_str(), nullptr);
    CreateDirectoryW(CloudDir().c_str(), nullptr);
}

static bool ReadRange(const std::wstring& path, std::vector<BYTE>& out, DWORD offset, DWORD maxBytes) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{}, pos{};
    GetFileSizeEx(f, &sz);
    DWORD avail = sz.QuadPart > offset ? (DWORD)std::min<LONGLONG>(sz.QuadPart - offset, (LONGLONG)maxBytes) : 0;
    out.resize(avail);
    pos.QuadPart = offset;
    DWORD got = 0;
    bool ok = SetFilePointerEx(f, pos, nullptr, FILE_BEGIN) && (avail == 0 || ReadFile(f, out.data(), avail, &got, nullptr));
    CloseHandle(f);
    out.resize(got);
    return ok;
}

typedef BOOL(WINAPI* MoveFileExW_t)(LPCWSTR, LPCWSTR, DWORD);

static bool WriteAll(const std::wstring& path, const void* data, DWORD size) {
    MakeParents(path);
    std::wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    bool ok = size == 0 || (WriteFile(f, data, size, &w, nullptr) && w == size);
    FlushFileBuffers(f);
    CloseHandle(f);
    static auto move = (MoveFileExW_t)HostSysProc("kernelbase.dll", "MoveFileExW");
    if (ok && move) ok = move(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0; // troca atômica: save nunca fica pela metade
    else if (ok) { DeleteFileW(path.c_str()); ok = MoveFileW(tmp.c_str(), path.c_str()) != 0; }
    if (!ok) DeleteFileW(tmp.c_str());
    return ok;
}

static bool Info(const std::string& name, WIN32_FILE_ATTRIBUTE_DATA& a) {
    return !name.empty() && GetFileAttributesExW(CloudPath(name).c_str(), GetFileExInfoStandard, &a) && !(a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

// ---------------------------------------------------------------- ISteamRemoteStorage014 (this em rcx)
static bool FileWrite(void*, const char* file, const void* data, int size) {
    std::string n = CloudName(file);
    CloudLock lk;
    bool ok = !n.empty() && size >= 0 && WriteAll(CloudPath(n), data, (DWORD)size);
    HLog("cloud: gravar %s (%d bytes) -> %s", n.c_str(), size, ok ? "ok" : "FALHOU");
    return ok;
}

static int FileRead(void*, const char* file, void* data, int size) {
    std::string n = CloudName(file);
    std::vector<BYTE> buf;
    CloudLock lk;
    if (n.empty() || size <= 0 || !ReadRange(CloudPath(n), buf, 0, (DWORD)size)) return 0;
    memcpy(data, buf.data(), buf.size());
    return (int)buf.size();
}

static SteamAPICall FileWriteAsync(void* self, const char* file, const void* data, unsigned size) {
    struct { int result; } r = {FileWrite(self, file, data, (int)size) ? 1 : 2}; // k_EResultOK / k_EResultFail
    SteamAPICall h = NewCall();
    CompleteCall(h, &r, sizeof(r)); // RemoteStorageFileWriteAsyncComplete_t
    return h;
}

static std::map<SteamAPICall, std::vector<BYTE>> g_reads;

static SteamAPICall FileReadAsync(void*, const char* file, unsigned offset, unsigned size) {
    std::string n = CloudName(file);
    std::vector<BYTE> buf;
    bool ok;
    {
        CloudLock lk;
        ok = !n.empty() && ReadRange(CloudPath(n), buf, offset, size);
    }
    SteamAPICall h = NewCall();
#pragma pack(push, 8)
    struct { SteamAPICall h; int result; unsigned offset; unsigned read; } r = {h, ok ? 1 : 2, offset, (unsigned)buf.size()}; // RemoteStorageFileReadAsyncComplete_t
#pragma pack(pop)
    {
        CloudLock lk;
        g_reads[h] = std::move(buf);
    }
    CompleteCall(h, &r, sizeof(r));
    return h;
}

static bool FileReadAsyncComplete(void*, SteamAPICall h, void* out, unsigned size) {
    CloudLock lk;
    auto it = g_reads.find(h);
    if (it == g_reads.end()) return false;
    memcpy(out, it->second.data(), std::min<size_t>(size, it->second.size()));
    g_reads.erase(it);
    return true;
}

static bool FileForget(void*, const char*) { return true; }

static bool FileDelete(void*, const char* file) {
    std::string n = CloudName(file);
    CloudLock lk;
    bool ok = !n.empty() && DeleteFileW(CloudPath(n).c_str());
    HLog("cloud: apagar %s -> %s", n.c_str(), ok ? "ok" : "falhou");
    return ok;
}

static SteamAPICall FileShare(void*, const char*) { return 0; }
static bool SetSyncPlatforms(void*, const char*, int) { return true; }

static bool FileExists(void*, const char* file) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    CloudLock lk;
    return Info(CloudName(file), a);
}

static int RS_GetFileSize(void*, const char* file) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    CloudLock lk;
    return Info(CloudName(file), a) ? (int)a.nFileSizeLow : 0;
}

static long long GetFileTimestamp(void*, const char* file) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    CloudLock lk;
    if (!Info(CloudName(file), a)) return 0;
    ULARGE_INTEGER t;
    t.LowPart = a.ftLastWriteTime.dwLowDateTime;
    t.HighPart = a.ftLastWriteTime.dwHighDateTime;
    return (long long)(t.QuadPart / 10000000ull) - 11644473600ll; // FILETIME -> Unix
}

static int GetSyncPlatforms(void*, const char*) { return -1; } // todas

static std::vector<std::pair<std::string, int>> g_list;

static void ListDir(const std::wstring& dir, const std::string& prefix) {
    WIN32_FIND_DATAW d;
    HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &d, FindExSearchNameMatch, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring w = d.cFileName;
        if (w == L"." || w == L"..") continue;
        std::string n = prefix + std::string(w.begin(), w.end());
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ListDir(dir + L"\\" + w, n + "/");
        else if (n.size() < 4 || n.compare(n.size() - 4, 4, ".tmp") != 0) g_list.push_back({n, (int)d.nFileSizeLow});
    } while (FindNextFileW(h, &d));
    FindClose(h);
}

static int GetFileCount(void*) {
    CloudLock lk;
    g_list.clear();
    ListDir(CloudDir(), "");
    return (int)g_list.size();
}

static const char* GetFileNameAndSize(void*, int i, int* size) {
    CloudLock lk;
    if (i < 0 || i >= (int)g_list.size()) { if (size) *size = 0; return ""; }
    if (size) *size = g_list[i].second;
    return g_list[i].first.c_str();
}

static bool GetQuota(void*, unsigned long long* total, unsigned long long* avail) {
    const unsigned long long kTotal = 1ull << 30; // 1 GB, como uma cota generosa da Steam
    unsigned long long used = 0;
    {
        CloudLock lk;
        g_list.clear();
        ListDir(CloudDir(), "");
        for (auto& f : g_list) used += (unsigned)f.second;
    }
    if (total) *total = kTotal;
    if (avail) *avail = used < kTotal ? kTotal - used : 0;
    return true;
}

static bool RetTrue(void*) { return true; }
static void SetCloudEnabled(void*, bool) {}

struct CloudSlot { int slot; void* fn; };
extern const CloudSlot g_cloudSlots[] = {
    {0, (void*)&FileWrite}, {1, (void*)&FileRead}, {2, (void*)&FileWriteAsync}, {3, (void*)&FileReadAsync}, {4, (void*)&FileReadAsyncComplete},
    {5, (void*)&FileForget}, {6, (void*)&FileDelete}, {7, (void*)&FileShare}, {8, (void*)&SetSyncPlatforms},
    {13, (void*)&FileExists}, {14, (void*)&FileExists}, {15, (void*)&RS_GetFileSize}, {16, (void*)&GetFileTimestamp}, {17, (void*)&GetSyncPlatforms},
    {18, (void*)&GetFileCount}, {19, (void*)&GetFileNameAndSize}, {20, (void*)&GetQuota},
    {21, (void*)&RetTrue}, {22, (void*)&RetTrue}, {23, (void*)&SetCloudEnabled},
    {-1, nullptr},
};
