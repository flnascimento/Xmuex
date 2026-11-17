// steam_api64: no console não há cliente Steam. A posse do jogo é verificada pelo launcher no download (chave do depot da
// conta do usuário); aqui só respondemos à API para o jogo seguir. Slots não tratados registram no log e devolvem 0.
// A Steam Cloud (saves e opções) fica em shim_steamcloud.cpp.
#include "host.h"
#include <map>
#include <set>
#include <vector>

static const unsigned kAppId = 758330;
static const unsigned long long kSteamId = 76561197960287930ull;
static const int kSlots = 96;

struct Iface { void** vtable; }; // objeto COM-like: primeiro campo é a vtable

// shim_steamcloud.cpp: Steam Cloud em arquivos locais e call results
struct CloudSlot { int slot; void* fn; };
extern const CloudSlot g_cloudSlots[];
void SteamRegisterCallResult(void* cb, unsigned long long h);
void SteamUnregisterCallResult(void* cb, unsigned long long h);
void SteamDispatchCallResults();

static std::vector<std::string> g_ifaceNames;
static std::set<unsigned> g_slotSeen;
static SRWLOCK g_lock = SRWLOCK_INIT;

static UINT_PTR SlotHandler(unsigned id) {
    AcquireSRWLockExclusive(&g_lock);
    bool first = g_slotSeen.insert(id).second;
    std::string n = (id >> 8) < g_ifaceNames.size() ? g_ifaceNames[id >> 8] : "?";
    ReleaseSRWLockExclusive(&g_lock);
    if (first) HLog("Steam: %s slot %u (stub -> 0)", n.c_str(), id & 0xFF);
    return 0;
}

// Slots conhecidos, por família de interface. Assinaturas: this em rcx.
static UINT_PTR RetTrue(void*) { return 1; }
static const char* StrEnglish(void*) { return "english"; }
static const char* StrLangs(void*) { return "english"; }
static const char* StrPlayer(void*) { return "Player"; }
static const char* StrUS(void*) { return "US"; }
static UINT_PTR AppId(void*) { return kAppId; }
static UINT_PTR Universe(void*) { return 1; }
static void* GetSteamID(void*, unsigned long long* ret) { *ret = kSteamId; return ret; } // CSteamID volta por ponteiro oculto

struct Special { const char* prefix; int slot; void* fn; };
static const Special kSpecials[] = {
    {"SteamUser", 1, (void*)&RetTrue},             // BLoggedOn
    {"SteamUser", 2, (void*)&GetSteamID},
    {"SteamUtils", 2, (void*)&Universe},           // GetConnectedUniverse
    {"SteamUtils", 4, (void*)&StrUS},              // GetIPCountry
    {"SteamUtils", 9, (void*)&AppId},
    {"STEAMAPPS", 0, (void*)&RetTrue},             // BIsSubscribed
    {"STEAMAPPS", 4, (void*)&StrEnglish},          // GetCurrentGameLanguage
    {"STEAMAPPS", 5, (void*)&StrLangs},            // GetAvailableGameLanguages
    {"STEAMAPPS", 6, (void*)&RetTrue},             // BIsSubscribedApp
    {"STEAMAPPS", 7, (void*)&RetTrue},             // BIsDlcInstalled
    {"SteamFriends", 0, (void*)&StrPlayer},        // GetPersonaName
    {"STEAMUSERSTATS", 0, (void*)&RetTrue},        // RequestCurrentStats
};

static std::map<std::string, Iface*> g_ifaces;

static Iface* GetIface(const char* ver) {
    std::string name = ver ? ver : "?";
    AcquireSRWLockExclusive(&g_lock);
    auto it = g_ifaces.find(name);
    Iface* r = it != g_ifaces.end() ? it->second : nullptr;
    ReleaseSRWLockExclusive(&g_lock);
    return r;
}

static Iface* NewIface(const std::string& name) {
    unsigned idx = (unsigned)g_ifaceNames.size();
    g_ifaceNames.push_back(name);
    void** vt = new void*[kSlots];
    for (int i = 0; i < kSlots; ++i) vt[i] = HostMakeThunk((void*)&SlotHandler, (idx << 8) | (unsigned)i);
    for (auto& s : kSpecials)
        if (name.rfind(s.prefix, 0) == 0) vt[s.slot] = s.fn;
    if (name.rfind("STEAMREMOTESTORAGE", 0) == 0)
        for (auto* c = g_cloudSlots; c->slot >= 0; ++c) vt[c->slot] = c->fn;
    auto* o = new Iface{vt};
    g_ifaces[name] = o;
    return o;
}

// Versões usadas pelo jogo; criadas antes de selar os thunks.
static const char* const kVersions[] = {
    "SteamUser019", "SteamUtils009", "SteamFriends015", "SteamMatchMakingServers002", "SteamMatchMaking009", "STEAMAPPS_INTERFACE_VERSION008",
    "STEAMUSERSTATS_INTERFACE_VERSION011", "STEAMREMOTESTORAGE_INTERFACE_VERSION014", "SteamNetworking005", "STEAMHTTP_INTERFACE_VERSION002",
    "STEAMSCREENSHOTS_INTERFACE_VERSION003", "STEAMUGC_INTERFACE_VERSION010", "SteamController006", "STEAMMUSIC_INTERFACE_VERSION001",
    "STEAMAPPLIST_INTERFACE_VERSION001", "STEAMHTMLSURFACE_INTERFACE_VERSION_004", "STEAMMUSICREMOTE_INTERFACE_VERSION001",
    "STEAMVIDEO_INTERFACE_V002", "STEAMINVENTORY_INTERFACE_V002", "STEAMPARENTALSETTINGS_INTERFACE_VERSION001", "?",
};

// ISteamClient: GetISteam*(this, hUser, hPipe, ver). GetISteamUtils é (this, hPipe, ver).
static void* Client_Get3(void*, int, int, const char* ver) {
    Iface* i = GetIface(ver);
    if (!i) { HLog("Steam: interface desconhecida %s", ver ? ver : "(null)"); i = GetIface("?"); }
    return i;
}
static void* Client_GetUtils(void*, int, const char* ver) {
    Iface* i = GetIface(ver);
    return i ? i : GetIface("?");
}
static UINT_PTR Client_One(void*) { return 1; }

static Iface g_client;
static void* g_clientVt[64];

void SteamShimInit() {
    for (auto* v : kVersions) NewIface(v);
    unsigned idx = (unsigned)g_ifaceNames.size();
    g_ifaceNames.push_back("SteamClient017");
    for (int i = 0; i < 64; ++i) g_clientVt[i] = HostMakeThunk((void*)&SlotHandler, (idx << 8) | (unsigned)i);
    g_clientVt[0] = (void*)&Client_One; // CreateSteamPipe
    g_clientVt[1] = (void*)&Client_One; // BReleaseSteamPipe
    g_clientVt[2] = (void*)&Client_One; // ConnectToGlobalUser
    for (int s : {5, 6, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36}) g_clientVt[s] = (void*)&Client_Get3;
    g_clientVt[9] = (void*)&Client_GetUtils;
    g_client.vtable = g_clientVt;
}

// ---- exports flat
static bool __cdecl S_RestartAppIfNecessary(unsigned) { return false; }
static bool __cdecl S_Init() { HLog("SteamAPI_Init (stub)"); return true; }
static void __cdecl S_Shutdown() {}
static void __cdecl S_RunCallbacks() { SteamDispatchCallResults(); }
static void __cdecl S_RegisterCallback(void*, int id) { HLog("SteamAPI_RegisterCallback(%d)", id); }
static void __cdecl S_UnregisterCallback(void*) {}
static void __cdecl S_RegisterCallResult(void* cb, unsigned long long h) { SteamRegisterCallResult(cb, h); }
static void __cdecl S_UnregisterCallResult(void* cb, unsigned long long h) { SteamUnregisterCallResult(cb, h); }
static int __cdecl S_GetHSteamUser() { return 1; }
static int __cdecl S_GetHSteamPipe() { return 1; }

static void* __cdecl S_CreateInterface(const char* ver) {
    HLog("SteamInternal_CreateInterface(%s)", ver ? ver : "(null)");
    if (ver && strncmp(ver, "SteamClient", 11) == 0) return &g_client;
    Iface* i = GetIface(ver);
    return i ? i : (void*)&g_client;
}

// struct { void (*init)(void* ctx); uintptr counter; CSteamAPIContext ctx; }
struct ContextInitData { void (*init)(void*); UINT_PTR counter; BYTE ctx[1]; };
static void* __cdecl S_ContextInit(void* p) {
    auto* d = (ContextInitData*)p;
    if (d->counter != 1) {
        d->init(d->ctx);
        d->counter = 1;
    }
    return d->ctx;
}

#define ST(n, f) {"steam_api64.dll", #n, (void*)&f}
const ShimEntry g_steamShims[] = {
    ST(SteamAPI_RestartAppIfNecessary, S_RestartAppIfNecessary), ST(SteamAPI_Init, S_Init), ST(SteamAPI_Shutdown, S_Shutdown),
    ST(SteamAPI_RunCallbacks, S_RunCallbacks), ST(SteamAPI_RegisterCallback, S_RegisterCallback), ST(SteamAPI_UnregisterCallback, S_UnregisterCallback),
    ST(SteamAPI_RegisterCallResult, S_RegisterCallResult), ST(SteamAPI_UnregisterCallResult, S_UnregisterCallResult),
    ST(SteamAPI_GetHSteamUser, S_GetHSteamUser), ST(SteamAPI_GetHSteamPipe, S_GetHSteamPipe),
    ST(SteamInternal_CreateInterface, S_CreateInterface), ST(SteamInternal_ContextInit, S_ContextInit),
    {nullptr, nullptr, nullptr},
};
