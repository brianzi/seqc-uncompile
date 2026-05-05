#!/usr/bin/env python3
"""
Comment cleanup script — Phase 2 of the comment style guide implementation.

Pass 1: File-level banner cleanup
  - Strip "— Phase NN..." suffix from single-line file header comments
    e.g. "// device_type.cpp — Phase 14b-i + 14b-ii-a"
    becomes "// device_type.cpp"
  - Strip phase-id-only lines from inside the ==== banner block
  - Strip sub-phase audit-trail references from prose in function-level
    comments that are not algorithm section headers
    e.g. "Disasm-derived (Phase 20d):" -> remove the parenthetical
    e.g. "Split from X during Phase 22b." -> "Split from X."
    e.g. "confirmed Phase 13c" -> "confirmed"
    e.g. "Phase 20b addition." -> (remove whole line if that's all it says)
    e.g. "Phase 20c added ..." -> strip the "Phase 20c" prefix

Pass 2: Normalize === Phase N: === section dividers to canonical form
  // === Phase N: description === @0xADDR
  ->
  // --- N. description --- @0xADDR
  (matches the canonical "// --- N. Description ---" form from style guide §9)

Pass 3: IF-NNN tag normalization
  - "see IF-NNN" -> "NOTE: IF-NNN"
  - "IF-NNN for..." -> "NOTE: IF-NNN —..."
  Where the IF reference is NOT already prefixed with NOTE: or TODO:

Note: in-function "// Phase N:" section headers that ARE algorithm outlines
are NOT touched — they will be standardized manually in a follow-on pass
since they require judgment about which are algorithmic vs workflow.
"""

import re
import sys
from pathlib import Path

SRC_ROOTS = [
    Path('/home/brian/zhinst/seqc_compiler/reconstructed/src'),
    Path('/home/brian/zhinst/seqc_compiler/reconstructed/include/zhinst'),
]

EXTENSIONS = {'.cpp', '.hpp', '.h', '.c', '.inc'}

files_changed = 0
lines_changed = 0

def process_file(path: Path) -> bool:
    content = path.read_text()
    original = content

    # -------------------------------------------------------------------------
    # Pass 1a: Strip "— Phase NN..." from file-header comment lines
    # Matches: // something.cpp — Phase 14b-i + 14b-ii-a
    # Keeps:   // something.cpp
    # Also:    // seqc_ast_nodes_evaluate.cpp — Phase 21h
    content = re.sub(
        r'(// \S+\.(?:cpp|hpp|c|h|inc))\s+[—–-]+\s+[Pp]hase\s+[\w\-+\s]+',
        r'\1',
        content
    )

    # Pass 1b: Strip "(Phase NN...)" parenthetical audit refs from prose comments
    # e.g. "(Phase 20d)" / "(Phase 20e-ii)" / "(Phase 13c)"
    content = re.sub(r'\s*\([Pp]hase\s+[\w\-]+\)', '', content)

    # Pass 1c: Strip ", Phase NN..." trailing refs
    content = re.sub(r',\s*[Pp]hase\s+[\w\-]+', '', content)

    # Pass 1d: Remove standalone "Phase NN addition." / "Phase NN added X" lines
    # Only where the entire comment line reduces to the phase reference
    content = re.sub(
        r'^([ \t]*)//\s*[Pp]hase\s+[\w\-]+\s+(?:addition|added|was added)[.;]?\s*\n',
        '',
        content,
        flags=re.MULTILINE
    )

    # Pass 1e: "Split from X.cpp during Phase NN." → "Split from X.cpp."
    content = re.sub(r'\s+during\s+[Pp]hase\s+[\w\-]+', '', content)

    # Pass 1f: "confirmed Phase NN" → "confirmed"
    content = re.sub(r'\bconfirmed\s+[Pp]hase\s+[\w\-]+', 'confirmed', content)

    # Pass 1g: "Disasm-derived (Phase NN):" at start of comment line
    content = re.sub(
        r'^([ \t]*)//\s*Disasm-derived\s*(?:\([Pp]hase[\w\-\s]+\))?\s*[:.]\s*\n',
        '',
        content,
        flags=re.MULTILINE
    )
    # Trailing colon form kept but phase stripped (already done by 1b above)

    # Pass 1h: Remove lines that became empty comments after stripping
    content = re.sub(r'^[ \t]*//\s*\n', '//\n', content, flags=re.MULTILINE)

    # -------------------------------------------------------------------------
    # Pass 2: Normalize === Phase N: description === section dividers
    # Pattern:  // === Phase N: description ===  @0xADDR
    # Target:   // --- N. description ---  @0xADDR
    def normalize_phase_header(m):
        indent = m.group(1)
        num = m.group(2)
        desc = m.group(3).strip().rstrip('=').strip()
        addr = m.group(4) or ''
        addr_str = ('  ' + addr.strip()) if addr.strip() else ''
        return f'{indent}// --- {num}. {desc} ---{addr_str}'

    content = re.sub(
        r'^([ \t]*)//\s*={3,}\s*[Pp]hase\s+(\d+\w*)\s*:\s*([^=@\n]+?)\s*={0,3}\s*(@[^\n]*)?$',
        normalize_phase_header,
        content,
        flags=re.MULTILINE
    )

    # -------------------------------------------------------------------------
    # Pass 3: Normalize IF-NNN cross-references
    # "see IF-NNN" / "see IF-NNN." → "NOTE: IF-NNN"
    content = re.sub(
        r'\bsee\s+(IF-\d+)\b\.?',
        r'NOTE: \1',
        content
    )
    # "IF-NNN for ..." not already prefixed → "NOTE: IF-NNN —"
    content = re.sub(
        r'(?<![A-Z]: )(IF-\d+)\s+for\b',
        r'NOTE: \1 —',
        content
    )

    if content != original:
        path.write_text(content)
        return True
    return False

total_files = 0
for root in SRC_ROOTS:
    for path in root.rglob('*'):
        if path.suffix in EXTENSIONS and path.is_file():
            if process_file(path):
                total_files += 1

print(f'Modified {total_files} files')
