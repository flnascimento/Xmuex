# Como importar os mods (não incluídos neste repositório)

Os mods abaixo são de terceiros e **não estão no repositório** por direitos autorais. O sistema que os aplica (launcher, patches em memória, pasta virtual) já vem pronto; falta só colocar os arquivos originais e rodar um script antes de montar o pacote.

## 1. Baixe os mods originais (cada um na fonte do autor)

| Mod | Autor | Jogo | Onde baixar |
|---|---|---|---|
| Shenmue I & II - Widescreen Patcher v2.1.8 | ner0 (base: Esppiral) | I e II | Shenmue Dojo / comunidade |
| Tradução Shenmue (legendas PT-BR) | Tribo Gamer | I | tribogamer.com |
| Better Textures for Shenmue v4.1 | Streamer2k | I | Nexus Mods |

Todos são opcionais e independentes entre si — pode importar só os que quiser.

## 2. Coloque as pastas em `mods-src/` (na raiz do projeto, fora do git)

```
mods-src/
  Shenmue I & II - Widescreen Patcher/      (contém "DRAGnDROP Shenmue EXE HERE.bat")
  Tradução Shenmue/                          (contém sm1/archives/dx11/mods/...)
  Better Textures for Shenmue v4.1.../       (qualquer pasta começando com "Better Textures"; contém archives/dx11/data/ e mods/)
```

Crie a pasta `mods-src/` se não existir. Ela já está no `.gitignore`.

## 3. Rode o script de importação

```
python tools/import_mods.py
```

Isso gera `stages/05-mods/payload/` (também fora do git) com os patches e arquivos prontos para entrar no pacote. O script avisa no terminal se algum mod não foi encontrado ou se os padrões de bytes do widescreen não baterem (ex.: versão errada do jogo).

Para apontar pastas em outro lugar: `python tools/import_mods.py --widescreen CAMINHO --traducao CAMINHO --texturas CAMINHO`.

## 4. Gere o pacote normalmente

```
tools/gen_cert.ps1   (só na primeira vez)
tools/build.ps1
```

`tools/build.ps1` inclui `stages/05-mods/payload/` dentro do `.msix` automaticamente, se ela existir. Sem os mods importados, o pacote sai igual, só sem as opções de mod aparecerem no launcher (ele verifica se os arquivos existem antes de mostrar a caixa de seleção).

## Detalhes técnicos

Ver `stages/05-mods/README.md` (como cada mod é aplicado: patch em memória, pasta virtual sobreposta, cópia real de arquivos).
