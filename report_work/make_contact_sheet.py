from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw


src = Path(sys.argv[1])
dst = Path(sys.argv[2])
files = sorted(src.glob("page-*.png"))
thumb_w = 360
gap = 24
label_h = 28
thumbs = []
for file in files:
    im = Image.open(file).convert("RGB")
    h = round(im.height * thumb_w / im.width)
    im = im.resize((thumb_w, h))
    thumbs.append((file.name, im))

cols = 3
rows = (len(thumbs) + cols - 1) // cols
cell_h = max(im.height for _, im in thumbs) + label_h
sheet = Image.new("RGB", (cols * thumb_w + (cols + 1) * gap, rows * cell_h + (rows + 1) * gap), "#d7d7d7")
draw = ImageDraw.Draw(sheet)
for index, (name, im) in enumerate(thumbs):
    row, col = divmod(index, cols)
    x = gap + col * (thumb_w + gap)
    y = gap + row * (cell_h + gap)
    sheet.paste(im, (x, y + label_h))
    draw.text((x, y + 4), name, fill="black")
dst.parent.mkdir(parents=True, exist_ok=True)
sheet.save(dst)
