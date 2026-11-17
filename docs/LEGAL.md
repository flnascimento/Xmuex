# Escopo legal

- Requer copia legitima do jogo na conta Steam do usuario.
- Este repositorio nao contem nem distribui codigo, assets ou binarios da SEGA/Steam, nem dos mods de terceiros (ver `docs/MODS.md`).
- O download do jogo e feito pela conta do proprio usuario, no dispositivo dele, por login QR da Steam (SteamKit2). As credenciais nunca sao persistidas.
- E a propria Steam quem verifica a posse: sem a licenca na conta do usuario, os servidores da Steam nao entregam a chave do depot. O launcher nao contorna isso em nenhum momento — so pede o que a conta ja tem direito a baixar.
- Engenharia reversa foi feita para interoperabilidade pessoal (rodar o jogo ja comprado em outro hardware da mesma familia Windows). Os achados em `rea/findings/` descrevem comportamento (que DLLs o jogo importa, como ele acessa arquivos), sem reproduzir trechos do binario.
- `rea/findings/import-audit-verdict.md` confirma que o executavel nao tem wrapper de DRM (sem SteamStub, sem packer): o Steamworks (`steam_api64`) e usado so para recursos opcionais (conquistas, nuvem), nao como trava de licenca. O runtime substitui esse DLL por um stub porque o Xbox nao tem como rodar um cliente Steam de verdade (sandbox UWP) — nao porque ha algo para "remover".
- Arquivos do jogo sao mapeados em memoria sem alteracao em disco; os patches de mods (ex.: widescreen) tambem sao aplicados na memoria, nunca no arquivo baixado.

Isto nao e aconselhamento juridico. O projeto segue o mesmo modelo publico de outros launchers UWP com download autenticado pela Steam (ex.: [BBCPrince/UWP-Spider-Launcher](https://github.com/BBCPrince/UWP-Spider-Launcher)), mas cada usuario e responsavel por como usa o software, na sua jurisdicao.
