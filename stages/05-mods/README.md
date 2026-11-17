# Etapa 05 – Mods

**Objetivo:** mods de PC já incluídos no pacote, sem o usuário baixar ou instalar nada, e sem alterar os arquivos do jogo baixados da Steam.

**Pronto quando:** widescreen e legendas em português funcionam no Xbox.

**Estado:** em andamento. v0.12 (texturas) testada só no PC (2026-11-17); widescreen/legendas na v0.10 também só no PC.

## Mods incluídos
| Mod | Autor | Jogo | Como entra |
|---|---|---|---|
| Widescreen nas cenas (Widescreen Patcher v2.1.8) | ner0, com base no trabalho de Esppiral (Shenmue Dojo) | I e II | Patches de bytes aplicados na memória ao abrir o jogo |
| Legendas em português | Tribo Gamer (tribogamer.com) | I | Arquivos `.tad`/`.tac` lidos de dentro do pacote por uma pasta virtual `archives\dx11\mods` |
| Texturas em alta definição (Streamer2k Textures - Better Textures for Shenmue v4.1) | Streamer2k (Nexus Mods) | I | Dois `.tac`/`.tad` grandes (~5,2 GB) instalados de verdade em `archives\dx11\data`, mais modelos/cenas pela mesma pasta virtual `archives\dx11\mods` |

Opções experimentais do Shenmue II (vêm do mesmo patcher, desligadas por padrão): sem bordas pretas nas cenas (inclui o ajuste de posição das legendas) e sem o ícone de câmera.

## Como usar
- No launcher, cada cartão de jogo tem as caixas de seleção dos mods. As opções ficam em `LocalState\mods.ini` e valem na próxima vez que o jogo abrir.
- **Legendas em português:** o mod substitui as legendas de outro idioma do jogo. Com Inglês selecionado as legendas continuam em inglês (conferido no PC). Escolha o idioma no menu de opções do jogo. Qual idioma o mod substitui ainda não foi confirmado.
- **Texturas em alta definição:** desligada por padrão (ao contrário dos outros mods) — é um pack grande e a primeira vez que liga demora para copiar os arquivos (na ordem de 1 minuto no PC; o jogo fica "parado" nessa hora, antes mesmo de aparecer a janela). Desligar apaga os dois arquivos que o próprio mod instalou.

## Como montar
1. Deixe as pastas originais em `projects/shenmue/mods-src/` (fora do git): `Shenmue I & II - Widescreen Patcher/`, `Tradução Shenmue/` e `Better Textures for Shenmue v4.1.../` (ou qualquer pasta cujo nome comece com "Better Textures").
2. `python tools/import_mods.py`: lê o `.bat` do patcher, separa os padrões da versão pinada (Shenmue I v1.06/1.07 e Shenmue II v1.07), confere que cada um aparece uma única vez no código dos exes da Steam, copia a tradução para `payload/overlay/` e as texturas para `payload/data/` (arquivos "de verdade") e `payload/overlay_tex/` (modelos/cenas).
3. `tools/build.ps1` inclui `payload/` no pacote como `mods/`. O `.tac` da tradução (459 MB) e os das texturas (~5,2 GB) comprimem bastante no `.msix` (pacote completo ficou com 2,97 GB na v0.12, texturas incluídas).

O instalador NSIS da tradução não foi executado. Pelo conteúdo listado com 7-Zip, ele copia exatamente os dois arquivos da pasta `sm1` (mais créditos e desinstalador). O pack de texturas (zip, sem instalador) segue a instalação padrão da comunidade: pasta `archives\dx11\data\` direto na raiz do jogo e pasta `mods\...` dentro de `archives\dx11\`.

## Log de decisões
- **Patches em memória, não no disco:** o runtime procura cada sequência original nas seções executáveis do exe mapeado e só troca se houver ocorrência única; senão registra no `host.log` e segue sem aquele patch. Assim o arquivo baixado continua original e "Verificar arquivos" segue válido.
- **Sobreposição sem cópia, com várias fontes:** o jogo lista `archives\dx11` e lê tudo o que encontra (`FindFirstFileA "%s/*"`). O runtime acrescenta `mods` à listagem quando a pasta real não existe e redireciona `archives\dx11\mods\...`, em `CreateFileA`, `FindFirstFileA`, `GetFileAttributes*`, `fopen`, `_fsopen` e `_stat64`. Tradução e texturas moram em pastas próprias no pacote (`overlay/` e `overlay_tex/`); o redirecionamento tenta cada uma (na ordem) e usa a primeira que tiver o arquivo pedido — assim cada mod liga/desliga independente, mesmo compartilhando a mesma pasta virtual do jogo. Não gasta espaço extra no console.
- **Texturas: cópia de verdade, não sobreposição.** `archives\dx11\data` é onde a Steam já guarda vários `.tac`/`.tad` com nomes diferentes (sem nenhum arquivo de manifesto encontrado) — o próprio jogo enumera esse diretório para descobrir os arquivos, exatamente como a instalação manual desse mod no PC funciona. Não dá para fingir isso com um redirecionamento de caminho (como no `mods`); os dois arquivos do pacote (`Streamer2K_Better_Textures_41 part1/2.tac/.tad`, nomes exclusivos do mod) são copiados para dentro da pasta baixada do jogo ao ligar, e apagados ao desligar — só esses quatro nomes, nunca outro arquivo.
- **Conteúdo de terceiros fora do git:** patcher, tradução, texturas e `payload/` estão no `.gitignore`. O repositório só tem o script que os converte. Uso pessoal; não redistribuir sem autorização dos autores.
- **`tools/pc_test.ps1` isolado da Steam real:** antes, a pasta `archives` do teste era uma *junction* direta para a instalação real da Steam (evita copiar 13 GB). Como o mod de texturas agora escreve em `archives\dx11\data`, isso mexeria na pasta real do jogo no PC. Só essa subpasta passou a ser uma pasta de verdade com hardlink por arquivo (ainda sem copiar nada); o resto da árvore continua por junction.

## Pendências
- Confirmar no Xbox: widescreen nos dois jogos, legendas em português e texturas em alta definição.
- Descobrir qual idioma do menu mostra a tradução e deixar isso escrito aqui.
- Conferir visualmente, depois de andar até as cenas 02/03, se os modelos/texturas da sobreposição (`overlay_tex`) aparecem puxados (só os arquivos de `archives\dx11\data` foram confirmados em uso no teste do PC).
