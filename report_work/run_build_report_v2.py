from __future__ import annotations

import sys

from docx import Document
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.oxml.settings import CT_Settings
from docx.styles.styles import Styles


original_getitem = Styles.__getitem__


def style_alias(self, key):
    if key == "Table Grid":
        key = "Normal Table"
    return original_getitem(self, key)


def get_or_add_update_fields(self):
    element = self.find(qn("w:updateFields"))
    if element is None:
        element = OxmlElement("w:updateFields")
        self.append(element)
    return element


Styles.__getitem__ = style_alias
CT_Settings.get_or_add_updateFields = get_or_add_update_fields

import build_report


def set_table_borders(table, color="000000", size="6"):
    tbl_pr = table._tbl.tblPr
    borders = tbl_pr.find(qn("w:tblBorders"))
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        tbl_pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        node = borders.find(qn(f"w:{edge}"))
        if node is None:
            node = OxmlElement(f"w:{edge}")
            borders.append(node)
        node.set(qn("w:val"), "single")
        node.set(qn("w:sz"), size)
        node.set(qn("w:space"), "0")
        node.set(qn("w:color"), color)


build_report.build()

output = Document(sys.argv[2])
for table in output.tables:
    set_table_borders(table)
output.save(sys.argv[2])
