# Waveform DSL Catalogue {#notes_waveform_generator_funcmap}

`WaveformGenerator` evaluates the **waveform DSL** at compile
time.  When a SeqC source contains an expression of waveform type
— for example `gauss(64, 32, 8)`, `add(a, b)`, or
`scale(0.5, w)` — the front-end dispatches through
`WaveformGenerator::call(name, args)`, which looks the function
up in an internal dispatch table and invokes the matching
generator.  The result is a `WaveformIR` value that flows on
through the pipeline; no DSL evaluation happens at run time on
the device.

The DSL is a closed set: a SeqC program can only use the function
names listed below.  Unrecognised names are rejected with
*calling unknown function 'NAME'*.

## Generators (33)

| Group       | Functions                                                                                          |
|-------------|----------------------------------------------------------------------------------------------------|
| Constants   | `zeros`, `ones`, `rect`, `placeholder`, `vect`                                                     |
| Trig        | `sine`, `cosine`, `sinc`                                                                            |
| Ramps       | `ramp`, `sawtooth`, `triangle`                                                                      |
| Bell shapes | `gauss`, `drag`, `blackman`, `hamming`, `hann`                                                      |
| Chirps      | `chirp`, `rrc`                                                                                      |
| Markers     | `marker`, `mask`, `lfsrGaloisMarker`                                                                |
| Random      | `rand`, `randomGauss`, `randomUniform`                                                              |
| Combiners   | `join`, `add`, `interleave`, `scale`, `multiply`                                                    |
| Slicing     | `cut`, `flip`, `filter`, `circshift`                                                                |

Argument lists, validation rules, and per-generator semantics
live with the individual `WaveformGenerator::<name>(...)` methods
in `reconstructed/src/waveform/waveform_generator*.cpp`.

## Deprecated aliases

| Alias | Forwards to     | Emits             |
|-------|-----------------|-------------------|
| `mask` | `marker`       | `DeprecatedFunc` warning before dispatch |
| `rand` | `randomGauss`  | `DeprecatedFunc` warning before dispatch |

The two names are accepted by the parser and the dispatcher;
`WaveformGenerator::call` emits the warning and then redirects
to the canonical entry above.

## Internal-only methods (not user-callable)

`WaveformGenerator` exposes a handful of methods that are reached
only from C++ inside the runtime (typically from
`custom_functions_play.cpp`) and are **not** registered in the
dispatch table.  Calling `merge(w1, w2)` or `grow(w, n)` from
SeqC source is therefore rejected with
*calling unknown function 'merge'* / *'grow'*.

| Method  | Reached from               | Purpose                                                           |
|---------|----------------------------|-------------------------------------------------------------------|
| `merge` | `custom_functions_play.cpp` | Per-channel waveform merge during `playWave` lowering.            |
| `grow`  | `custom_functions_play.cpp` | Extend a waveform to a target length during marker emission.      |

## See also

- \ref notes_custom_functions — `playWave` and friends that call
  the internal-only methods above.
- \ref notes_play_config — `WaveformIR` flows into `PlayConfig`
  during prefetch.
