#!/usr/bin/env python3
"""Dump SeqC compiler ELF sections for comparison.

Usage:
    python tests/dump_elf.py tests/cases/hdawg_play_dual_ch.seqc HDAWG8 [--samplerate 2.4e9] [--sequencer qa] [--recon]
    python tests/dump_elf.py tests/cases/hdawg_play_dual_ch.seqc HDAWG8 --both

The --both flag dumps original and recon side-by-side with diffs highlighted.
"""

import argparse
import struct
import sys
import os


def parse_elf_sections(data: bytes) -> dict:
    """Parse a 32-bit LE ELF and return {name: bytes} for each section."""
    if len(data) < 52 or data[:4] != b"\x7fELF":
        return {}
    ei_class = data[4]
    if ei_class == 1:  # 32-bit
        hdr = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
        e_entry = hdr[3]
        e_shoff, e_shentsize, e_shnum, e_shstrndx = hdr[5], hdr[10], hdr[11], hdr[12]
        sh_fmt, sh_size = "<IIIIIIIIII", 40
    elif ei_class == 2:  # 64-bit
        hdr = struct.unpack_from("<HHIQQQIHHHHHH", data, 16)
        e_entry = hdr[3]
        e_shoff, e_shentsize, e_shnum, e_shstrndx = hdr[5], hdr[9], hdr[10], hdr[11]
        sh_fmt, sh_size = "<IIQQQQIIQQ", 64
    else:
        return {}

    shdrs = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        shdrs.append(struct.unpack_from(sh_fmt, data, off))

    strtab_sh = shdrs[e_shstrndx]
    strtab = data[strtab_sh[4]:strtab_sh[4] + strtab_sh[5]]

    sections = {}
    for sh in shdrs:
        end = strtab.find(b'\x00', sh[0])
        name = strtab[sh[0]:end].decode('ascii', errors='replace')
        if name:
            sections[name] = data[sh[4]:sh[4] + sh[5]]

    sections['__e_entry__'] = e_entry
    return sections


def compile_seqc(code, devtype, samplerate=None, sequencer=None, index=0,
                 module_dir='.'):
    """Compile SeqC and return ELF bytes."""
    sys.path.insert(0, module_dir)
    # Fresh import each time
    mod_name = '_seqc_compiler'
    if mod_name in sys.modules:
        del sys.modules[mod_name]
    mod = __import__(mod_name)
    kwargs = {}
    if samplerate is not None:
        kwargs['samplerate'] = samplerate
    if sequencer is not None:
        kwargs['sequencer'] = sequencer
    result = mod.compile_seqc(code, devtype, {}, index, **kwargs)
    if isinstance(result, tuple):
        return result[0]
    return result.get('elf', b'')


def dump_section(name, data, indent="  "):
    """Pretty-print a section."""
    is_text = len(data) > 0 and all(
        32 <= b < 127 or b in (10, 13, 9) for b in data[:200])

    if name == '.text':
        # Binary: 4-byte instruction words, little-endian
        print(f"{indent}{name}: {len(data)} bytes = {len(data)//4} instructions")
        for i in range(0, len(data), 4):
            word = struct.unpack_from('<I', data, i)[0]
            print(f"{indent}  [{i//4:3d}] 0x{word:08x}")
    elif name == '.linenr':
        # Binary: 16-byte records of 4 x int32 LE: (absIdx, counter, seq, lineNumber)
        # See reconstructed/notes/elf_reader.md for the full layout.
        print(f"{indent}{name}: {len(data)} bytes = {len(data)//16} records")
        for i in range(0, len(data), 16):
            absIdx, counter, seq, ln = struct.unpack_from('<iiii', data, i)
            print(f"{indent}  abs={absIdx} ctr={counter} seq={seq} line={ln}")
    elif name == '.channels':
        # Binary: chanInfo.size() * sizeof(int) bytes (variable, per-device).
        print(f"{indent}{name}: {len(data)} bytes = {data.hex()}")
    elif name == '.version_bin':
        # Binary: 16 bytes
        print(f"{indent}{name}: {data.hex()}")
    elif name.startswith('.wf_'):
        # Binary: waveform samples, 16-bit signed LE pairs (I,Q or single channel)
        print(f"{indent}{name}: {len(data)} bytes = {len(data)//2} samples")
        # Show first few samples
        for i in range(0, min(len(data), 32), 2):
            val = struct.unpack_from('<h', data, i)[0]
            print(f"{indent}  [{i//2:3d}] {val:6d} (0x{val & 0xFFFF:04x})")
        if len(data) > 32:
            print(f"{indent}  ...")
    elif is_text:
        # Text sections: .asm, .c, .filename, .waveforms, .wavemem,
        #   .arguments, .version_json, .nodes_json
        text = data.decode('ascii', errors='replace')
        print(f"{indent}{name}: ({len(data)} bytes)")
        for line in text.split('\n')[:200]:
            print(f"{indent}  {line}")
        lines = text.split('\n')
        if len(lines) > 200:
            print(f"{indent}  ... ({len(lines) - 200} more lines)")
    else:
        # Unknown binary
        print(f"{indent}{name}: {len(data)} bytes (binary)")
        print(f"{indent}  hex: {data[:64].hex()}")


def dump_both(code, devtype, samplerate, sequencer, index, recon_dir):
    """Dump and compare original vs recon."""
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    print("=== Compiling with ORIGINAL ===")
    try:
        orig_elf = compile_seqc(code, devtype, samplerate, sequencer, index,
                                repo_root)
        orig = parse_elf_sections(orig_elf)
        print(f"ELF: {len(orig_elf)} bytes, e_entry=0x{orig.get('__e_entry__', 0):x}")
    except Exception as e:
        print(f"ERROR: {e}")
        return

    # For recon, we need a subprocess to avoid module conflicts
    import subprocess, tempfile, json
    worker = os.path.join(repo_root, 'tests', 'compile_worker.py')
    with tempfile.TemporaryDirectory() as tmpdir:
        elf_path = os.path.join(tmpdir, 'recon.elf')
        meta_path = os.path.join(tmpdir, 'recon.json')
        cmd = [
            sys.executable, worker,
            '--module-dir', recon_dir,
            '--module-name', '_seqc_compiler',
            '--code', code,
            '--devtype', devtype,
            '--index', str(index),
            '--output-elf', elf_path,
            '--output-meta', meta_path,
        ]
        if samplerate is not None:
            cmd += ['--samplerate', str(samplerate)]
        if sequencer is not None:
            cmd += ['--sequencer', sequencer]

        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        meta = {}
        if os.path.exists(meta_path):
            with open(meta_path) as f:
                meta = json.load(f)
        if meta.get('error'):
            print(f"\n=== RECON ERROR: {meta['error'][:200]} ===")
            return
        if not os.path.exists(elf_path):
            print(f"\n=== RECON FAILED: {proc.stderr[:200]} ===")
            return
        with open(elf_path, 'rb') as f:
            recon_elf = f.read()

    recon = parse_elf_sections(recon_elf)
    print(f"\n=== Comparing: orig {len(orig_elf)} bytes vs recon {len(recon_elf)} bytes ===")
    print(f"e_entry: orig=0x{orig.get('__e_entry__', 0):x}  recon=0x{recon.get('__e_entry__', 0):x}")

    all_names = sorted(set(k for k in orig if not k.startswith('__')) |
                       set(k for k in recon if not k.startswith('__')))

    for name in all_names:
        o = orig.get(name)
        r = recon.get(name)
        if o is None:
            print(f"\n>>> {name}: MISSING IN ORIGINAL (recon has {len(r)} bytes)")
            continue
        if r is None:
            print(f"\n>>> {name}: MISSING IN RECON (orig has {len(o)} bytes)")
            dump_section(name, o, "  orig: ")
            continue
        if o == r:
            print(f"\n  {name}: IDENTICAL ({len(o)} bytes)")
            continue

        print(f"\n>>> {name}: DIFFERS (orig={len(o)} recon={len(r)} bytes)")
        print("  --- ORIGINAL ---")
        dump_section(name, o, "    ")
        print("  --- RECON ---")
        dump_section(name, r, "    ")


def main():
    p = argparse.ArgumentParser(description="Dump SeqC ELF sections")
    p.add_argument('seqc_file', help='Path to .seqc source file')
    p.add_argument('devtype', help='Device type (e.g. HDAWG8, SHFQA4)')
    p.add_argument('--samplerate', type=float, default=None)
    p.add_argument('--sequencer', default=None)
    p.add_argument('--index', type=int, default=0)
    p.add_argument('--recon', action='store_true',
                   help='Use recon module instead of original')
    p.add_argument('--both', action='store_true',
                   help='Compare original and recon side-by-side')
    p.add_argument('--recon-dir', default=None,
                   help='Recon build directory')
    args = p.parse_args()

    with open(args.seqc_file) as f:
        code = f.read()

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    recon_dir = args.recon_dir or os.path.join(repo_root, 'reconstructed', 'build')

    if args.both:
        dump_both(code, args.devtype, args.samplerate, args.sequencer,
                  args.index, recon_dir)
        return

    module_dir = recon_dir if args.recon else repo_root
    try:
        elf = compile_seqc(code, args.devtype, args.samplerate, args.sequencer,
                           args.index, module_dir)
    except Exception as e:
        print(f"Compilation error: {e}")
        sys.exit(1)

    sections = parse_elf_sections(elf)
    label = "RECON" if args.recon else "ORIGINAL"
    print(f"=== {label}: {len(elf)} bytes, e_entry=0x{sections.get('__e_entry__', 0):x} ===\n")

    for name in sorted(k for k in sections if not k.startswith('__')):
        dump_section(name, sections[name])
        print()


if __name__ == '__main__':
    main()
