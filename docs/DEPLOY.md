# Compilar, instalar e testar no Xbox

## 0. Pré-requisitos (uma vez por máquina)
- Windows 10/11 com Visual Studio Build Tools (workloads C++/UWP) e Windows SDK: rode `tools/bootstrap.ps1` elevado (também ativa o Modo de Desenvolvedor do Windows).
- Xbox Series S|X em Modo de Desenvolvedor (Dev Home), na mesma rede.
- Certificado de assinatura: `tools/gen_cert.ps1` (gera `certs/dev.pfx`, fora do git — cada máquina gera o seu).

## 1. Compilar
```
tools/build.ps1
```
Gera `stages/01-launcher/out/Shenmue_<versao>.msix`.

## 2. Instalar
1. Device Portal (`https://IP:11443`) → *My games & apps* → *Add*.
2. Selecione o `.msix`. É atualização: o jogo já baixado em `LocalState\game` continua lá.
3. Se pedir dependências, adicione os arquivos de `stages/01-launcher/out/ShenmueLauncher_<versao>_x64_Test/Dependencies/x64/`:
   `Microsoft.NET.CoreRuntime.2.2.appx`, `Microsoft.NET.CoreFramework.Debug.2.2.appx`, `Microsoft.VCLibs.x64.14.00.appx`.

   São componentes da própria Microsoft (parte do SDK/NuGet `Microsoft.NETCore.UniversalWindowsPlatform`, usados por qualquer app UWP em C# feito para rodar fora da Store), não deste projeto — vêm junto do pacote de build na pasta `Dependencies/x64` e também nas Releases deste repositório. Mesmo compilando em Release, a dependência `.Debug` do framework .NET persiste porque este app executa via CoreCLR/JIT (não compilado nativamente), e só por isso o Windows exige o dispositivo em Modo de Desenvolvedor/sideload liberado para instalar — o mesmo cenário em que o Xbox já está. Não existe link oficial de download avulso da Microsoft para esses três arquivos fora do SDK/NuGet.

O console mostra um só tile, **Shenmue Launcher**. O app do jogo fica oculto e é aberto pelo botão JOGAR.

## 3. Marcar o pacote como "Game"
O tipo vale para o pacote inteiro. Sem isso o console dá poucos recursos e pode recusar o Direct3D 11 que o jogo exige.
O pacote pede esses recursos sozinho (`expandedResources`), mas essa capability é obsoleta, então marque também:
1. No Dev Home, selecione **Shenmue Launcher**.
2. Aperte o botão **View** (dois quadrados) → *View details* → tipo **Game**.

Alternativa: Device Portal → *Settings* → *Preferences* → *Treat UWP apps as games by default*.

## 4. Usar o launcher
- **Login:** só por QR code, sempre visível à esquerda. No app Steam do celular: Steam Guard → ícone de QR → escaneie e confirme. Só é preciso para baixar ou verificar arquivos.
- **Jogos:** cada um tem o seu cartão, com estado, **JOGAR**, **Baixar** (cada jogo separado), **Verificar arquivos** e **Apagar**.

## Mods
Nenhum mod vem embutido por padrão neste repositório (são de terceiros — ver `docs/MODS.md` para importar). Depois de importados, cada mod liga/desliga no cartão do jogo no launcher, independente dos outros.

## Saves
Os saves e as opções do jogo ficam em `LocalState\cloud` (mesmo formato da Steam Cloud do PC: `sm1saves/SHENMUE1_000`, `sm1saves/Settings`...). Apagar um jogo no launcher não apaga os saves.
Para levar saves do PC: copie o conteúdo de `C:\Program Files (x86)\Steam\userdata\<id>\758330\remote` para `LocalState\cloud` pelo Device Portal (*File explorer*).

## 5. Se algo falhar
Ao voltar, o launcher mostra uma caixa vermelha com o resultado da última execução. Tire uma foto dela.
Log completo: botão **Ver log do jogo (diagnostico)**, ou Device Portal → *File explorer* → *LocalAppData* → `ShenmueUwpLauncher_...` → `LocalState` → `host.log`, `boot.log`, `launcher.log`.

| O que aparece | O que significa |
|---|---|
| "nao chegou a iniciar" | O `ShenmueGame.exe` não carregou (DLL de sistema ausente) |
| "parou antes do jogo carregar" + etapas | Falhou no app nativo, antes de mapear o jogo |
| `D3D11CreateDevice` com erro e "teste nivel" | Sem Direct3D 11: pacote não está como Game |
| `EXCECAO` com `shenmue.exe+0x...` | Falha dentro do jogo; o endereço diz onde |

## Opções de depuração (arquivos vazios em LocalState)
| Arquivo | Efeito |
|---|---|
| `framedump.txt` | salva quadros em `LocalState\frames` |
| `mute.txt` | sem som |
| `watchdog.txt` | watchdog permanente (por padrão, 5 min) |
| `autopad.txt` | controle simulado: linhas `segundos botoesHex duracaoMs` |
| `strict.txt` | bloqueia DLLs do sistema que podem faltar no console (simulação no PC) |

## Testar no PC, sem Xbox
`tools/pc_test.ps1 -Game 1 -Seconds 60 [-Strict]` registra o pacote no sandbox UWP do PC e usa os dados da instalação Steam local. `tools/pc_test_launcher.ps1` clica em JOGAR via UI Automation.
