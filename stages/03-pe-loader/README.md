# Etapa 03 – Runtime nativo (loader de PE + shims)

**Objetivo:** executar o `Shenmue.exe`/`Shenmue2.exe` originais da Steam dentro de um app UWP, sem alterar os arquivos do jogo.

**Pronto quando:** o jogo chega ao gameplay no sandbox UWP do PC.

**Estado:** concluído no PC em 2026-11-07 (Shenmue I: menu, New Game, cena de abertura; Shenmue II: título e abertura). Áudio inicializa, mas foi testado mudo. Sem teste com controle físico.

## Como funciona
`ShenmueGame.exe` (app UWP nativo, `CoreWindow`) mapeia o exe do jogo na memória e resolve cada import nesta ordem:
1. **Shim** (`src/shim_*.cpp`);
2. **DLL do próprio jogo** (Oodle, GameHangon), mapeada pelo mesmo loader;
3. **Sistema/pacote** (`GetProcAddress`);
4. **Trap**: registra `TRAP` no `host.log` e devolve 0, para não derrubar o jogo.

| Arquivo | Papel |
|---|---|
| `src/main.cpp` | App UWP, ativação por protocolo `shenmue-uwp-run:N` |
| `src/host.cpp` | Resolução de imports, TLS, thunks, log, VEH, watchdog |
| `src/pe_loader.cpp` | Mapeamento, relocações, imports, proteções, tabela de exceções |
| `src/tls_slot.cpp` | `ShTlsA..C.dll`: blocos TLS reais para as DLLs do jogo (imports estáticos do exe) |
| `src/shim_kernel.cpp` | Módulos, `LoadLibrary`, `GetModuleHandle`, `_CxxThrowException`, stdout→log |
| `src/shim_user32.cpp` | Janela e fila de mensagens falsas |
| `src/shim_dx.cpp` | Swapchain proxy, DXGI falso, `D3DReflect` _43→_47 |
| `src/shim_audio.cpp` | XAudio2 2.7 sobre 2.9 |
| `src/shim_winrt.cpp` | XInput sobre `Windows.Gaming.Input` |
| `src/shim_steam.cpp` | `steam_api64` sem cliente Steam |
| `src/shim_steamcloud.cpp` | Steam Cloud (saves e opções) em `LocalState\cloud` + call results assíncronos |
| `src/shim_net.cpp` | ws2_32 sem rede: jogo offline |
| `src/shim_misc.cpp` | imm32, dwmapi, dbghelp, registro, shell32, winmm |

Build: `tools/build.ps1` (ou `build.ps1 exe` nesta pasta). Teste: `tools/pc_test.ps1`.

## Log de decisões
- **App nativo separado do launcher C#:** o código do exe do jogo acessa o TLS como `gs:[58h][0]`, índice 0 fixo, que é sempre do .exe principal do processo. Por isso o host precisa ser um .exe próprio, com bloco TLS reservado. O launcher abre o jogo por protocolo.
- **App nativo entra no pacote na etapa final** (`tools/build.ps1`): o build CoreCLR do C# trata todo `Application` do manifesto como gerenciado e falha com um exe nativo.
- **TLS das DLLs:** `ShTls*.dll` são carregadas pelo SO, então toda thread (inclusive as do sistema, como as do XAudio2) recebe bloco válido sem hook de criação de thread.
- **Swapchain proxy:** o jogo desenha em textura nossa (`R8G8B8A8_UNORM_SRGB`), copiada no `Present` para o swapchain flip de `CoreWindow`, que não aceita sRGB.
- **DXGI falso:** enumeração fixa (1920x1080 e 1280x720), porque no console a enumeração real pode não existir.
- **`_CxxThrowException` próprio:** o original usa `RtlPcToFileHeader`, que não conhece módulos mapeados manualmente.
- **`D3DReflect`:** as structs do `d3dcompiler_47` são maiores que as do `_43`; wrappers copiam só o tamanho antigo.
- **VC runtime no pacote:** `vcruntime140_app`/`msvcp140_app` retail ficam ao lado do exe, porque o pacote C# Debug só depende do VCLibs Debug.
- **Sem rede:** a única rede do jogo era telemetria (`garry.sgaas.net`, via libcurl). Sem ws2_32 real, o jogo não depende de rede no console.
- **Steam:** stubs; a posse do jogo é verificada no download pelo launcher (chave do depot da conta do usuário).

- **Saves pela Steam Cloud:** com a Steam "ligada", o jogo grava saves e opções (inclusive o idioma das legendas, em `sm1saves/Settings`) só pela `ISteamRemoteStorage` (campo `+0x48` do contexto Steam). `shim_steamcloud.cpp` implementa a interface em arquivos locais, no mesmo formato da pasta `remote` da Steam no PC, e entrega os resultados assíncronos via `SteamAPI_RunCallbacks` (vtable do `CCallbackBase` no MSVC: [0] = `Run(param, ioFailure, handle)`).

## Pendências conhecidas
- Teclado real não funciona bem no jogo (`GetRawInputData` é stub; só afeta o teste no PC).
- Idioma fixo em inglês (`GetCurrentGameLanguage`).
- Conquistas e estatísticas Steam não funcionam.
- Sem tratamento de suspensão do app no console.
