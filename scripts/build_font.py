"""Build an SD-resident 18px Unicode font, preserving the upstream OFL notice."""
from pathlib import Path
import math
import struct
import urllib.request
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

root = Path(__file__).resolve().parents[1]
cache = root / "font-source"
assets = root / "assets"
cache.mkdir(exist_ok=True)
assets.mkdir(exist_ok=True)
font_path = cache / "NotoSansSC.ttf"
upstream = "https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/"
if not font_path.exists():
    urllib.request.urlretrieve(upstream + "NotoSansSC%5Bwght%5D.ttf", font_path)
urllib.request.urlretrieve(upstream + "OFL.txt", assets / "OFL-NotoSansSC.txt")
face = ImageFont.truetype(str(font_path), 18)
cmap = TTFont(font_path).getBestCmap()
points = sorted(c for c in cmap if c >= 32 and c not in (0xFEFF,))
records = []
with (assets / "font18.bin").open("wb") as output:
    output.write(struct.pack("<4sIII", b"VDF1", len(points), 20, 24))
    data_offset = 16 + 12 * len(points)
    for i, cp in enumerate(points):
        advance = max(1, min(20, math.ceil(face.getlength(chr(cp)))))
        output.write(struct.pack("<III", cp, data_offset + 480 * i, advance))
    for i, cp in enumerate(points):
        image = Image.new("L", (20, 24))
        ImageDraw.Draw(image).text((0, 20), chr(cp), font=face, fill=255, anchor="ls")
        output.write(image.tobytes())
        if (i + 1) % 5000 == 0:
            print("glyphs", i + 1, "/", len(points), flush=True)
print("FONT", assets / "font18.bin", "glyphs", len(points), flush=True)
