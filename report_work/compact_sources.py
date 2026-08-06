from __future__ import annotations

import json
import sys
from pathlib import Path


def normalize(text: str) -> str:
    return " ".join(text.replace("\u00a0", " ").split())


def main() -> None:
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for name in ("vision", "chassis"):
        data = json.loads((src / f"{name}.json").read_text(encoding="utf-8"))
        lines = []
        for para in data["paragraphs"]:
            text = normalize(para["text"])
            if text:
                lines.append(text)
        for table in data["tables"]:
            lines.append(f"[表{table['index']}]")
            for row in table["rows"]:
                cells = []
                for cell in row:
                    cells.append(" / ".join(normalize(p["text"]) for p in cell if normalize(p["text"])))
                lines.append(" | ".join(cells))
        (out / f"{name}.txt").write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
