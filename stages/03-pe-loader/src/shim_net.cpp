// ws2_32 sem rede. O único uso no jogo é a telemetria (libcurl embutido), dispensável; assim o jogo não depende de rede nem de
// ws2_32.dll no console. Toda operação falha com "rede indisponível", e o libcurl trata isso normalmente.
#include "host.h"

static volatile LONG g_err = 10050; // WSAENETDOWN
static const INT_PTR kSocketError = -1;

static int WINAPI S_WSAStartup(WORD ver, void* data) {
    memset(data, 0, 408); // sizeof(WSADATA) em x64
    ((WORD*)data)[0] = ver;
    ((WORD*)data)[1] = 0x0202;
    return 0;
}
static int WINAPI S_WSACleanup() { return 0; }
static int WINAPI S_WSAGetLastError() { return (int)g_err; }
static void WINAPI S_WSASetLastError(int e) { InterlockedExchange(&g_err, e); }
static UINT_PTR WINAPI S_socket(int, int, int) { return ~(UINT_PTR)0; }
static int WINAPI S_closesocket(UINT_PTR) { return 0; }
static INT_PTR WINAPI S_fail() { InterlockedExchange(&g_err, 10050); return kSocketError; }
static int WINAPI S_getaddrinfo(const char*, const char*, const void*, void** res) { if (res) *res = nullptr; return 11001; } // WSAHOST_NOT_FOUND
static void WINAPI S_freeaddrinfo(void*) {}
static int WINAPI S_gethostname(char* name, int len) { if (name && len > 4) { strcpy_s(name, len, "xbox"); return 0; } return -1; }
static int WINAPI S_FDIsSet(UINT_PTR, void*) { return 0; }
static int WINAPI S_select(int, void*, void*, void*, const long* tv) {
    DWORD ms = tv ? (DWORD)(tv[0] * 1000 + tv[1] / 1000) : 10;
    Sleep(ms);
    return 0;
}
static ULONG WINAPI S_htonl(ULONG v) { return _byteswap_ulong(v); }
static USHORT WINAPI S_htons(USHORT v) { return _byteswap_ushort(v); }

#define W(key, fn) {"ws2_32.dll", key, (void*)&fn}
const ShimEntry g_netShims[] = {
    W("#115", S_WSAStartup), W("#116", S_WSACleanup), W("#111", S_WSAGetLastError), W("#112", S_WSASetLastError),
    W("#23", S_socket), W("#3", S_closesocket), W("#1", S_fail), W("#2", S_fail), W("#4", S_fail), W("#5", S_fail), W("#6", S_fail),
    W("#7", S_fail), W("#10", S_fail), W("#13", S_fail), W("#16", S_fail), W("#17", S_fail), W("#19", S_fail), W("#20", S_fail), W("#21", S_fail),
    W("#18", S_select), W("#151", S_FDIsSet), W("#57", S_gethostname),
    W("#8", S_htonl), W("#14", S_htonl), W("#9", S_htons), W("#15", S_htons),
    W("getaddrinfo", S_getaddrinfo), W("freeaddrinfo", S_freeaddrinfo), W("WSAIoctl", S_fail),
    {nullptr, nullptr, nullptr},
};
