#include "pe_loader.h"
#include <algorithm>

// Em UWP, memória executável exige as variantes *FromApp (capability codeGeneration).
#ifdef HOST_UWP
#define PE_ALLOC(addr, size, prot) VirtualAllocFromApp((addr), (size), MEM_RESERVE | MEM_COMMIT, (prot))
#define PE_PROTECT(addr, size, prot, old) VirtualProtectFromApp((addr), (size), (prot), (old))
typedef ULONG PeOldProt;
#else
#define PE_ALLOC(addr, size, prot) VirtualAlloc((addr), (size), MEM_RESERVE | MEM_COMMIT, (prot))
#define PE_PROTECT(addr, size, prot, old) VirtualProtect((addr), (size), (prot), (old))
typedef DWORD PeOldProt;
#endif

static bool ReadWholeFile(const std::wstring& path, std::vector<BYTE>& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    data.resize((size_t)sz.QuadPart);
    size_t done = 0;
    while (done < data.size()) {
        DWORD got = 0;
        if (!ReadFile(h, data.data() + done, (DWORD)std::min<size_t>(data.size() - done, 1 << 24), &got, nullptr) || !got) break;
        done += got;
    }
    CloseHandle(h);
    return done == data.size();
}

template <class T> static T* Rva(BYTE* base, DWORD rva) { return reinterpret_cast<T*>(base + rva); }

static const IMAGE_DATA_DIRECTORY& Dir(IMAGE_NT_HEADERS64* nt, int i) { return nt->OptionalHeader.DataDirectory[i]; }

static bool ResolveThunks(BYTE* base, const char* dll, DWORD nameThunkRva, DWORD iatRva, const ImportResolver& resolve,
                          size_t& count, std::string& err) {
    auto* nameT = Rva<IMAGE_THUNK_DATA64>(base, nameThunkRva);
    auto* iat = Rva<IMAGE_THUNK_DATA64>(base, iatRva);
    for (; nameT->u1.AddressOfData; ++nameT, ++iat, ++count) {
        void* fn;
        if (IMAGE_SNAP_BY_ORDINAL64(nameT->u1.Ordinal)) {
            fn = resolve(dll, nullptr, (WORD)IMAGE_ORDINAL64(nameT->u1.Ordinal));
        } else {
            auto* ibn = Rva<IMAGE_IMPORT_BY_NAME>(base, (DWORD)nameT->u1.AddressOfData);
            fn = resolve(dll, (const char*)ibn->Name, 0);
        }
        if (!fn) { err = std::string("import sem resolução: ") + dll; return false; }
        iat->u1.Function = (ULONGLONG)fn;
    }
    return true;
}

bool MapImage(const std::wstring& path, const ImportResolver& resolve, LoadedModule& out, MapStats& stats, std::string& err) {
    std::vector<BYTE> file;
    if (!ReadWholeFile(path, file)) { err = "falha ao ler arquivo"; return false; }
    if (file.size() < sizeof(IMAGE_DOS_HEADER)) { err = "arquivo pequeno"; return false; }
    auto* dos = (IMAGE_DOS_HEADER*)file.data();
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { err = "MZ inválido"; return false; }
    auto* fnt = (IMAGE_NT_HEADERS64*)(file.data() + dos->e_lfanew);
    if (fnt->Signature != IMAGE_NT_SIGNATURE || fnt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) { err = "não é PE x64"; return false; }

    SIZE_T size = fnt->OptionalHeader.SizeOfImage;
    BYTE* base = (BYTE*)PE_ALLOC((void*)fnt->OptionalHeader.ImageBase, size, PAGE_READWRITE);
    if (!base) base = (BYTE*)PE_ALLOC(nullptr, size, PAGE_READWRITE);
    if (!base) { err = "VirtualAlloc falhou (" + std::to_string(GetLastError()) + ")"; return false; }

    memcpy(base, file.data(), fnt->OptionalHeader.SizeOfHeaders);
    auto* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++stats.sections) {
        DWORD raw = std::min<DWORD>(sec[i].SizeOfRawData, sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData);
        if (sec[i].PointerToRawData + (size_t)raw > file.size()) { err = "seção fora do arquivo"; return false; }
        memcpy(base + sec[i].VirtualAddress, file.data() + sec[i].PointerToRawData, raw);
    }

    // Relocações
    ptrdiff_t delta = (ptrdiff_t)(base - (BYTE*)nt->OptionalHeader.ImageBase);
    if (delta != 0) {
        auto& rd = Dir(nt, IMAGE_DIRECTORY_ENTRY_BASERELOC);
        if (!rd.Size) { err = "sem relocações e base preferida indisponível"; return false; }
        auto* blk = Rva<IMAGE_BASE_RELOCATION>(base, rd.VirtualAddress);
        auto* end = (BYTE*)blk + rd.Size;
        while ((BYTE*)blk < end && blk->SizeOfBlock) {
            DWORD n = (blk->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
            auto* e = (WORD*)(blk + 1);
            for (DWORD i = 0; i < n; ++i) {
                WORD type = e[i] >> 12, off = e[i] & 0xFFF;
                BYTE* p = base + blk->VirtualAddress + off;
                if (type == IMAGE_REL_BASED_DIR64) { *(ULONGLONG*)p += delta; ++stats.relocs; }
                else if (type == IMAGE_REL_BASED_HIGHLOW) { *(DWORD*)p += (DWORD)delta; ++stats.relocs; }
                else if (type != IMAGE_REL_BASED_ABSOLUTE) { err = "tipo de relocação não suportado"; return false; }
            }
            blk = (IMAGE_BASE_RELOCATION*)((BYTE*)blk + blk->SizeOfBlock);
        }
    }

    // Imports normais
    auto& id = Dir(nt, IMAGE_DIRECTORY_ENTRY_IMPORT);
    if (id.Size) {
        for (auto* d = Rva<IMAGE_IMPORT_DESCRIPTOR>(base, id.VirtualAddress); d->Name; ++d) {
            const char* dll = (const char*)(base + d->Name);
            DWORD t = d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk;
            if (!ResolveThunks(base, dll, t, d->FirstThunk, resolve, stats.imports, err)) return false;
        }
    }
    // Delay imports (resolvidos antecipadamente)
    auto& dd = Dir(nt, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
    if (dd.Size) {
        for (auto* d = Rva<IMAGE_DELAYLOAD_DESCRIPTOR>(base, dd.VirtualAddress); d->DllNameRVA; ++d) {
            const char* dll = (const char*)(base + d->DllNameRVA);
            if (!ResolveThunks(base, dll, d->ImportNameTableRVA, d->ImportAddressTableRVA, resolve, stats.delayImports, err)) return false;
        }
    }

    // Proteções por seção
    PeOldProt old;
    PE_PROTECT(base, nt->OptionalHeader.SizeOfHeaders, PAGE_READONLY, &old);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        DWORD c = sec[i].Characteristics, prot;
        bool x = c & IMAGE_SCN_MEM_EXECUTE, w = c & IMAGE_SCN_MEM_WRITE, r = c & IMAGE_SCN_MEM_READ;
        prot = x ? (w ? PAGE_EXECUTE_READWRITE : (r ? PAGE_EXECUTE_READ : PAGE_EXECUTE)) : (w ? PAGE_READWRITE : PAGE_READONLY);
        SIZE_T len = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
        if (len && !PE_PROTECT(base + sec[i].VirtualAddress, len, prot, &old)) { err = "falha ao proteger secao (" + std::to_string(GetLastError()) + ")"; return false; }
    }

    // Tabela de exceções x64 (SEH/unwind)
    auto& ed = Dir(nt, IMAGE_DIRECTORY_ENTRY_EXCEPTION);
    if (ed.Size) RtlAddFunctionTable(Rva<RUNTIME_FUNCTION>(base, ed.VirtualAddress), ed.Size / sizeof(RUNTIME_FUNCTION), (DWORD64)base);
    FlushInstructionCache(GetCurrentProcess(), base, size);

    out.path = path;
    out.base = base;
    out.size = size;
    out.nt = nt;
    out.hasTls = Dir(nt, IMAGE_DIRECTORY_ENTRY_TLS).Size != 0;
    out.entry = nt->OptionalHeader.AddressOfEntryPoint ? base + nt->OptionalHeader.AddressOfEntryPoint : nullptr;
    return true;
}

void* FindExport(const LoadedModule& m, const char* name) {
    auto& xd = Dir(m.nt, IMAGE_DIRECTORY_ENTRY_EXPORT);
    if (!xd.Size) return nullptr;
    auto* ex = Rva<IMAGE_EXPORT_DIRECTORY>(m.base, xd.VirtualAddress);
    auto* names = Rva<DWORD>(m.base, ex->AddressOfNames);
    auto* ords = Rva<WORD>(m.base, ex->AddressOfNameOrdinals);
    auto* funcs = Rva<DWORD>(m.base, ex->AddressOfFunctions);
    for (DWORD i = 0; i < ex->NumberOfNames; ++i)
        if (!strcmp((const char*)(m.base + names[i]), name)) return m.base + funcs[ords[i]];
    return nullptr;
}
