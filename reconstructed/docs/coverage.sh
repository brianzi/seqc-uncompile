#!/usr/bin/env bash
# ============================================================================
# coverage.sh — documentation coverage snapshot
# ============================================================================
# Reads the Doxygen warning log and XML output produced by `make docs` and
# prints a one-line score plus a breakdown of unclear / verifyme / binarynote
# items.
#
# Usage (from anywhere):
#   reconstructed/docs/coverage.sh
#
# Or specify an alternate build dir:
#   reconstructed/docs/coverage.sh /path/to/build
# ============================================================================

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-${repo_root}/reconstructed/build}"
docs_dir="${build_dir}/docs"
warn_log="${docs_dir}/doxygen-warnings.log"
xml_dir="${docs_dir}/xml"

if [[ ! -f "${warn_log}" ]]; then
    echo "error: ${warn_log} not found — run 'cmake --build ${build_dir} --target docs' first" >&2
    exit 1
fi

# Count undocumented warnings.  Doxygen emits messages like
#   <file>:<line>: warning: Member foo() ... is not documented.
# and similar for Compound / class / namespace / parameter.
undoc_total=$(grep -cE 'is not documented|undocumented' "${warn_log}" || true)
undoc_members=$(grep -cE 'Member .* (is|of class|of file|of namespace) .* is not documented' "${warn_log}" || true)
undoc_compounds=$(grep -cE 'Compound .* is not documented' "${warn_log}" || true)
undoc_params=$(grep -cE 'parameter .* is not documented|parameters of member|argument .* of command @param is not found' "${warn_log}" || true)

# Count tag occurrences across the source tree (counts \unclear, \verifyme,
# \binarynote, \unverifiable — not the alias definitions themselves).
src_dirs=("${repo_root}/reconstructed/src" "${repo_root}/reconstructed/include")
unclear=$( { grep -rE '^\s*//!?.*\\unclear\b' "${src_dirs[@]}" 2>/dev/null || true; } | wc -l)
verifyme=$( { grep -rE '^\s*//!?.*\\verifyme\b' "${src_dirs[@]}" 2>/dev/null || true; } | wc -l)
binarynote=$( { grep -rE '^\s*//!?.*\\binarynote\b' "${src_dirs[@]}" 2>/dev/null || true; } | wc -l)
unverifiable=$( { grep -rE '^\s*//!?.*\\unverifiable\b' "${src_dirs[@]}" 2>/dev/null || true; } | wc -l)

# Documented vs. total symbols (rough heuristic from XML index)
documented=0
total=0
if [[ -d "${xml_dir}" ]]; then
    # Each <memberdef> with non-empty <briefdescription> counts as documented.
    if command -v xmlstarlet >/dev/null 2>&1; then
        total=$(xmlstarlet sel -t -v 'count(//memberdef)' "${xml_dir}"/*.xml 2>/dev/null || echo 0)
        documented=$(xmlstarlet sel -t -v 'count(//memberdef[normalize-space(briefdescription)!=""])' "${xml_dir}"/*.xml 2>/dev/null || echo 0)
    else
        # Fallback: grep-based count.  Less precise but no dependencies.
        total=$(grep -hcE '<memberdef ' "${xml_dir}"/*.xml 2>/dev/null | awk '{s+=$1} END {print s+0}')
        documented=$(python3 -c "
import glob, re, sys
total = doc = 0
for path in glob.glob('${xml_dir}/*.xml'):
    with open(path) as f:
        text = f.read()
    for m in re.finditer(r'<memberdef[^>]*>.*?</memberdef>', text, re.S):
        total += 1
        body = m.group(0)
        bd = re.search(r'<briefdescription>(.*?)</briefdescription>', body, re.S)
        if bd and bd.group(1).strip():
            doc += 1
print(doc)
" 2>/dev/null || echo 0)
    fi
fi

pct="—"
if [[ "${total}" -gt 0 ]]; then
    pct=$(awk "BEGIN { printf \"%.1f%%\", 100.0 * ${documented} / ${total} }")
fi

echo "Documentation coverage:"
echo "  Symbols documented   : ${documented}/${total} (${pct})"
echo "  Undocumented warnings: ${undoc_total} total"
echo "    members            : ${undoc_members}"
echo "    compounds          : ${undoc_compounds}"
echo "    parameters         : ${undoc_params}"
echo "Open backlog tags:"
echo "  \\unclear      : ${unclear}"
echo "  \\verifyme     : ${verifyme}"
echo "  \\binarynote   : ${binarynote}"
echo "  \\unverifiable : ${unverifiable}"
