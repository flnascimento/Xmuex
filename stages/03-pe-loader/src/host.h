// Runtime nativo do port: carrega o jogo (PE) dentro do app UWP e fornece substitutos (shims) para o que não existe em UWP.
#pragma once
#include <windows.h>
#include <string>
#include "pe_loader.h"

void HLog(const char* fmt, ...);

extern std::wstring g_localW;    // LocalState
extern std::wstring g_gameRootW; // LocalState\game
extern std::wstring g_gameDirW;  // LocalState\game\sm1 ou sm2
extern int g_gameId;             // 1 ou 2
extern LoadedModule g_exe;
extern IUnknown* g_coreWindowUnk; // CoreWindow do app

bool HostFlagFile(const wchar_t* name); // true se LocalState\<name> existe (opções de depuração)

// Tabela de shims: dll em minúsculas; name = nome da função ou "#ordinal".
struct ShimEntry { const char* dll; const char* name; void* fn; };
extern const ShimEntry g_kernelShims[];
extern const ShimEntry g_userShims[];
extern const ShimEntry g_steamShims[];
extern const ShimEntry g_dxShims[];
extern const ShimEntry g_audioShims[];
extern const ShimEntry g_inputShims[];
extern const ShimEntry g_miscShims[];
extern const ShimEntry g_netShims[];
extern const ShimEntry g_modShims[];

void ModsApply(LoadedModule& exe, int game); // mods.cpp: widescreen e tradução embutidos

// Módulos mapeados manualmente
LoadedModule* HostModuleByName(const std::string& lowerName);
LoadedModule* HostModuleByAddr(const void* addr);
LoadedModule* HostEnsureModule(const std::string& lowerName); // mapeia se o arquivo existir na pasta do jogo
HMODULE HostSysModule(const std::string& lowerName);          // módulo real do sistema/pacote
void* HostSysProc(const char* dll, const char* name);
std::string HostLower(std::string s);

// Thunks gerados: "mov ecx,id; jmp handler". rcx é perdido; demais registradores preservados.
void* HostMakeThunk(void* handler, unsigned id);
void HostSealThunks();

void SteamShimInit();
void HostQueueKey(unsigned vk, bool down);
