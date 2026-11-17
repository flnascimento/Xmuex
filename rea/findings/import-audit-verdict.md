# Veredito da auditoria de imports (etapa 02)
- **Data:** 2026-11-07
- **Binários:** `sm1/Shenmue.exe` (sha1 8d78635…), `sm2/Shenmue2.exe`, DLLs do jogo. Dados completos: `import-audit.md` (gerado por `rea/scripts/pe_audit.py`).
- **Pergunta:** o executável é portável para UWP via loader de PE + shims?
- **Resposta: GO (com riscos).**

## Achados
1. **Superfície pequena e favorável.** PE64 x64 com relocações (`.reloc`), sem packer/DRM visível (sem seção tipo `.bind`; entropia de `.text` normal), imagem ~160 MiB (cabe nos 5 GiB).
2. **Gráficos:** apenas `D3D11CreateDeviceAndSwapChain` e `CreateDXGIFactory`. O D3D11 existe em UWP; o ponto crítico é o swapchain criado para HWND → o shim deve criar o device e um swapchain de CoreWindow (`CreateSwapChainForCoreWindow`) e devolvê-lo ao jogo. `d3dcompiler_43!D3DReflect` → `d3dcompiler_47` empacotado.
3. **Janela/entrada (`user32`, 44 funções):** só janela/mensagens/cursor/clipboard/raw input; shims simples (janela fantasma + fila de mensagens alimentada por CoreWindow/Windows.Gaming.Input). `imm32` (2) e `dwmapi` (1): stubs.
4. **Controle:** `xinput1_3` ordinal 2 (XInputGetState) → shim para `Windows.Gaming.Input` (ou xinputuap).
5. **Áudio:** `xapofx1_5!CreateFX` (efeito XAPO de reverb/volume) → shim com XAudio2 do UWP; o áudio em si deve passar por XAudio2 via COM, disponível.
6. **Steam:** `steam_api64` (12 funções) → stub: `Init` verdadeiro, `RestartAppIfNecessary` falso, interfaces vazias.
7. **Rede/LDAP:** `ws2_32` ok em UWP; `wldap32` (16 ordinais, provável libcurl) → stubs que falham.
8. **Diagnóstico:** `dbghelp` (3) → stubs.
9. **Compressão:** Oodle (`oo2core`/`oo2ext`) e `zlibwapi`, `libfbxsdk`: DLLs x64 próprias, só kernel32/CRT. Mapeadas pelo mesmo loader (ou compiladas fora de uso).
10. **CRT:** `api-ms-win-crt-*`, `msvcp140`, `vcruntime140`: fornecer via pacote UWP do VC runtime.
11. **Descartáveis:** `renderdoc.dll`, `TootleDLL_2015_64.dll` (d3d9/d3dx9) não são importadas pelo exe principal.

## Riscos
- Carregar PE manualmente exige memória executável (`VirtualAlloc` RWX / capability de JIT) permitida em Dev Mode; validar no Xbox.
- O jogo faz chamadas diretas de kernel32 (76) que precisam existir: todas são "UWP ok" na lista do SDK.
- TLS callbacks e SEH (`.pdata`) exigem registro com `RtlAddFunctionTable`.
- Arquivos: o jogo espera caminhos de disco do Windows → redirecionar para `LocalState\game`.
- Janela de 1 swapchain: resolução/Widescreen dependem de patch no exe (etapa 05).

## Próximos passos
Etapa 03: host nativo C++ UWP (projeto em `stages/03-pe-loader`) com loader, tabela de imports/shims na ordem de prioridade: kernel32/CRT → user32 → D3D11/DXGI → XInput → XAudio2 → Steam stub.
