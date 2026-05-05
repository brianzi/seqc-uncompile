#!/usr/bin/env python3
"""
Pretty-print test manifest with colors and formatting.

Shows tests organized by group with metadata (tags, comments, source size).
"""

import argparse
import sys
from pathlib import Path
from collections import defaultdict
from typing import List, Optional

# Terminal colors
RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
BLUE = "\033[34m"
MAGENTA = "\033[35m"
CYAN = "\033[36m"
WHITE = "\033[37m"

# Semantic colors
GROUP_COLOR = CYAN + BOLD
NAME_COLOR = WHITE
BRACKET_COLOR = DIM
DEVICE_COLOR = YELLOW
TAG_COLOR = BLUE
COMMENT_COLOR = DIM
HEADER_COLOR = BOLD + WHITE


def colorize_test_name(name: str, dim_prefix: bool = False) -> tuple:
    """
    Colorize hierarchical test name: group:subgroup:name[device, args]
    
    Returns: (colored_name_part, colored_params_part, raw_name_without_params, raw_params_with_brackets)
    """
    if '[' not in name:
        colored = f"{NAME_COLOR}{name}{RESET}"
        return (colored, "", name, "")
    
    # Split name and bracket parts
    parts = name.split('[', 1)
    hierarchy = parts[0]
    params_with_brackets = '[' + parts[1]
    
    # Colorize hierarchy: group:subgroup:testname
    hierarchy_parts = hierarchy.split(':')
    if len(hierarchy_parts) > 1:
        # Dim all but last part (group prefix), last part is GREEN
        colored_hierarchy = f"{DIM}{':'.join(hierarchy_parts[:-1])}:{RESET}{GREEN}{hierarchy_parts[-1]}{RESET}"
    else:
        colored_hierarchy = f"{GREEN}{hierarchy_parts[0]}{RESET}"
    
    # Colorize bracket: [device, args] - both brackets dimmed, content yellow
    bracket_content = parts[1].rstrip(']')
    colored_bracket = f"{DIM}[{RESET}{YELLOW}{bracket_content}{RESET}{DIM}]{RESET}"
    
    return (colored_hierarchy, colored_bracket, hierarchy, params_with_brackets)


def count_seqc_lines(cases_dir: Path, test_dict: dict) -> Optional[int]:
    """Count lines in SeqC source (file or inline code)."""
    if 'code' in test_dict:
        return len(test_dict['code'].strip().split('\n'))
    elif 'file' in test_dict:
        seqc_path = cases_dir / test_dict['file']
        if seqc_path.exists():
            with open(seqc_path) as f:
                return len(f.readlines())
    return None


def format_tags(tags: List[str]) -> str:
    """Format tag list with color."""
    if not tags:
        return f"{DIM}—{RESET}"
    # Show first 3 tags, rest as count
    visible = tags[:3]
    rest = len(tags) - 3
    formatted = f"{TAG_COLOR}{', '.join(visible)}{RESET}"
    if rest > 0:
        formatted += f"{DIM} +{rest}{RESET}"
    return formatted


def format_comment(comment: Optional[str], max_len: int = 50) -> str:
    """Format comment with truncation."""
    if not comment:
        return f"{DIM}—{RESET}"
    if len(comment) <= max_len:
        return f"{COMMENT_COLOR}{comment}{RESET}"
    return f"{COMMENT_COLOR}{comment[:max_len-3]}...{RESET}"


def show_manifest(args):
    """Display manifest in tabular format."""
    from manifest_loader import load_manifest
    
    # Load tests
    manifest_path = args.cases_dir / "manifest.json"
    tests = load_manifest(manifest_path)
    
    # Apply filtering (reuse logic from diff_test.py)
    if args.tags:
        tag_set = set(args.tags.split(','))
        tests = [t for t in tests if tag_set & set(t.tags)]
    if args.exclude_tags:
        exclude_tag_set = set(args.exclude_tags.split(','))
        tests = [t for t in tests if not (exclude_tag_set & set(t.tags))]
    if args.groups:
        group_set = set(args.groups.split(','))
        tests = [t for t in tests if any(g in group_set for g in t.groups)]
    if args.exclude_groups:
        exclude_group_set = set(args.exclude_groups.split(','))
        tests = [t for t in tests if not any(g in exclude_group_set for g in t.groups)]
    if args.filter:
        tests = [t for t in tests if args.filter in t.name]
    
    if not tests:
        print(f"{RED}No tests match the filters{RESET}")
        return 1
    
    # Group by top-level group, then by subgroup
    by_group = defaultdict(lambda: defaultdict(list))
    for test in tests:
        top_group = test.groups[0] if test.groups else 'ungrouped'
        # Determine subgroup from the name hierarchy
        name_parts = test.name.split(':')
        
        # Examples:
        # "core:shfli_playZero[...]" -> top=core, sub=None (only 2 parts, no subgroup)
        # "core:general:nop[...]" -> top=core, sub=general (3+ parts, has subgroup)
        # "ziasm:register:test_0[...]" -> top=ziasm, sub=register
        
        if len(name_parts) >= 3 and name_parts[0] == top_group:
            # Has subgroup: group:subgroup:testname
            subgroup = name_parts[1]
        else:
            # No subgroup: just group:testname
            subgroup = None
        
        by_group[top_group][subgroup].append(test)
    
    # Print header
    total = len(tests)
    num_groups = len(by_group)
    print(f"\n{HEADER_COLOR}{'=' * 80}{RESET}")
    print(f"{HEADER_COLOR}Test Manifest: {total} tests across {num_groups} groups{RESET}")
    print(f"{HEADER_COLOR}{'=' * 80}{RESET}\n")
    
    # Print each group
    for group_name in sorted(by_group.keys()):
        subgroups = by_group[group_name]
        total_group_tests = sum(len(tests) for tests in subgroups.values())
        
        # Count subgroups (excluding None)
        num_subgroups = sum(1 for sg in subgroups.keys() if sg is not None)
        
        # Group header with arrow
        if num_subgroups > 0:
            print(f"{DIM}→{group_name}:{RESET} {CYAN}{total_group_tests}{RESET} {DIM}tests in{RESET} {CYAN}{num_subgroups}{RESET} {DIM}subgroups{RESET}")
        else:
            print(f"{DIM}→{group_name}:{RESET} {CYAN}{total_group_tests}{RESET} {DIM}tests{RESET}")
        
        # Print each subgroup
        for subgroup_name in sorted(subgroups.keys(), key=lambda x: (x is not None, x)):
            subgroup_tests = subgroups[subgroup_name]
            
            # Subgroup header (if there are subgroups)
            if subgroup_name is not None:
                print(f"  {DIM}→{group_name}:{RESET}{WHITE}{subgroup_name}:{RESET} {CYAN}{len(subgroup_tests)}{RESET} {DIM}tests{RESET}")
            
            if args.compact:
                # Compact mode: just names
                for test in subgroup_tests:
                    colored_name, colored_params, _, _ = colorize_test_name(test.name, dim_prefix=True)
                    indent = "    " if subgroup_name else "  "
                    print(f"{indent}{colored_name}{colored_params}")
            else:
                # Full mode: tabular with metadata
                # First pass: calculate maximum name length (hierarchy part only, without params)
                test_data = []
                max_name_len = 0
                
                for test in subgroup_tests:
                    # Get source line count
                    test_dict = test.to_dict()
                    if test.code:
                        test_dict['code'] = test.code
                    if test.file:
                        test_dict['file'] = test.file
                    lines = count_seqc_lines(args.cases_dir, test_dict)
                    
                    # Parse name into hierarchy and params
                    colored_name_part, colored_params_part, raw_name, raw_params = colorize_test_name(test.name, dim_prefix=True)
                    
                    # Track maximum hierarchy length (for alignment)
                    max_name_len = max(max_name_len, len(raw_name))
                    
                    test_data.append((test, lines, colored_name_part, colored_params_part, raw_name, raw_params))
                
                # Second pass: print with proper alignment
                for test, lines, colored_name_part, colored_params_part, raw_name, raw_params in test_data:
                    tags = format_tags(test.tags[:5])  # Show up to 5 tags
                    
                    # Line count
                    if lines is not None:
                        lines_str = f"{GREEN}{lines:3}L{RESET}"
                    else:
                        lines_str = f"{DIM}  —{RESET}"
                    
                    # Calculate padding to align params column
                    # All hierarchy parts should end at the same column
                    name_padding = max_name_len - len(raw_name) + 3  # +3 for spacing
                    
                    # Indentation based on nesting
                    indent = "    " if subgroup_name else "  "
                    
                    # Print row: indent + colored_name + padding + colored_params + lines + tags
                    print(f"{indent}{colored_name_part}{' ' * name_padding}{colored_params_part}  {lines_str}  {tags}")
            
            # Blank line after each subgroup
            if subgroup_name is not None:
                print()
        
        print()  # Blank line between groups
    
    # Summary footer
    print(f"{DIM}{'─' * 80}{RESET}")
    print(f"{HEADER_COLOR}Total: {total} tests{RESET}\n")
    
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Pretty-print test manifest with colors and metadata",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Show all tests
  python3 show_manifest.py
  
  # Show only ZIAI tests
  python3 show_manifest.py --groups ziai
  
  # Show HDAWG tests with verbose output
  python3 show_manifest.py --tags hdawg -v
  
  # Compact view of documentation tests
  python3 show_manifest.py --groups documentation_examples --compact
  
  # Filter by name pattern
  python3 show_manifest.py --filter arithmetic
        """
    )
    
    parser.add_argument("--cases-dir", type=Path,
                        default=Path(__file__).parent / "cases",
                        help="Directory containing manifest.json")
    
    # Filtering options (same as test runners)
    parser.add_argument("--filter", default=None,
                        help="Only show tests whose name contains this string")
    parser.add_argument("--tags", default=None,
                        help="Only show tests with these tags (comma-separated)")
    parser.add_argument("--exclude-tags", default=None,
                        help="Exclude tests with these tags (comma-separated)")
    parser.add_argument("--groups", default=None,
                        help="Only show tests from these groups (comma-separated)")
    parser.add_argument("--exclude-groups", default=None,
                        help="Exclude tests from these groups (comma-separated)")
    
    # Display options
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Verbose output (multi-line per test)")
    parser.add_argument("--compact", action="store_true",
                        help="Compact output (names only)")
    
    args = parser.parse_args()
    
    try:
        return show_manifest(args)
    except Exception as e:
        print(f"{RED}Error: {e}{RESET}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
