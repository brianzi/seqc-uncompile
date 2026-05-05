#!/usr/bin/env python3
"""
Comment cleanup — Phase 2 (fixed: no code-indentation collapsing).

Strips workflow phase references from comments only.
Does NOT touch code indentation or non-comment lines.
"""

import re
from pathlib import Path

SRC_ROOTS = [
    Path('/home/brian/zhinst/seqc_compiler/reconstructed/src'),
    Path('/home/brian/zhinst/seqc_compiler/reconstructed/include/zhinst'),
]
EXTENSIONS = {'.cpp', '.hpp', '.h', '.c', '.inc'}

def strip_phase_from_comment_line(line: str) -> str:
    """Process a single line, stripping workflow Phase refs only from comment portions."""
    # Only process lines that contain //
    if '//' not in line:
        return line

    # Find the comment start
    comment_start = line.index('//')
    code_part = line[:comment_start]
    comment_part = line[comment_start:]

    # Apply transformations to comment_part only
    original_comment = comment_part

    # 1. "// file.cpp — Phase NNx (desc)." banner suffix
    comment_part = re.sub(
        r'(// \S+\.(?:cpp|hpp|c|h|inc))\s*[—–]\s*Phase\s+[\w\-\.]+(?:\s+\([^)]*\))?\.?\s*$',
        r'\1',
        comment_part
    )

    # 2. "(Phase NNx-followup, Finding N)" -> ""
    comment_part = re.sub(r'\s*\([Pp]hase\s+[\w\-\.]+,\s*Finding\s+\d+\)', '', comment_part)

    # 3. "(Phase NNx)" standalone parens -> ""
    comment_part = re.sub(r'\s*\([Pp]hase\s+[\w\-\.]+\)', '', comment_part)

    # 4. "— Phase NNx" trailing on any comment line (not algorithm section headers)
    #    Only strip if NOT preceded by "--- N." or "=== N:" pattern (those are kept)
    if not re.search(r'//\s*[-=]{3}', comment_part):
        comment_part = re.sub(r'\s*[—–]\s*[Pp]hase\s+[\w\-\.]+\.?\s*$', '', comment_part)

    # 5. "during Phase NNx" -> ""
    comment_part = re.sub(r'\s+during\s+[Pp]hase\s+[\w\-\.]+', '', comment_part)

    # 6. "in Phase NNx" -> "" (only trailing, not "Phase N:" section refs)
    comment_part = re.sub(r'\s+in [Pp]hase\s+[\w\-\.]+(?=\s*[;,.)]\s*$|\s*$)', '', comment_part)

    # 7. "after Phase NNx" -> ""
    comment_part = re.sub(r'\s+after\s+[Pp]hase\s+[\w\-\.]+(?=\s*[;,.)]\s*$|\s*$)', '', comment_part)

    # 8. "confirmed Phase NNx" -> "confirmed"
    comment_part = re.sub(r'\bconfirmed\s+[Pp]hase\s+[\w\-\.]+', 'confirmed', comment_part)

    # 9. "renamed Phase NNx" -> "renamed"
    comment_part = re.sub(r'\brenamed\s+[Pp]hase\s+[\w\-\.]+', 'renamed', comment_part)

    # 10. "As of Phase NNx this" -> "This"
    comment_part = re.sub(r'\bAs of [Pp]hase\s+[\w\-\.]+\s+', '', comment_part)

    # 11. "Layout revised in Phase NNx after" -> "Layout revised after"
    comment_part = re.sub(r'\bLayout revised in [Pp]hase\s+[\w\-\.]+\s+', 'Layout revised ', comment_part)

    # 12. "Full layout recovered Phase NNx from" -> "Full layout recovered from"
    comment_part = re.sub(r'\bFull layout recovered [Pp]hase\s+[\w\-\.]+\s+', 'Full layout recovered ', comment_part)

    # 13. "// Layout (Phase NNx, date):" -> "// Binary layout (date):"
    comment_part = re.sub(r'(// Layout)\s+\([Pp]hase\s+[\w\-\.]+,\s*', r'\1 (', comment_part)
    comment_part = re.sub(r'(// Layout)\s+\([Pp]hase\s+[\w\-\.]+\)\s*:', r'// Binary layout:', comment_part)

    # 14. "CORRECTION YYYY-MM-DD (Phase NNx, desc):" line -> remove entire line
    if re.match(r'[ \t]*//\s*CORRECTION\s+\d{4}-\d{2}-\d{2}\s+\([^)]*\)\s*:', comment_part):
        return ''  # signal to remove line

    # 15. "FINAL LAYOUT (Phase NNx ...):" -> "Binary layout:"
    comment_part = re.sub(r'FINAL LAYOUT\s+\([^)]*\)\s*:', 'Binary layout:', comment_part)

    # 16. "corrected in Phase NNx ... after" -> "corrected after"
    comment_part = re.sub(r'\bcorrected in [Pp]hase\s+[\w\-\.\s]+after\b', 'corrected after', comment_part)

    # 17. "Field rename completed in Phase NNx " -> "Field rename completed: "
    comment_part = re.sub(r'\bField rename completed in [Pp]hase\s+[\w\-\.]+\s+', 'Field rename completed: ', comment_part)

    # 18. "Phase NNx renumbering: ..." -> remove whole line
    if re.match(r'[ \t]*//\s*[Pp]hase\s+[\w\-\.]+\s+renumbering\s*:', comment_part):
        return ''

    # 19. "After the Phase NNx renumbering" -> "These aliases"
    comment_part = re.sub(r'\bAfter the [Pp]hase\s+[\w\-\.]+\s+renumbering\s+', 'These aliases ', comment_part)

    # 20. "Promoted from X (Phase NNx/NNy)." -> "Promoted from X."
    comment_part = re.sub(r'\s*\([Pp]hase\s+[\w\-\./]+\)\.?', '', comment_part)

    # 21. "deferred to Phase NNx" -> "pending"
    comment_part = re.sub(r'\bdeferred to [Pp]hase\s+[\w\-\.]+', 'pending', comment_part)

    # 22. "(NEW — Phase NNx)" -> "(NEW)"
    comment_part = re.sub(r'\s*[—–]\s*[Pp]hase\s+[\w\-\.]+\s*\)', ')', comment_part)

    # 23. "Phase NNx: was/is..." -> strip prefix (only non-section-header lines)
    if not re.search(r'//\s*[-=]{3}', comment_part):
        comment_part = re.sub(r'\b[Pp]hase\s+[\w\-\.]+:\s*(?!--)', '', comment_part)

    # 24. "Phase NNx-followup resolved: body implemented below."
    if re.match(r'[ \t]*//\s*[Pp]hase\s+[\w\-\.]+\s+resolved.*below\.?\s*$', comment_part):
        return ''

    # 25. Standalone "// Phase NNx." line
    if re.match(r'[ \t]*//\s*[Pp]hase\s+[\w\-\.]+\.\s*$', comment_part):
        return ''

    # 26. "Phase NNx addition/additions." trailing clause
    comment_part = re.sub(r'\s*[—–]\s*[Pp]hase\s+[\w\-\.]+\s+addition[s]?\.?$', '.', comment_part)

    # 27. "Phase NNx-prereq reconstruction (in progress)." standalone
    if re.match(r'[ \t]*//\s*[Pp]hase\s+[\w\-\.]+\s+reconstruction\s+\(', comment_part):
        return ''

    # 28. "Under the corrected X (Phase NNx-followup," -> strip the paren
    comment_part = re.sub(r'\([Pp]hase\s+[\w\-\./]+,\s*', '(', comment_part)
    comment_part = re.sub(r'\([Pp]hase\s+[\w\-\./]+\)', '', comment_part)

    # 29. "(Resolved Phase NNx:...)" -> ""
    comment_part = re.sub(r'\(Resolved [Pp]hase\s+[\w\-\.]+:[^)]*\)', '', comment_part)

    # 30. "Phase NNx investigation" in parenthetical -> ""
    comment_part = re.sub(r'\([Pp]hase\s+[\w\-\.]+\s+investigation\)', '', comment_part)

    # 31. "Phase NNx; see X" -> "see X"
    comment_part = re.sub(r'[Pp]hase\s+[\w\-\.]+;\s+see\s+', 'see ', comment_part)

    # 32. "After the cleanup that" fix
    comment_part = re.sub(r'After the [Pp]hase\s+[\w\-\.\s/]+ wrap-up cleanup that', 'After the cleanup that', comment_part)

    # 33. " Batch NNx wrap-up cleanup:" -> "Cleanup:"
    comment_part = re.sub(r'[Pp]hase\s+[\w\-\.]+ Batch\s+[\w]+\s+wrap-up cleanup\s*:', 'Cleanup:', comment_part)

    return code_part + comment_part


def normalize_section_headers(content: str) -> str:
    """Convert === Phase N: desc === and ---- Phase N: desc ---- to canonical --- N. desc ---"""
    def repl(m):
        indent = m.group(1)
        num = m.group(2)
        desc = m.group(3).strip().rstrip('=-').strip()
        addr = (m.group(4) or '').strip()
        addr_str = ('  ' + addr) if addr else ''
        return f'{indent}// --- {num}. {desc} ---{addr_str}'

    # === Phase N: desc === @addr
    content = re.sub(
        r'^([ \t]*)//\s*={3,}\s*[Pp]hase\s+(\d+\w*)\s*:\s*([^=@\n]+?)\s*={0,4}\s*(@[^\n]*)?$',
        repl,
        content,
        flags=re.MULTILINE
    )
    # ---- Phase N: desc ---- @addr
    content = re.sub(
        r'^([ \t]*)//\s*-{3,}\s*[Pp]hase\s+(\d+\w*)\s*:\s*([^-@\n]+?)\s*-{0,4}\s*(@[^\n]*)?$',
        repl,
        content,
        flags=re.MULTILINE
    )
    # -- Phase N: desc -- @addr  (two-dash variant)
    content = re.sub(
        r'^([ \t]*)//\s*-{2}\s*[Pp]hase\s+(\d+\w*)\s*:\s*([^-@\n]+?)\s*-{0,4}\s*(@[^\n]*)?$',
        repl,
        content,
        flags=re.MULTILINE
    )
    return content


def normalize_if_refs(content: str) -> str:
    """Normalize IF-NNN cross-references."""
    # "see IF-NNN" -> "NOTE: IF-NNN"
    content = re.sub(r'\bsee\s+(IF-\d+)\b\.?', r'NOTE: \1', content)
    # bare "IF-NNN for ..." not already prefixed -> "NOTE: IF-NNN —"
    content = re.sub(r'(?<![A-Z]: )(IF-\d+)\s+for\b', r'NOTE: \1 —', content)
    return content


def process_file(path: Path) -> bool:
    content = path.read_text()
    original = content

    # Step 1: normalize section headers (operates on full content — safe, only touches comments)
    content = normalize_section_headers(content)

    # Step 2: IF-NNN normalization
    content = normalize_if_refs(content)

    # Step 3: per-line phase stripping
    lines = content.splitlines(keepends=True)
    new_lines = []
    for line in lines:
        result = strip_phase_from_comment_line(line.rstrip('\n'))
        if result == '':
            # Line should be removed — skip
            pass
        else:
            # Preserve original line ending
            ending = '\n' if line.endswith('\n') else ''
            new_lines.append(result + ending)
    content = ''.join(new_lines)

    if content != original:
        path.write_text(content)
        return True
    return False


total = 0
for root in SRC_ROOTS:
    for path in root.rglob('*'):
        if path.suffix in EXTENSIONS and path.is_file():
            if process_file(path):
                total += 1

print(f'Modified {total} files')
