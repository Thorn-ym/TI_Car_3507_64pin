from __future__ import annotations

import sys
from pathlib import Path

from docx import Document
from docx.enum.text import WD_LINE_SPACING
from docx.oxml.ns import qn
from docx.shared import Pt


src = Path(sys.argv[1])
dst = Path(sys.argv[2])
doc = Document(src)

fixed = []
for index, paragraph in enumerate(doc.paragraphs):
    if not paragraph._p.xpath(".//m:oMath"):
        continue

    ppr = paragraph._p.get_or_add_pPr()
    spacing = ppr.find(qn("w:spacing"))
    if spacing is not None:
        spacing.attrib.pop(qn("w:line"), None)
        spacing.attrib.pop(qn("w:lineRule"), None)

    fmt = paragraph.paragraph_format
    fmt.line_spacing_rule = WD_LINE_SPACING.AT_LEAST
    fmt.line_spacing = Pt(34)
    fmt.space_before = Pt(3)
    fmt.space_after = Pt(3)
    fmt.keep_together = True
    fixed.append(index)

doc.core_properties.author = ""
doc.core_properties.last_modified_by = ""
doc.core_properties.comments = ""
doc.save(dst)
print(f"fixed_formula_paragraphs={fixed}")
