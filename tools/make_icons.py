"""Gera os ícones do launcher (Assets/*.png) com Pillow: kanji 莎木 (Shenmue) em vermelho sobre fundo escuro."""
import os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = os.path.join(os.path.dirname(__file__), "..", "stages", "01-launcher", "src", "Assets")
RED, DARK = (200, 55, 45), (14, 14, 18)


def square(n=1024):
    img = Image.new("RGBA", (n, n), DARK)
    d = ImageDraw.Draw(img)
    for i in range(n):  # gradiente vertical sutil
        c = int(14 + 22 * i / n)
        d.line([(0, i), (n, i)], fill=(c + 10, c - 4, c - 2, 255))
    d.ellipse([n * .08, n * .08, n * .92, n * .92], outline=RED, width=int(n * .035))
    f = ImageFont.truetype(r"C:\Windows\Fonts\msyhbd.ttc", int(n * .40))
    txt = "莎木"
    w = d.textlength(txt, font=f)
    glow = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.text(((n - w) / 2, n * .30), txt, font=f, fill=RED + (255,))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(n * .012)))
    d.text(((n - w) / 2, n * .30), txt, font=f, fill=(245, 240, 235, 255))
    return img


def wide(w, h):
    base = Image.new("RGBA", (w, h), DARK)
    sq = square(1024).resize((int(h * .9), int(h * .9)), Image.LANCZOS)
    base.alpha_composite(sq, (int(h * .05), int(h * .05)))
    d = ImageDraw.Draw(base)
    f = ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", int(h * .20))
    d.text((int(h * 1.05), int(h * .28)), "SHENMUE", font=f, fill=(245, 240, 235, 255))
    d.rectangle([int(h * 1.05), int(h * .56), int(h * 1.05) + int(h * .9), int(h * .59)], fill=RED)
    f2 = ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", int(h * .11))
    d.text((int(h * 1.05), int(h * .64)), "I & II  ·  UWP", font=f2, fill=(190, 190, 195, 255))
    return base


os.makedirs(OUT, exist_ok=True)
sq = square(1024)
for name, px in [("StoreLogo", 50), ("Square44x44Logo", 44), ("Square150x150Logo", 150), ("Square310x310Logo", 310)]:
    sq.resize((px, px), Image.LANCZOS).save(os.path.join(OUT, name + ".png"))
wide(310, 150).save(os.path.join(OUT, "Wide310x150Logo.png"))
wide(620, 300).save(os.path.join(OUT, "SplashScreen.png"))
print("ok")
