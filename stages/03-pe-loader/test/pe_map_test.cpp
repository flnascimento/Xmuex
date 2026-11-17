// Teste no PC: mapeia Shenmue.exe/Shenmue2.exe sem executar e relata imports.
// Uso: pe_map_test.exe <pasta_do_jogo> <sm1|sm2>
#include "../src/pe_loader.h"
#include <cstdio>
#include <map>
#include <set>

static std::wstring W(const std::string& s) { return std::wstring(s.begin(), s.end()); }
static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

static std::wstring g_dir;
static std::map<std::string, LoadedModule> g_mods;
static std::set<std::string> g_unresolved;
static void Dummy() {}

static void* Resolve(const std::string& dllIn, const char* func, WORD ord);

static void* ResolveOwn(const std::string& dll, const char* func, WORD ord, bool& handled) {
    std::wstring p = g_dir + L"\\" + W(dll);
    if (GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) { handled = false; return nullptr; }
    handled = true;
    auto it = g_mods.find(dll);
    if (it == g_mods.end()) {
        LoadedModule m; MapStats st; std::string err;
        if (!MapImage(p, Resolve, m, st, err)) { printf("  ! falha ao mapear %s: %s\n", dll.c_str(), err.c_str()); return nullptr; }
        printf("  + %s mapeada @%p (relocs %zu, imports %zu)\n", dll.c_str(), m.base, st.relocs, st.imports);
        it = g_mods.emplace(dll, m).first;
    }
    return func ? FindExport(it->second, func) : nullptr;
}

static void* Resolve(const std::string& dllIn, const char* func, WORD ord) {
    std::string dll = Lower(dllIn);
    bool handled;
    void* f = ResolveOwn(dll, func, ord, handled);
    if (handled) { if (!f) g_unresolved.insert(dll + "!" + (func ? func : "#" + std::to_string(ord))); return f ? f : (void*)&Dummy; }
    HMODULE h = LoadLibraryW(W(dll).c_str());
    f = h ? GetProcAddress(h, func ? func : MAKEINTRESOURCEA(ord)) : nullptr;
    if (!f) { g_unresolved.insert(dll + "!" + (func ? func : "#" + std::to_string(ord))); return (void*)&Dummy; }
    return f;
}

int main(int argc, char** argv) {
    if (argc < 3) { puts("uso: pe_map_test <pasta_do_jogo> <sm1|sm2>"); return 2; }
    std::string sub = argv[2];
    g_dir = W(std::string(argv[1])) + L"\\" + W(sub);
    SetDllDirectoryW(g_dir.c_str());
    std::wstring exe = g_dir + L"\\" + (sub == "sm1" ? L"Shenmue.exe" : L"Shenmue2.exe");
    LoadedModule m; MapStats st; std::string err;
    if (!MapImage(exe, Resolve, m, st, err)) { printf("FALHA: %s\n", err.c_str()); return 1; }
    printf("EXE mapeado @%p (base preferida %s)\n", m.base, (ULONGLONG)m.base == m.nt->OptionalHeader.ImageBase ? "OK" : "não");
    printf("  seções %zu · relocs %zu · imports %zu · delay %zu · TLS %s · entry RVA 0x%llx\n", st.sections, st.relocs, st.imports,
           st.delayImports, m.hasTls ? "sim" : "não", (unsigned long long)((BYTE*)m.entry - m.base));
    printf("Imports não resolvidos: %zu\n", g_unresolved.size());
    for (auto& s : g_unresolved) printf("  - %s\n", s.c_str());
    return 0;
}
