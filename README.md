# Xmuex

Port pessoal de **Shenmue I & II (Steam)** para **Xbox Series S|X em Modo de Desenvolvedor**, sem emulação: o Xbox já é Windows x64, então o `Shenmue.exe`/`Shenmue2.exe` original da Steam roda mapeado em memória por um loader de PE próprio, com shims que traduzem as chamadas Win32 que ele faz para as APIs disponíveis em UWP (janela, Direct3D 11, XInput, XAudio2). Nome: trocadilho entre **Xbox** e **Shenmue**.

Um launcher UWP próprio baixa o jogo **direto da Steam, pela conta do usuário** (login por QR code, sem senha salva) — ninguém precisa copiar arquivos por USB. Inspirado em [UWP-Spider-Launcher](https://github.com/BBCPrince/UWP-Spider-Launcher), que faz o mesmo para outro jogo.

## Regras
- Nenhum arquivo do jogo, binário, save, certificado ou credencial entra neste repositório.
- Sem importação por USB/Wi-Fi: somente Steam (QR ou login + Steam Guard). Nenhuma senha/token é salvo.
- Download pinado por build/manifest e verificado por hash.
- Mods de PC (widescreen, tradução, texturas) **não são distribuídos aqui** — ver `docs/MODS.md` para importar os seus.

Ver `NOTICE.md` para a declaração de intenção e boa-fé (PT-BR/EN), e `docs/LEGAL.md` para o raciocínio completo sobre direitos autorais.

## Etapa atual
Ver [STATUS.md](STATUS.md).

## Estrutura
| Pasta | Conteúdo |
|---|---|
| `stages/01-launcher` | Launcher UWP (C#/XAML) + download Steam via SteamKit2 |
| `stages/02-import-audit` | Auditoria de imports do `Shenmue.exe`/DLLs (decide o que precisa de shim) |
| `stages/03-pe-loader` | Runtime nativo `ShenmueGame.exe`: loader de PE + shims Win32→UWP |
| `stages/04-xbox-test` | Notas de teste no console |
| `stages/05-mods` | Sistema de mods embutidos no pacote (código; os arquivos de cada mod ficam de fora — `docs/MODS.md`) |
| `rea/findings/` | Achados de engenharia reversa (comportamento do jogo, não trechos do binário) |
| `docs/` | Roadmap, deploy no Xbox, mods, legal |
| `tools/` | `bootstrap.ps1` (setup), `gen_cert.ps1` (certificado de assinatura), `build.ps1` (gera o pacote), `pc_test*.ps1` (testa no PC), `import_mods.py` |

## Compilar e instalar
Ver `docs/DEPLOY.md`.

## Alvo
Jogo: Shenmue I & II (Steam), appid 758330, depot 758332, build 3291213 (manifest 2953351546760814442).

## Licença
Código deste repositório sob MIT com uma condição adicional de uso responsável (`LICENSE`): forks não podem remover a verificação de posse feita pela própria Steam. Não cobre o jogo nem os mods de terceiros.
