# WaveformGenerator::funcMap_ parity audit {#notes_waveform_generator_funcmap}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

**Audited**: 2026-05-08 (resolves IF-202)
**Binary range**: `WaveformGenerator` ctor at `0x248200`..`0x249b90`
**Source of truth**: objdump of `_seqc_compiler.so`, sequence of
`std::piecewise_construct` emplace calls into the funcMap_ at `this+0x88`.

## Authoritative binary funcMap_ entries (33)

Each row corresponds to one `lea piecewise_construct,%rdx` /
`lea <method>,%rcx` / `call __emplace_unique_key_args` triple inside the
constructor.  Keys are written into the stack frame as libc++ short-string
SSO bytes (`movb len<<1 ; movl/movw chars`) immediately before the
emplace call.

| #  | Key                | Method          | Method addr | Emplace site |
|----|--------------------|-----------------|-------------|--------------|
| 1  | `zeros`            | `zeros`         | 0x249b90    | 0x2482ca     |
| 2  | `ones`             | `ones`          | 0x249e10    | 0x248368     |
| 3  | `sine`             | `sin`           | 0x24a0f0    | 0x2483f4     |
| 4  | `cosine`           | `cos`           | 0x24abd0    | 0x248486     |
| 5  | `sinc`             | `sinc`          | 0x24b6e0    | 0x248512     |
| 6  | `ramp`             | `ramp`          | 0x24c2c0    | 0x24859e     |
| 7  | `sawtooth`         | `sawtooth`      | 0x24c8b0    | 0x248631     |
| 8  | `triangle`         | `triangle`      | 0x24d330    | 0x2486c4     |
| 9  | `gauss`            | `gauss`         | 0x24ddb0    | 0x248752     |
| 10 | `drag`             | `drag`          | 0x24e950    | 0x2487de     |
| 11 | `blackman`         | `blackman`      | 0x24f530    | 0x248871     |
| 12 | `hamming`          | `hamming`       | 0x24fd20    | 0x248904     |
| 13 | `hann`             | `hann`          | 0x250250    | 0x248990     |
| 14 | `rect`             | `rect`          | 0x250770    | 0x248a1c     |
| 15 | `chirp`            | `chirp`         | 0x250bb0    | 0x248aaa     |
| 16 | `mask`             | `mask`          | 0x251cb0    | 0x248b36     |
| 17 | `marker`           | `marker`        | 0x251cd0    | 0x248bc8     |
| 18 | `rand`             | `rand`          | 0x251cf0    | 0x248c54     |
| 19 | `randomGauss`      | `randomGauss`   | 0x252930    | 0x248cee     |
| 20 | `randomUniform`    | `randomUniform` | 0x253440    | 0x248d8f     |
| 21 | `lfsrGaloisMarker` | `lfsrGaloisMarker` | 0x253bc0 | 0x248e1f     |
| 22 | `rrc`              | `rrc`           | 0x254290    | 0x248ea7     |
| 23 | `vect`             | `vect`          | 0x255570    | 0x248f33     |
| 24 | `placeholder`      | `placeholder`   | 0x255850    | 0x248fcd     |
| 25 | `join`             | `join`          | 0x255da0    | 0x249059     |
| 26 | `add`              | `add`           | 0x256ff0    | 0x2490e1     |
| 27 | `interleave`       | `interleave`    | 0x258140    | 0x24917a     |
| 28 | `scale`            | `scale`         | 0x258270    | 0x249208     |
| 29 | `multiply`         | `multiply`      | 0x258750    | 0x24929b     |
| 30 | `cut`              | `cut`           | 0x2598d0    | 0x249323     |
| 31 | `flip`             | `flip`          | 0x25a310    | 0x2493af     |
| 32 | `filter`           | `filter`        | 0x25a540    | 0x249441     |
| 33 | `circshift`        | `circshift`     | 0x25b0e0    | 0x2494d6     |

## aliasMap_ entries (binary, after funcMap_ block)

After the 33 funcMap_ emplaces, the ctor performs two further emplaces
into a different `unordered_map<string,string>` member at `this+0x28`
(the `aliasMap_`).  Both keys/values are inline SSO writes; no method
LEA is involved.

| Alias key | Resolved value | Emplace site |
|-----------|----------------|--------------|
| `mask`    | `marker`       | 0x24957e     |
| `rand`    | `randomGauss`  | 0x2495ee     |

These are the deprecated-name aliases consumed by `WaveformGenerator::call`
to emit a `DeprecatedFunc` warning before redispatching to the new name.

## Methods that exist but are NOT registered

| Method  | Address  | Why                                                        |
|---------|----------|------------------------------------------------------------|
| `merge` | 0x25f5c0 | Internal-only; reachable from `custom_functions_play.cpp`. |
| `grow`  | 0x260640 | Same as above.                                             |

Recon previously registered both of these in `funcMap_`
(`waveform_generator.cpp:151-152` pre-fix), which made user-callable
SeqC programs `merge(w1,w2)` and `grow(w,n)` succeed in recon while the
original binary rejected them with `"calling unknown function 'merge'"`.
This caused the 11 `cov_merge_*` / `cov_grow_*` cases in
`coverage_round_tier1` to diff.  Fix: removed both registrations; left
the methods in place because `custom_functions_play.cpp` still calls
them through `waveformGen_->merge(...)` / `->grow(...)`.

## Method to verify against the binary

```bash
# All 33 method LEAs into %rcx in the ctor body:
awk 'NR==401031,/^$/' /tmp/obj.txt \
  | grep -E "lea.*\(%rip\),%rcx" \
  | grep -oE "WaveformGenerator[0-9]+[A-Za-z_]+"
```

Should print 33 unique method names (matches table above).

## Diff vs recon (2026-05-08, after fix)

- Keys present in BOTH: 33 (full table above) plus aliasMap entries.
- Keys RECON-ONLY: none (was: `merge`, `grow` — now removed).
- Keys BINARY-ONLY: none.
