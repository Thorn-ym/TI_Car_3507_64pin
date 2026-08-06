from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image


src = Path(sys.argv[1])
dst = Path(sys.argv[2])
page = Image.open(src).convert("RGB")
gray = page.convert("L")
threshold = 235
rows = []
for y in range(page.height):
    count = sum(1 for x in range(page.width) if gray.getpixel((x, y)) < threshold)
    if count > 12:
        rows.append(y)

groups = []
if rows:
    start = prev = rows[0]
    for y in rows[1:]:
        if y - prev > 20:
            groups.append((start, prev))
            start = y
        prev = y
    groups.append((start, prev))

formula_groups = [(a, b) for a, b in groups if b - a >= 18]
if not formula_groups:
    raise SystemExit("no content groups")

top = max(0, formula_groups[0][0] - 40)
bottom = min(page.height, formula_groups[-1][1] + 40)
crop = page.crop((120, top, page.width - 90, bottom))
crop.save(dst)
print(groups)
