# Roadmap

## 01 Launcher
UWP C#/XAML, SteamKit2 (QR + conta/Steam Guard), download do depot 758332 pinado em manifest, verificação por hash, armazenamento em `LocalState`. Saída: pacote `.appx` instalável no Xbox. Critério: baixa e verifica o jogo no Xbox/PC.

## 02 Auditoria de imports
Script que lê o PE (imports/exports/seções/relocs/TLS) de `Shenmue.exe`, `Shenmue2.exe` e DLLs e classifica cada API: permitida em UWP / precisa de shim / bloqueada. Saída: `rea/findings/import-audit.md`. Critério de decisão: go/no-go do port.

## 03 Loader de PE + shims
App UWP nativo `ShenmueGame.exe` que mapeia o PE e resolve imports via shims (janela→fila própria, swapchain→CoreWindow, input→Windows.Gaming.Input, steam_api64 stub, XAudio2 2.7→2.9), D3D11 nativo. Testado no sandbox UWP do PC. Critério: chega ao gameplay. Detalhes em `stages/03-pe-loader/README.md`.

## 04 Teste no Xbox
Deploy via Device Portal, medir memória (limite ~5 GiB), estabilidade, entrada por controle. Critério: jogo jogável.

## 05 Mods
Camada de overrides de arquivos (tradução/texturas), widescreen patch, compatibilidade com hooks de dxgi.
