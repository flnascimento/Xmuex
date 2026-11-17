// Mods embutidos no pacote (<pacote>\mods, gerado por tools/import_mods.py):
// - patches de bytes no exe já mapeado (widescreen), sem tocar no arquivo em disco;
// - sobreposição de arquivos: archives\dx11\mods do jogo passa a apontar para uma ou mais pastas do
//   pacote (<pacote>\mods\overlay\smN\... e \overlay_tex\smN\...: tradução e texturas, cada uma com
//   seu próprio toggle), sem copiar nada para a pasta do jogo;
// - instalação de arquivos de dados (texturas em archives\dx11\data): o jogo descobre os arquivos
//   .tac/.tad desse diretório sozinho (é assim que o pacote original tem vários), então eles
//   precisam existir de verdade ali. Copiados/apagados pelo nome (nunca toca nos arquivos da Steam).
// Opções em LocalState\mods.ini (escrito pelo launcher).
#include "host.h"
#include <cstdio>
#include <sys/stat.h>
#include <vector>

static std::vector<std::string> g_overlays; // vazio = sobreposição desligada; ordem = prioridade

static std::string Narrow(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::string ReadFileText(const std::wstring& path) {
    std::string out;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return out;
    char buf[4096];
    DWORD got;
    while (ReadFile(f, buf, sizeof(buf), &got, nullptr) && got) out.append(buf, got);
    CloseHandle(f);
    return out;
}

static int Option(const std::string& ini, const char* key, int def) {
    std::string k = std::string(key) + "=";
    size_t p = 0;
    while ((p = ini.find(k, p)) != std::string::npos) {
        if (p == 0 || ini[p - 1] == '\n') return ini[p + k.size()] == '1' ? 1 : 0;
        p += k.size();
    }
    return def;
}

static std::vector<BYTE> Hex(const std::string& s) {
    std::vector<BYTE> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2) v.push_back((BYTE)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    return v;
}

// Cópia com log de progresso a cada 256 MB (os .tac de texturas têm alguns GB cada).
static bool CopyBig(const std::wstring& src, const std::wstring& dst) {
    HANDLE in = CreateFileW(src.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (in == INVALID_HANDLE_VALUE) return false;
    HANDLE out = CreateFileW(dst.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) { CloseHandle(in); return false; }
    std::vector<BYTE> buf(4u << 20);
    unsigned long long total = 0, nextLog = 256ull << 20;
    bool ok = true;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(in, buf.data(), (DWORD)buf.size(), &got, nullptr)) { ok = false; break; }
        if (!got) break;
        DWORD wrote = 0;
        if (!WriteFile(out, buf.data(), got, &wrote, nullptr) || wrote != got) { ok = false; break; }
        total += got;
        if (total >= nextLog) { HLog("mods: ...%llu MB copiados", total >> 20); nextLog += (256ull << 20); }
    }
    CloseHandle(in);
    CloseHandle(out);
    if (!ok) DeleteFileW(dst.c_str());
    return ok;
}

static bool SameSize(const std::wstring& a, const std::wstring& b) {
    WIN32_FILE_ATTRIBUTE_DATA fa{}, fb{};
    if (!GetFileAttributesExW(a.c_str(), GetFileExInfoStandard, &fa)) return false;
    if (!GetFileAttributesExW(b.c_str(), GetFileExInfoStandard, &fb)) return false;
    return fa.nFileSizeHigh == fb.nFileSizeHigh && fa.nFileSizeLow == fb.nFileSizeLow;
}

// Nomes únicos do mod de texturas (Streamer2k Textures - Better Textures for Shenmue v4.1), para
// nunca mexer em nenhum outro arquivo de archives\dx11\data (os da Steam).
static const wchar_t* const kTexDataFiles[] = {
    L"Streamer2K_Better_Textures_41 part1.tac", L"Streamer2K_Better_Textures_41 part1.tad",
    L"Streamer2K_Better_Textures_41 part2.tac", L"Streamer2K_Better_Textures_41 part2.tad",
};

static void InstallTextureData(const std::wstring& pkgDataDir, const std::wstring& gameDataDir, bool enabled) {
    for (const wchar_t* name : kTexDataFiles) {
        std::wstring src = pkgDataDir + L"\\" + name, dst = gameDataDir + L"\\" + name;
        if (enabled) {
            if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) continue; // pack não embutido no pacote
            if (SameSize(src, dst)) continue;
            HLog("mods: copiando %ls para archives\\dx11\\data...", name);
            if (!CopyBig(src, dst)) HLog("mods: falha ao copiar %ls", name);
        } else if (GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileW(dst.c_str());
            HLog("mods: removido %ls de archives\\dx11\\data", name);
        }
    }
}

// Aplica um patch se o original aparecer exatamente uma vez nas seções executáveis.
static bool PatchOnce(LoadedModule& m, const std::vector<BYTE>& orig, const std::vector<BYTE>& rep) {
    BYTE* found = nullptr;
    int count = 0;
    auto* sec = IMAGE_FIRST_SECTION(m.nt);
    for (WORD i = 0; i < m.nt->FileHeader.NumberOfSections; ++i) {
        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        BYTE* p = m.base + sec[i].VirtualAddress;
        size_t len = sec[i].Misc.VirtualSize;
        for (size_t j = 0; j + orig.size() <= len; ++j)
            if (p[j] == orig[0] && memcmp(p + j, orig.data(), orig.size()) == 0) { found = p + j; ++count; }
    }
    if (count != 1) return false;
    ULONG old;
    if (!VirtualProtectFromApp(found, rep.size(), PAGE_READWRITE, &old)) return false;
    memcpy(found, rep.data(), rep.size());
    VirtualProtectFromApp(found, rep.size(), old, &old);
    FlushInstructionCache(GetCurrentProcess(), found, rep.size());
    return true;
}

void ModsApply(LoadedModule& exe, int game) {
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir(self);
    dir = dir.substr(0, dir.find_last_of(L'\\')) + L"\\mods";
    std::string ini = ReadFileText(g_localW + L"\\mods.ini");
    std::string sm = game == 2 ? "sm2" : "sm1";

    // Widescreen
    std::string patches = ReadFileText(dir + L"\\patches\\widescreen_" + std::wstring(sm.begin(), sm.end()) + L".txt");
    bool ws = Option(ini, ("widescreen_" + sm).c_str(), 1) != 0;
    bool fvi = game == 2 && Option(ini, "sm2_sem_bordas", 0);
    bool cam = game == 2 && Option(ini, "sm2_sem_icone_camera", 0);
    if (patches.empty()) HLog("mods: sem patches de widescreen no pacote");
    int applied = 0, failed = 0;
    size_t pos = 0;
    while (pos < patches.size()) {
        size_t nl = patches.find('\n', pos);
        std::string line = patches.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? patches.size() : nl + 1;
        if (line.empty() || line[0] == '#') continue;
        char tag[32] = {}, a[256] = {}, b[256] = {};
        if (sscanf_s(line.c_str(), "%31s %255s %255s", tag, (unsigned)sizeof(tag), a, (unsigned)sizeof(a), b, (unsigned)sizeof(b)) != 3) continue;
        std::string t = tag;
        bool want = (t == "main" && ws) || ((t == "fvi" || t == "vsp") && ws && fvi) || (t == "nosnapcamico" && cam);
        if (!want) continue;
        if (PatchOnce(exe, Hex(a), Hex(b))) ++applied;
        else { ++failed; HLog("mods: patch %s nao encontrado (versao diferente?)", tag); }
    }
    HLog("mods: widescreen %s (%d patches aplicados, %d falharam; sem bordas=%d, sem camera=%d)", ws ? "ligado" : "desligado", applied, failed, fvi, cam);

    // Texturas em alta definição (Streamer2k Textures - Better Textures for Shenmue v4.1; só Shenmue I).
    // archives\dx11\data: arquivos reais (o jogo enumera esse diretório sozinho).
    bool tex = game == 1 && Option(ini, "texturas_sm1", 0) != 0;
    if (game == 1) {
        InstallTextureData(dir + L"\\data\\sm1\\archives\\dx11\\data", g_gameDirW + L"\\archives\\dx11\\data", tex);
    }

    // Tradução e texturas (archives\dx11\mods do jogo): cada mod tem sua própria pasta no pacote;
    // o redirecionamento tenta cada uma, na ordem, e usa a primeira que realmente tiver o arquivo.
    std::wstring smW(sm.begin(), sm.end());
    g_overlays.clear();
    bool ptbr = Option(ini, ("legendas_ptbr_" + sm).c_str(), 1) != 0;
    std::wstring ptbrOverlay = dir + L"\\overlay\\" + smW + L"\\archives\\dx11\\mods";
    if (ptbr && GetFileAttributesW(ptbrOverlay.c_str()) != INVALID_FILE_ATTRIBUTES) g_overlays.push_back(Narrow(ptbrOverlay));
    std::wstring texOverlay = dir + L"\\overlay_tex\\" + smW + L"\\archives\\dx11\\mods";
    if (tex && GetFileAttributesW(texOverlay.c_str()) != INVALID_FILE_ATTRIBUTES) g_overlays.push_back(Narrow(texOverlay));
    if (!g_overlays.empty()) {
        // A pasta real precisa existir para o jogo achá-la ao listar archives\dx11.
        CreateDirectoryW((g_gameDirW + L"\\archives\\dx11\\mods").c_str(), nullptr);
    }
    HLog("mods: legendas PT-BR %s, texturas %s (sobreposicao com %zu fonte(s))",
         ptbr ? "ligadas" : "desligadas", tex ? "ligadas" : "desligadas", g_overlays.size());
}

// Barras normais viram invertidas e barras repetidas viram uma só ("a//b" -> "a\b").
static std::string Normalize(const char* in) {
    std::string p;
    for (const char* c = in; *c; ++c) {
        char ch = *c == '/' ? '\\' : *c;
        if (ch == '\\' && !p.empty() && p.back() == '\\') continue;
        p += ch;
    }
    return p;
}

// LocalState\filelog.txt: registra os caminhos que o jogo abre (para achar arquivos de configuração).
static void FileLog(const char* api, const char* path) {
    static const bool on = HostFlagFile(L"filelog.txt");
    static volatile LONG n = 0;
    if (on && path && InterlockedIncrement(&n) <= 400) HLog("arquivo %s: %s", api, path);
}

typedef DWORD(WINAPI* GetFileAttributesA_t)(LPCSTR);

// Troca o trecho ...archives\dx11\mods pelo caminho da primeira sobreposição (g_overlays) que
// realmente tiver o arquivo; sem nenhuma, mantém o comportamento antigo (usa a primeira mesmo assim,
// deixando o jogo tratar como "não existe").
static bool Redirect(const char* in, std::string& out) {
    if (g_overlays.empty() || !in) return false;
    std::string p = Normalize(in);
    std::string low = HostLower(p);
    static const char key[] = "archives\\dx11\\mods";
    size_t k = low.find(key);
    if (k == std::string::npos) return false;
    size_t end = k + sizeof(key) - 1;
    if (end < low.size() && low[end] != '\\') return false;
    std::string suffix = p.substr(end);
    static auto realAttr = (GetFileAttributesA_t)HostSysProc("kernelbase.dll", "GetFileAttributesA");
    out = g_overlays[0] + suffix;
    for (const auto& root : g_overlays) {
        std::string candidate = root + suffix;
        if (realAttr(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) { out = candidate; break; }
    }
    static volatile LONG g_redirLogs = 0;
    if (InterlockedIncrement(&g_redirLogs) <= 12) HLog("mods: %s -> %s", in, out.c_str());
    return true;
}

typedef HANDLE(WINAPI* CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE(WINAPI* FindFirstFileA_t)(LPCSTR, LPWIN32_FIND_DATAA);
typedef BOOL(WINAPI* GetFileAttributesExA_t)(LPCSTR, GET_FILEEX_INFO_LEVELS, LPVOID);

static HANDLE WINAPI S_CreateFileA(LPCSTR n, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl) {
    static auto real = (CreateFileA_t)HostSysProc("kernelbase.dll", "CreateFileA");
    FileLog(disp == OPEN_EXISTING ? "CreateFileA(ler)" : "CreateFileA(escrever)", n);
    std::string r;
    return real(Redirect(n, r) ? r.c_str() : n, acc, share, sa, disp, flags, tmpl);
}
// Listagem de archives\dx11: se a pasta "mods" não existir de verdade, ela é acrescentada ao fim da listagem.
typedef BOOL(WINAPI* FindNextFileA_t)(HANDLE, LPWIN32_FIND_DATAA);
static SRWLOCK g_findLock = SRWLOCK_INIT;
static std::vector<std::pair<HANDLE, bool>> g_injectFind; // handle -> já viu "mods"

static bool IsArchivesDx11List(const char* n) {
    std::string low = HostLower(Normalize(n));
    static const char tail[] = "archives\\dx11\\*";
    return low.size() >= sizeof(tail) - 1 && low.compare(low.size() - (sizeof(tail) - 1), sizeof(tail) - 1, tail) == 0;
}

static HANDLE WINAPI S_FindFirstFileA(LPCSTR n, LPWIN32_FIND_DATAA d) {
    static auto real = (FindFirstFileA_t)HostSysProc("kernelbase.dll", "FindFirstFileA");
    FileLog("FindFirstFileA", n);
    std::string r;
    HANDLE h = real(Redirect(n, r) ? r.c_str() : n, d);
    if (h != INVALID_HANDLE_VALUE && !g_overlays.empty() && IsArchivesDx11List(n)) {
        AcquireSRWLockExclusive(&g_findLock);
        g_injectFind.push_back({h, _stricmp(d->cFileName, "mods") == 0});
        ReleaseSRWLockExclusive(&g_findLock);
    }
    return h;
}

static BOOL WINAPI S_FindNextFileA(HANDLE h, LPWIN32_FIND_DATAA d) {
    static auto real = (FindNextFileA_t)HostSysProc("kernelbase.dll", "FindNextFileA");
    BOOL ok = real(h, d);
    AcquireSRWLockExclusive(&g_findLock);
    for (auto it = g_injectFind.begin(); it != g_injectFind.end(); ++it) {
        if (it->first != h) continue;
        if (ok && _stricmp(d->cFileName, "mods") == 0) it->second = true;
        if (!ok && !it->second) {
            it->second = true;
            memset(d, 0, sizeof(*d));
            d->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            strcpy_s(d->cFileName, "mods");
            ok = TRUE;
            HLog("mods: pasta virtual archives\\dx11\\mods listada");
        }
        break;
    }
    ReleaseSRWLockExclusive(&g_findLock);
    return ok;
}

static BOOL WINAPI S_FindClose(HANDLE h) {
    AcquireSRWLockExclusive(&g_findLock);
    for (auto it = g_injectFind.begin(); it != g_injectFind.end(); ++it)
        if (it->first == h) { g_injectFind.erase(it); break; }
    ReleaseSRWLockExclusive(&g_findLock);
    return FindClose(h);
}
static DWORD WINAPI S_GetFileAttributesA(LPCSTR n) {
    static auto real = (GetFileAttributesA_t)HostSysProc("kernelbase.dll", "GetFileAttributesA");
    std::string r;
    return real(Redirect(n, r) ? r.c_str() : n);
}
static BOOL WINAPI S_GetFileAttributesExA(LPCSTR n, GET_FILEEX_INFO_LEVELS l, LPVOID d) {
    static auto real = (GetFileAttributesExA_t)HostSysProc("kernelbase.dll", "GetFileAttributesExA");
    std::string r;
    return real(Redirect(n, r) ? r.c_str() : n, l, d);
}
static FILE* __cdecl S_fsopen(const char* n, const char* m, int sh) { FileLog(m, n); std::string r; return _fsopen(Redirect(n, r) ? r.c_str() : n, m, sh); }
static FILE* __cdecl S_fopen(const char* n, const char* m) { FileLog(m, n); std::string r; return fopen(Redirect(n, r) ? r.c_str() : n, m); }
static errno_t __cdecl S_fopen_s(FILE** f, const char* n, const char* m) { FileLog(m, n); std::string r; return fopen_s(f, Redirect(n, r) ? r.c_str() : n, m); }
static int __cdecl S_stat64(const char* n, struct _stat64* s) { std::string r; return _stat64(Redirect(n, r) ? r.c_str() : n, s); }

const ShimEntry g_modShims[] = {
    {"kernel32.dll", "CreateFileA", (void*)&S_CreateFileA},
    {"kernel32.dll", "FindFirstFileA", (void*)&S_FindFirstFileA},
    {"kernel32.dll", "FindNextFileA", (void*)&S_FindNextFileA},
    {"kernel32.dll", "FindClose", (void*)&S_FindClose},
    {"kernel32.dll", "GetFileAttributesA", (void*)&S_GetFileAttributesA},
    {"kernel32.dll", "GetFileAttributesExA", (void*)&S_GetFileAttributesExA},
    {"api-ms-win-crt-stdio-l1-1-0.dll", "_fsopen", (void*)&S_fsopen},
    {"api-ms-win-crt-stdio-l1-1-0.dll", "fopen", (void*)&S_fopen},
    {"api-ms-win-crt-stdio-l1-1-0.dll", "fopen_s", (void*)&S_fopen_s},
    {"api-ms-win-crt-filesystem-l1-1-0.dll", "_stat64", (void*)&S_stat64},
    {nullptr, nullptr, nullptr},
};
