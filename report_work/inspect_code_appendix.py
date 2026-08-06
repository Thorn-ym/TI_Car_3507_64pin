from pathlib import Path
from zipfile import ZipFile
from xml.etree import ElementTree as ET

from docx import Document
from docx.oxml.ns import qn


path = Path("output/设计报告核心代码摘录.docx")
doc = Document(path)

assert len(doc.sections) == 1
section = doc.sections[0]
assert round(section.page_width.cm, 1) == 21.0
assert round(section.page_height.cm, 1) == 29.7

paragraphs = doc.paragraphs
assert len(paragraphs) == 9

titles = paragraphs[0::3]
codes = paragraphs[1::3]
notes = paragraphs[2::3]

assert len(titles) == len(codes) == len(notes) == 3
assert all(p.style.name == "Code Caption Academic" for p in titles)
assert all(p.style.name == "Academic Code" for p in codes)
assert all(p.style.name == "Code Explanation Academic" for p in notes)

for index, paragraph in enumerate(codes, start=1):
    assert paragraph.runs, f"code {index} has no run"
    for run in paragraph.runs:
        rfonts = run._element.get_or_add_rPr().get_or_add_rFonts()
        assert rfonts.get(qn("w:ascii")) == "Consolas"
        assert rfonts.get(qn("w:eastAsia")) == "Consolas"
        assert run.font.size.pt == 10.5
    longest = max(len(line) for line in paragraph.text.splitlines())
    assert longest <= 68, f"code {index} longest line is {longest}"

with ZipFile(path) as archive:
    styles = ET.fromstring(archive.read("word/styles.xml"))
    ns = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
    expected = {
        "AcademicCode": ("Consolas", "21"),
        "CodeCaptionAcademic": ("SimHei", "24"),
        "CodeExplanationAcademic": ("SimSun", "18"),
    }
    for style_id, (font, size) in expected.items():
        style = styles.find(f".//w:style[@w:styleId='{style_id}']", ns)
        assert style is not None, style_id
        rfonts = style.find("w:rPr/w:rFonts", ns)
        sz = style.find("w:rPr/w:sz", ns)
        assert rfonts is not None and rfonts.get(qn("w:ascii")) == font
        assert sz is not None and sz.get(qn("w:val")) == size

print("DOCX structural audit passed")
for index, paragraph in enumerate(codes, start=1):
    lines = paragraph.text.splitlines()
    print(f"code {index}: {len(lines)} lines, max width {max(map(len, lines))}")
