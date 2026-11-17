# Achados de engenharia reversa para o runtime (etapa 03)
- **Data:** 2026-11-07
- **Binário:** `sm1/Shenmue.exe` (sha1 8d78635…). `Shenmue2.exe` segue o mesmo padrão; RVAs valem só para o Shenmue I.
- **Método:** análise estática com `rea/scripts/re_lib.py` (capstone + `.pdata`) e execução com `host.log`. O CLI do REA (`npx rea-agents`) responde, mas o Ghidra não está instalado; não foi necessário até aqui.

## Achados
| Tema | Achado | Evidência (RVA) | Impacto no port |
|---|---|---|---|
| TLS | Exe acessa `gs:[58h]` → `[rax]`, índice 0 fixo | 0x2b81f0 (alocador por thread) | Host precisa ser o .exe principal |
| TLS | exe 36 bytes, GameHangon 8, oo2ext 72; sem callbacks | diretório TLS | Blocos pequenos; `ShTls*.dll` bastam para as DLLs |
| D3D | `D3D11CreateDeviceAndSwapChain`: níveis 11_1/11_0, `R8G8B8A8_UNORM_SRGB`, 1 buffer, DISCARD, usage 0x30 | 0x2913a6 | Swapchain proxy |
| Swapchain | Só usa Present (+0x40), GetBuffer (+0x48), SetFullscreenState (+0x50), ResizeBuffers (+0x68), Release | referências a 0xed8128 | Proxy pequeno |
| DXGI | `CreateDXGIFactory` só enumera adaptador/saídas/modos | 0x2b9860, 0x2b95d0, 0x2b9480 | Enumeração falsa |
| Janela | Retângulo vem da saída DXGI e do `GetMonitorInfoA` | 0x24a241, 0x2b9591 | Valores devem ser coerentes |
| Cursor | `do { r = ShowCursor(TRUE); } while (r <= 0)` | 0x245763 | `ShowCursor` precisa de contador real, senão trava |
| Áudio | XAudio2 **2.7** via COM (`LoadLibraryExW("XAudio2_7.DLL")` + `CoCreateInstance`), limiter via `XAPOFX1_5!CreateFX`, reverb via `CoCreateInstance` | 0x26fe40 | Fachada 2.7 sobre 2.9 |
| Áudio | Voz master fixa: 6 canais, 48 kHz | 0x26ff1b | Sem teste em console |
| CLSIDs | XAudio2 `5a508685-a254-4fba-9b82-9a24b00306af`, Reverb `6a93130e-1d53-41d1-a9cf-e758800bb179` | 0x925f38, 0x925ec0 | Interceptados em `CoCreateInstance` |
| Arquivos | Caminhos relativos ao diretório atual (`./tex/cache`, `.\sm1saves\`, `%s/Archives/%s/`); só muda de pasta em build de desenvolvimento | 0x24a3a5 | Host faz `SetCurrentDirectory` para a pasta do jogo |
| Steam | `RestartAppIfNecessary` e `Init` em 0x2bd680; interfaces via `SteamInternal_CreateInterface` + `ContextInit` | 0x2bd680 | Stubs; slots usados aparecem no `host.log` |
| Saves | Com Steam ativa (flag 0xd87da5), saves e opções vão só para `ISteamRemoteStorage` (ctx+0x48): FileWrite/FileRead/FileExists/GetFileSize, FileWriteAsync/FileReadAsync com `CCallResult` (callbacks 1331/1332), GetQuota em uint64 | 0x2487f0, 0x249270, 0x249340, 0x249130 | Nuvem local em `shim_steamcloud.cpp`; `rea/scripts/steam_slots.py` lista os slots usados |
| Rede | Telemetria: libcurl envia eventos para `https://garry.sgaas.net/event` numa thread, logo na abertura | 0x915490, 0x2eece0 | `getaddrinfo` falso derruba o jogo; shim de ws2_32 sem rede |
| Outros | libcurl embutido (carrega `secur32`, lê registro para dados de hardware), YEBIS (pós-efeitos), imgui | strings | Registro devolve "não encontrado" |

## Pendências de RE
- Efeito da falta de `secur32.dll` no console, se ela não existir.
- Formato de `archives/dx11` (será necessário para mods de texturas/tradução, etapa 05).
