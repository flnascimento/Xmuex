# Xmuex

> ## Notice / Aviso
>
> **EN:** This is a personal fan project, built to run my own legally purchased Steam copy of **Shenmue I & II** on my own Xbox. There is no intent of piracy or fraudulent use whatsoever.
>
> The game is downloaded exclusively through the end user's own Steam account, via official Steam login (QR code), with no bypass of that verification. I do not distribute, have never distributed, and will never distribute any game file or any SEGA / Shenmue Production Committee asset. I do not include any third-party mods (translation, HD textures, widescreen patch) — only the mechanism that applies them, for users who already own them on their own. The sole intent is to share, for free, with other fans who also already own the game, a way to play it at the best possible quality on an Xbox with Dev Mode.
>
> **Commitment:** if SEGA, the Shenmue Production Committee, or Valve believe this project infringes any right, even unintentionally (and despite AI-assisted research done in good faith to try to understand whether I might be infringing anything), please get in touch (open an issue in this repository) — it will be made private or taken down immediately, no questions asked.
>
> **On forks and copies:** this code is open for transparency, not to become a piracy tool. I do not authorize forks or redistributions that remove, replace, or bypass the Steam-based ownership verification used to unlock access to the game — see the additional condition in `LICENSE`.
>
> I'm just a fan who wants to play Shenmue on Xbox. And to extend that to others who, like me, own the original game and want to do the same. Nothing more than that.
>
> ---
>
> **PT-BR:** Este é um projeto pessoal, de fã, feito para rodar no meu próprio Xbox a minha própria cópia de **Shenmue I & II**, comprada legalmente na Steam. Não há nenhuma pretensão de pirataria ou uso fraudulento.
>
> O jogo é baixado exclusivamente pela conta Steam do próprio usuário, por login oficial (QR code), sem nenhum tipo de bypass dessa verificação. Não distribuo, nunca distribuí e nunca vou distribuir nenhum arquivo do jogo, nem qualquer asset da SEGA / Shenmue Production Committee. Não incluo mods de terceiros (tradução, texturas em alta definição, widescreen) — só o mecanismo que os aplica, para quem já os possui por conta própria. A única intenção é compartilhar, de graça, com outros fãs que também já compraram o jogo, uma forma de jogá-lo com a melhor qualidade possível no Xbox com Dev Mode.
>
> **Compromisso:** se a SEGA, o Shenmue Production Committee ou a Valve entenderem que este projeto infringe algum direito, mesmo que de forma não intencional (e com pesquisas feitas com apoio de IA para tentar entender se eu estaria infringindo algo), peço por gentileza que entrem em contato (issue neste repositório) — ele será tornado privado ou removido imediatamente, sem discussão.
>
> **Sobre forks e cópias:** o código é aberto para transparência, não para virar ferramenta de pirataria. Não autorizo forks ou distribuições que removam, substituam ou burlem a verificação de posse feita pela própria Steam para liberar o acesso ao jogo — ver a condição adicional na `LICENSE`.
>
> Sou só um fã que quer jogar Shenmue no Xbox. E estender isso a outros que, assim como eu, têm o jogo original e querem fazer o mesmo. Nada além disso.

Port pessoal de **Shenmue I & II (Steam)** para **Xbox Series S|X em Modo de Desenvolvedor**, sem emulação: o Xbox já é Windows x64, então o `Shenmue.exe`/`Shenmue2.exe` original da Steam roda mapeado em memória por um loader de PE próprio (PE = *Portable Executable*, o formato dos `.exe`/`.dll` do Windows — o loader lê esse formato e carrega o executável na memória, do mesmo jeito que o Windows faz ao abrir um programa), com shims que traduzem as chamadas Win32 que ele faz para as APIs disponíveis em UWP (janela, Direct3D 11, XInput, XAudio2). Nome: trocadilho entre **Xbox** e **Shenmue**.

Um launcher UWP próprio baixa o jogo **direto da Steam, pela conta do usuário** (login por QR code, sem senha salva) — ninguém precisa, nem pode, copiar arquivos por USB.

## Regras
- Nenhum arquivo do jogo, binário, save, certificado ou credencial entra neste repositório.
- Sem importação por USB/Wi-Fi: somente Steam (QR ou login + Steam Guard). Nenhuma senha/token é salvo.
- Download pinado por build/manifest e verificado por hash.
- Mods de PC (widescreen, tradução, texturas) **não são distribuídos aqui** — ver `docs/MODS.md` para importar os seus.

Ver `docs/LEGAL.md` para o raciocínio completo sobre direitos autorais.

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

## Instalar
Baixe o pacote pronto em [Releases](https://github.com/flnascimento/Xmuex/releases) (o `.msix` e as dependências `.appx`) ou compile o seu com `tools/build.ps1` — ver `docs/DEPLOY.md`.

## Alvo
Jogo: Shenmue I & II (Steam), appid 758330, depot 758332, build 3291213 (manifest 2953351546760814442).

## Licença
Código deste repositório sob MIT com uma condição adicional de uso responsável (`LICENSE`): forks não podem remover a verificação de posse feita pela própria Steam. Não cobre o jogo nem os mods de terceiros.
