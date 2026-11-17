// DLL mínima com bloco TLS reservado. O loader do SO aloca esse bloco em toda thread; o host copia o template do módulo
// do jogo para cá e faz o _tls_index do jogo apontar para o índice desta DLL. Compilada uma vez por DLL do jogo com TLS,
// com SLOT_FN diferente em cada uma (ver build.ps1), e importada estaticamente pelo ShenmueGame.exe.
#include <windows.h>

extern "C" ULONG _tls_index;
extern "C" const IMAGE_TLS_DIRECTORY64 _tls_used;

__declspec(thread) volatile char g_reserved[1024];

extern "C" __declspec(dllexport) void __cdecl SLOT_FN(ULONG* index, void** templ, size_t* size) {
    g_reserved[0] = 0; // mantém a variável no binário
    *index = _tls_index;
    *templ = (void*)_tls_used.StartAddressOfRawData;
    *size = (size_t)(_tls_used.EndAddressOfRawData - _tls_used.StartAddressOfRawData);
}
