# Etapa 04 – Teste no Xbox (Series S)

**Objetivo:** rodar o pacote no console em Dev Mode e medir o que o PC não mostra.

**Pronto quando:** o jogo é jogável com controle.

**Estado:** concluído com a v0.9 (2026-11-07), pelo relato do usuário. Passo a passo em `docs/DEPLOY.md`.

## O que observar
- [ ] Instala como atualização, sem apagar o jogo baixado
- [ ] Um só tile, sem a pergunta de qual app abrir
- [ ] Pacote marcado como **Game**
- [ ] JOGAR abre o jogo
- [ ] Logos SEGA/d3t e tela-título
- [ ] Controle funciona nos menus e no jogo
- [ ] Som
- [ ] Estável por 30 min; save carrega depois de reabrir
- [ ] Shenmue II

## O que pode ser diferente do PC
- Memória executável (`VirtualAllocFromApp` + `codeGeneration`): testada só no PC.
- DLLs do sistema ausentes no console: viram `TRAP` no `host.log`.
- Nível do Direct3D: o jogo exige 11_0; sem o modo Game pode falhar em `D3D11CreateDevice`.
- Memória: 1,9 GB de CPU no PC; a memória de GPU no console ainda não foi medida.

## Log de testes e decisões
- **v0.7 (2026-11-07):** o tile abria como uma pasta perguntando qual app usar; JOGAR deu tela preta e fechou. O log do console não foi visto, então a causa não está confirmada.
- **v0.8:** resposta ao teste acima, sem acesso ao log:
  - Tile único: app do jogo com `AppListEntry="none"`. O Xbox agrupa os apps do mesmo pacote; a causa da "pasta" é inferida, não confirmada.
  - `expandedResources` pede recursos de Game para o pacote todo (ideia vinda da documentação do UWP-Spider-Launcher).
  - Modo estrito no PC (`strict.txt`) simula o console sem `kernel32`, `user32`, `ws2_32`, `advapi32`, `ole32` e `shell32`. Com a v0.7 ele derrubava o jogo em `getaddrinfo` (telemetria), no início, com sintoma igual ao relatado. Na v0.8 os dois jogos passam.
  - Jogo offline (`shim_net.cpp`): a única rede era telemetria da SEGA.
  - `ShTls*.dll` e `d3dcompiler_47.dll` viraram imports estáticos: `LoadPackagedLibrary` falhou com erro 126 no primeiro lançamento após instalar no PC, o mesmo caso do primeiro teste no console.
  - XAudio2 carregado sob demanda: se faltar, o jogo fica sem som em vez de nem abrir.
  - Launcher mostra o resultado da última execução; `boot.log` registra as etapas antes do `host.log`.
- **v0.8 (2026-11-07):** o usuário relatou "Funcionou!" no Xbox Series S. Detalhes ainda não informados: som, controle, estabilidade, Shenmue II e se o tile único resolveu a pergunta. A causa exata da falha da v0.7 segue sem confirmação.
- **Próximos passos combinados:** o usuário vai procurar melhorias; depois, mods (etapa 05).
- **Teste da v0.8:** funciona, mas apertar B fecha o jogo. Causa: no Xbox o B dispara o "voltar" do sistema (`BackRequested`) e o app nativo não tratava.
- **v0.9:**
  - O app do jogo trata `BackRequested`, e os botões do controle não são mais repassados ao jogo como teclas (`VK_GAMEPAD_*`); o jogo lê o controle só por XInput. Não verificado com controle real; só no Xbox.
  - Launcher novo (pedido do usuário): login só por QR, com o QR sempre na tela; um cartão por jogo com estado, JOGAR, baixar, verificar e apagar. Download separado por jogo (`sm1` ~4,6 GB, `sm2` ~9,2 GB).
- **Teste da v0.9:** usuário confirmou "Tudo certo": o B não fecha mais o jogo e o launcher novo funciona. Itens da lista sem confirmação explícita: estabilidade de 30 min e save após reabrir.
- **v0.10 no Xbox (relato):** não salva. Causa: o jogo grava saves e opções pela Steam Cloud (`ISteamRemoteStorage`), e o stub recusava tudo. O idioma da legenda também não ficava gravado, porque fica em `sm1saves/Settings`.
- **v0.11:** Steam Cloud local em `LocalState\cloud` (compartilhada pelos dois jogos e preservada ao apagar um jogo). No PC: os saves copiados da Steam aparecem e carregam, e as gravações do jogo (`BOOKKEEP`, `Stats0`) funcionam. O save manual pelo menu não foi exercitado no PC.

