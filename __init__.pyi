"""Zurich Instruments LabOne SeqC Compiler."""

from typing import (
    List,
    Union,
    Any,
    Dict,
    Optional,
    Tuple,
)

_STUB_VERSION_HASH = "59606b1dbd6950dacfa7867a6980bf14"

def compile_seqc(
    code: str,
    devtype: str,
    options: Union[str, List[str]],
    index: int,
    samplerate: Optional[float] = None,
    sequencer: Optional[str] = None,
    wavepath: Optional[str] = None,
    waveforms: Optional[str] = None,
    filename: Optional[str] = None,
) -> Tuple[bytes, Dict[str, Any]]:
    """Compile the sequencer code.

    This function is a purely static function that does not require an
    active connection to a Data Server.

    .. versionadded:: 22.08

    Args:
        code: SeqC input
        devtype: target device type, e.g., HDAWG8, SHFQC
        options: list of device options, or string of
            options separated by newlines as returned by node
            /dev.../features/options.
        index: index of the AWG core
        samplerate: target sample rate of the sequencer
            Mandatory and only respected for HDAWG. Should match the
            value set on the device:
            `/dev.../system/clocks/sampleclock/freq`.
        sequencer: one of 'qa', 'sg', or 'auto'.
            Mandatory for SHFQC.
        wavepath: path to directory with waveforms. Defaults to
            path used by LabOne UI or AWG Module.
        waveforms: list of CSV waveform files separated by ';'.
            Defaults to an empty list. Set to `None` to include
            all CSV files in `wavepath`.
        filename: name of embedded ELF filename.

    Returns:
        Tuple (elf, extra) of binary ELF data for sequencer and extra
        dictionary with compiler output.

    Note:
        The same function is available in the `zhinst-seqc-compiler`
        package. `zhinst.core.compile_seqc` will forward the call to
        `zhinst.seqc_compiler.compile_seqc` if a compatible version of
        this package is installed. A version is compatible if major and
        minor package versions match, and the revision of
        `zhinst-seqc-compiler` is greater or equal to the revision of
        `zhinst-core`. A warning will be issued if the versions do not
        match."""
