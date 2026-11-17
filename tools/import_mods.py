#!/usr/bin/env python3
"""Prepara os mods que vão embutidos no pacote (stages/05-mods/payload, fora do git).

- Widescreen: lê o .bat do "Shenmue I & II - Widescreen Patcher" (ner0, baseado no trabalho de Esppiral), pega os padrões da
  versão pinada (Shenmue I v1.06/1.07, Shenmue II v1.07), confere contra os exes da Steam e grava patches/widescreen_smN.txt.
- Tradução: copia os arquivos .tad/.tac da "Tradução Shenmue" (Tribo Gamer) para overlay/sm1/archives/dx11/mods.
- Texturas: copia o "Streamer2k Textures - Better Textures for Shenmue" (só Shenmue I) -- os .tac/.tad grandes
  (archives/dx11/data, instalados de verdade na pasta do jogo em tempo de execução) para data/sm1/archives/dx11/data,
  e a sobreposição de modelos/cenas (mods/...) para overlay_tex/sm1/archives/dx11/mods.

Uso: python tools/import_mods.py [--widescreen PASTA] [--traducao PASTA] [--texturas PASTA] [--steam PASTA_SMLAUNCH]
"""
import argparse, glob, os, re, shutil, sys
import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAYLOAD = os.path.join(ROOT, "stages", "05-mods", "payload")


def find_texturas_default():
    matches = sorted(glob.glob(os.path.join(ROOT, "mods-src", "Better Textures*")))
    return matches[0] if matches else os.path.join(ROOT, "mods-src", "Better Textures for Shenmue")

# Conjuntos do .bat para a build 3291213 (depot pinado): tag -> sufixo das variáveis.
SETS = {
    1: [("main", "sm1_106-107")],
    2: [("main", "sm2_107"), ("fvi", "fvi_sm2_107"), ("vsp", "vsp_sm2_106-107"), ("nosnapcamico", "nosnapcamico_sm2_106-107")],
}


def parse_bat(path):
    bat = open(path, encoding="latin-1").read()
    pairs = {}
    for m in re.finditer(r"^set (origiPattern|patchPattern)(\d+)_([\w-]+?)=(.*)$", bat, re.M):
        kind, idx, suffix, val = m.groups()
        data = bytes(int(x, 16) for x in re.findall(r":x([0-9A-Fa-f]{2})", val))
        pairs.setdefault(suffix, {}).setdefault(int(idx), {})[kind] = data
    return bat, pairs


def text_ranges(exe_path):
    pe = pefile.PE(exe_path, fast_load=True)
    out = []
    for s in pe.sections:
        if s.Characteristics & 0x20000000:  # executável
            out.append((s.PointerToRawData, s.PointerToRawData + s.SizeOfRawData))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--widescreen", default=os.path.join(ROOT, "mods-src", "Shenmue I & II - Widescreen Patcher"))
    ap.add_argument("--traducao", default=os.path.join(ROOT, "mods-src", "Tradução Shenmue"))
    ap.add_argument("--texturas", default=find_texturas_default())
    ap.add_argument("--steam", default=r"C:\Program Files (x86)\Steam\steamapps\common\SMLaunch")
    a = ap.parse_args()
    ok = True

    bat_path = os.path.join(a.widescreen, "DRAGnDROP Shenmue EXE HERE.bat")
    if os.path.exists(bat_path):
        bat, pairs = parse_bat(bat_path)
        ver = re.search(r"^set version=(.*)$", bat, re.M).group(1).strip()
        os.makedirs(os.path.join(PAYLOAD, "patches"), exist_ok=True)
        for game, sets in SETS.items():
            exe = os.path.join(a.steam, "sm1" if game == 1 else "sm2", "Shenmue.exe" if game == 1 else "Shenmue2.exe")
            data = open(exe, "rb").read() if os.path.exists(exe) else None
            ranges = text_ranges(exe) if data else []
            lines = [f"# Shenmue I & II - Widescreen Patcher v{ver} (ner0; base: Esppiral). Gerado por tools/import_mods.py.",
                     "# tag original substituto (hex). O runtime aplica na memória, com ocorrência única na seção de código."]
            for tag, suffix in sets:
                for idx, p in sorted(pairs.get(suffix, {}).items()):
                    orig, rep = p["origiPattern"], p["patchPattern"]
                    if len(orig) != len(rep):
                        print(f"ERRO: {suffix} #{idx}: tamanhos diferentes"); ok = False; continue
                    if data is not None:
                        hits = [m.start() for m in re.finditer(re.escape(orig), data)]
                        in_text = [h for h in hits if any(s <= h < e for s, e in ranges)]
                        status = "ok" if len(in_text) == 1 else f"ATENÇÃO: {len(in_text)} ocorrências no código"
                        if len(in_text) != 1: ok = False
                        print(f"  Shenmue {game} {tag:13} #{idx}: {len(orig):2} bytes, {status}")
                    lines.append(f"{tag} {orig.hex()} {rep.hex()}")
            open(os.path.join(PAYLOAD, "patches", f"widescreen_sm{game}.txt"), "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
        print("widescreen: patches gravados")
    else:
        print("widescreen: patcher não encontrado em", a.widescreen); ok = False

    src = os.path.join(a.traducao, "sm1", "archives", "dx11", "mods")
    dst = os.path.join(PAYLOAD, "overlay", "sm1", "archives", "dx11", "mods")
    if os.path.isdir(src):
        os.makedirs(dst, exist_ok=True)
        for f in os.listdir(src):
            s, d = os.path.join(src, f), os.path.join(dst, f)
            if not os.path.exists(d) or os.path.getsize(d) != os.path.getsize(s):
                shutil.copy2(s, d)
            print(f"tradução: {f} ({os.path.getsize(d) / 1e6:.0f} MB)")
    else:
        print("tradução: arquivos não encontrados em", src); ok = False

    # Texturas (só Shenmue I): arquivos "de verdade" em data/sm1/archives/dx11/data (instalados na
    # pasta do jogo em tempo de execução) + sobreposição de modelos/cenas em overlay_tex/sm1/....
    tex_data_src = os.path.join(a.texturas, "archives", "dx11", "data")
    tex_data_dst = os.path.join(PAYLOAD, "data", "sm1", "archives", "dx11", "data")
    tex_mods_src = os.path.join(a.texturas, "mods")
    tex_mods_dst = os.path.join(PAYLOAD, "overlay_tex", "sm1", "archives", "dx11", "mods")
    if os.path.isdir(tex_data_src) and os.path.isdir(tex_mods_src):
        os.makedirs(tex_data_dst, exist_ok=True)
        total = 0
        for f in sorted(os.listdir(tex_data_src)):
            s, d = os.path.join(tex_data_src, f), os.path.join(tex_data_dst, f)
            if not os.path.exists(d) or os.path.getsize(d) != os.path.getsize(s):
                shutil.copy2(s, d)
            total += os.path.getsize(d)
            print(f"texturas: {f} ({os.path.getsize(d) / 1e9:.2f} GB)")
        n = 0
        for dirpath, _dirs, files in os.walk(tex_mods_src):
            rel = os.path.relpath(dirpath, tex_mods_src)
            outdir = os.path.join(tex_mods_dst, rel) if rel != "." else tex_mods_dst
            os.makedirs(outdir, exist_ok=True)
            for f in files:
                s, d = os.path.join(dirpath, f), os.path.join(outdir, f)
                if not os.path.exists(d) or os.path.getsize(d) != os.path.getsize(s):
                    shutil.copy2(s, d)
                n += 1
        print(f"texturas: {total / 1e9:.1f} GB em archives/dx11/data; {n} arquivo(s) de sobreposição")
    else:
        print("texturas: pack não encontrado em", a.texturas); ok = False

    open(os.path.join(PAYLOAD, "CREDITS.txt"), "w", encoding="utf-8", newline="\n").write(
        "Widescreen: Shenmue I & II - Widescreen Patcher, por ner0 (Shenmue Dojo), baseado no trabalho de Esppiral.\n"
        "Legendas em português do Shenmue: Tribo Gamer (tribogamer.com).\n"
        "Texturas em alta definição (Shenmue I): Streamer2k Textures - Better Textures for Shenmue, por Streamer2k.\n"
        "Mods de terceiros: uso pessoal. Não redistribua sem autorização dos autores.\n")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
