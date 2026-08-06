from __future__ import annotations

import json
import sys
from pathlib import Path


def clean(text: str) -> str:
    return " ".join(text.replace("\u00a0", " ").split())


def summarize_docx(path: Path, out_path: Path) -> None:
    data = json.loads(path.read_text(encoding="utf-8"))
    lines = [f"SOURCE: {data['path']}", "", "SECTIONS:"]
    for index, sec in enumerate(data["sections"], 1):
        lines.append(f"[{index}] {json.dumps(sec, ensure_ascii=False)}")
    lines.extend(["", "PARAGRAPHS:"])
    for index, para in enumerate(data["paragraphs"], 1):
        text = clean(para["text"])
        if text:
            meta = {k: v for k, v in para.items() if k not in {"text", "runs"} and v is not None}
            run_meta = []
            for run in para["runs"]:
                rm = {k: v for k, v in run.items() if k != "text" and v is not None}
                run_meta.append({"text": clean(run["text"]), **rm})
            lines.append(f"P{index:04d} {text}")
            lines.append(f"  META {json.dumps(meta, ensure_ascii=False)}")
            if run_meta:
                lines.append(f"  RUNS {json.dumps(run_meta, ensure_ascii=False)}")
    lines.extend(["", "TABLES:"])
    for table in data["tables"]:
        lines.append(f"TABLE {table['index']} STYLE={table['style']}")
        for r_idx, row in enumerate(table["rows"], 1):
            cells = []
            for cell in row:
                texts = [clean(p["text"]) for p in cell if clean(p["text"])]
                cells.append(" / ".join(texts))
            lines.append(f"R{r_idx:03d}: " + " || ".join(cells))
    lines.extend(["", "STYLES:"])
    for style in data["styles"]:
        used = {k: v for k, v in style.items() if v is not None}
        lines.append(json.dumps(used, ensure_ascii=False))
    out_path.write_text("\n".join(lines), encoding="utf-8")


def summarize_pdf(path: Path, out_path: Path) -> None:
    data = json.loads(path.read_text(encoding="utf-8"))
    lines = [f"SOURCE: {data['path']}"]
    for page in data["pages"]:
        lines.extend(["", f"===== PAGE {page['page']} =====", page["text"]])
        if page["tables"]:
            lines.append("TABLES:")
            lines.append(json.dumps(page["tables"], ensure_ascii=False, indent=2))
    out_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for name in ("template_converted", "vision", "chassis"):
        summarize_docx(src / f"{name}.json", out / f"{name}.txt")
    summarize_pdf(src / "problem.json", out / "problem.txt")


if __name__ == "__main__":
    main()
