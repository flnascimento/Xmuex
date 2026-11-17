// Loader manual de PE x64 (mapeia, relocaliza, resolve imports). Sem dependências além de kernel32.
#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

struct LoadedModule {
    std::wstring path;
    std::string name;        // nome minúsculo, ex.: "shenmue.exe"
    BYTE* base = nullptr;
    size_t size = 0;
    IMAGE_NT_HEADERS64* nt = nullptr;
    bool hasTls = false;
    void* entry = nullptr;   // AddressOfEntryPoint absoluto
};

// Devolve o endereço da função ou nullptr. Para import por ordinal, func == nullptr.
using ImportResolver = std::function<void*(const std::string& dll, const char* func, WORD ordinal)>;

struct MapStats { size_t relocs = 0, imports = 0, delayImports = 0, sections = 0; };

bool MapImage(const std::wstring& path, const ImportResolver& resolve, LoadedModule& out, MapStats& stats, std::string& err);
void* FindExport(const LoadedModule& m, const char* name);
