// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// pybind11 entry points for the SeqC compiler Python module.
//
// Functions:
//   pyCompileSeqc       @0xe0000  — Python-callable wrapper around compileSeqc
//   makeSeqcCompiler    @0xe1900  — registers compile_seqc into a pybind11 module
//   makeSeqcCompilerInCore        — calls makeSeqcCompiler with inCore=true
//   PyInit__seqc_compiler @0xf5350 — CPython module init entry point
//
// Guarded by ZHINST_HAS_PYBIND11 so builds without pybind11 still compile.
// ============================================================================

#ifdef ZHINST_HAS_PYBIND11

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <boost/json.hpp>

#include <string>

namespace py = pybind11;

namespace zhinst {

// Forward declaration of the core compilation function (from compile_seqc.cpp)
std::string compileSeqc(std::string const& jsonConfig,
                        std::string sourceCode,
                        std::string deviceId,
                        unsigned long awgIndex,
                        std::string const& options);

// Forward declarations for version globals
// Binary: LaboneVersion::fullVersionWithBuild @ 0xb01ad0 (const char*)
//         LaboneVersion::commitHash           @ 0xb01ad8 (const char*)
namespace LaboneVersion {
extern const char* fullVersionWithBuild;
extern const char* commitHash;
}

// Anonymous namespace for module-local helpers
namespace {

// Docstring for compile_seqc — static string in anonymous namespace at 0xb01a50
const char* const docstring =
    "Compile SeqC source code for a Zurich Instruments AWG device.\n"
    "\n"
    "Args:\n"
    "    code: SeqC source code string.\n"
    "    devtype: Device type identifier (e.g. 'HDAWG8').\n"
    "    options: Device options string (newline-separated).\n"
    "    index: AWG core index (default: 0).\n"
    "    **kwargs: Additional keyword arguments passed as JSON config.\n"
    "\n"
    "Returns:\n"
    "    Tuple of (elf_bytes, json_result_dict).\n";

}  // anonymous namespace

// ============================================================================
// pyCompileSeqc @0xe0000
//
// Signature (from binary mangling):
//   pybind11::object pyCompileSeqc(
//       const std::string& sourceCode,
//       std::string deviceId,
//       const pybind11::object& samplerate_or_extra,
//       unsigned long awgIndex,
//       const pybind11::kwargs& kwargs)
//
// Binary flow:
//   1. Try to cast 3rd arg to string (pyTryCast<string>) — if it succeeds,
//      use as the config/options string directly.
//   2. If cast fails, treat 3rd arg as a dict/kwargs and iterate to build
//      a JSON config object.
//   3. Merge **kwargs into the JSON config.
//   4. Create tracing span "pyCompileSeqc".
//   5. Release the GIL (gil_scoped_release).
//   6. Call compileSeqc(jsonConfig, sourceCode, deviceId, awgIndex, options).
//   7. Re-acquire GIL.
//   8. Split packed result: JSON part (up to first '\0') + ELF binary part.
//   9. Return Python tuple: (bytes(elf), json.loads(jsonResult)).
// ============================================================================
py::object pyCompileSeqc(std::string const& sourceCode,     // @0xe0000
                         std::string deviceId,
                         py::object const& samplerate_or_extra,
                         unsigned long awgIndex,
                         py::kwargs const& kwargs)
{
    std::string options;
    boost::json::object jobj;

    // Step 1-2: Try to extract options string from 3rd arg
    // Binary @0xe0000: tries py::cast<string> on 3rd arg first.
    // If it's a string, that becomes the options parameter.
    // If not, extract config entries from it (dict or scalar samplerate).
    // In BOTH cases, kwargs are merged into the JSON config afterward.
    try {
        options = samplerate_or_extra.cast<std::string>();
    } catch (py::cast_error const&) {
        // 3rd arg is not a string — build JSON config from it
        if (py::isinstance<py::dict>(samplerate_or_extra)) {
            py::dict dict = samplerate_or_extra.cast<py::dict>();
            for (auto& item : dict) {
                std::string key = py::str(item.first);
                try {
                    double val = item.second.cast<double>();
                    jobj[key] = val;
                } catch (...) {
                    try {
                        std::string val = item.second.cast<std::string>();
                        jobj[key] = val;
                    } catch (...) {
                        // Skip unsupported types
                    }
                }
            }
        } else if (!samplerate_or_extra.is_none()) {
            // Scalar: treat as samplerate
            try {
                double sr = samplerate_or_extra.cast<double>();
                jobj["samplerate"] = sr;
            } catch (...) {
                // ignore
            }
        }
    }

    // Step 3: Merge kwargs into JSON config (always, regardless of 3rd arg type)
    // Binary @0xe0000: kwargs iteration happens unconditionally after the
    // try/catch block for the 3rd arg cast.
    for (auto& item : kwargs) {
        std::string key = py::str(item.first);
        try {
            double val = item.second.cast<double>();
            jobj[key] = val;
        } catch (...) {
            try {
                std::string val = item.second.cast<std::string>();
                jobj[key] = val;
            } catch (...) {
                // Skip unsupported types
            }
        }
    }

    std::string jsonConfig = boost::json::serialize(jobj);

    // Step 4-5: Release GIL and call compileSeqc
    std::string packed;
    {
        py::gil_scoped_release release;
        packed = compileSeqc(jsonConfig, sourceCode, deviceId, awgIndex, options);
    }

    // Step 6-8: Split packed result at first '\0'
    size_t sep = packed.find('\0');
    std::string jsonResult;
    std::string elfData;
    if (sep != std::string::npos) {
        jsonResult = packed.substr(0, sep);
        elfData = packed.substr(sep + 1);
    } else {
        jsonResult = packed;
    }

    // Step 9: Return Python tuple (bytes(elf), json.loads(json_result))
    py::bytes elfBytes(elfData);
    py::object jsonDict = py::module_::import("json").attr("loads")(jsonResult);
    return py::make_tuple(elfBytes, jsonDict);
}

// ============================================================================
// makeSeqcCompiler @0xe1900
//
// Registers pyCompileSeqc into a pybind11 module.
//
// Binary flow:
//   When inCore=false: function name = "compile_seqc"
//   When inCore=true:  function name = "_" + name
//   Registers via m.def() with args:
//     pybind11::arg("code"), pybind11::arg("devtype"),
//     pybind11::arg("options"), pybind11::arg_v("index", 0),
//     plus kwargs and docstring.
// ============================================================================
void makeSeqcCompiler(py::module_& m,             // @0xe1900
                      std::string const& name,
                      bool inCore)
{
    std::string funcName;
    if (inCore) {
        funcName = "_" + name;
    } else {
        funcName = name;
    }

    m.def(funcName.c_str(),
          &pyCompileSeqc,
          docstring,
          py::arg("code"),
          py::arg("devtype"),
          py::arg("options"),
          py::arg_v("index", 0UL));
}

// ============================================================================
// makeSeqcCompilerInCore
//
// Convenience wrapper: calls makeSeqcCompiler with inCore=true.
// ============================================================================
void makeSeqcCompilerInCore(py::module_& m, std::string const& name) {
    makeSeqcCompiler(m, name, true);
}

}  // namespace zhinst

// ============================================================================
// PyInit__seqc_compiler @0xf5350
//
// CPython module initialization entry point.
//
// Binary flow:
//   1. Python version check (strncmp first 4 chars of Py_GetVersion())
//   2. Create module: pybind11::module_::create_extension_module("_seqc_compiler", ...)
//   3. Set attributes:
//      __doc__         = "Zurich Instruments LabOne SeqC Compiler."
//      __version__     = LaboneVersion::fullVersionWithBuild
//      __commit_hash__ = LaboneVersion::commitHash
//   4. Call makeSeqcCompiler(module, "compile_seqc", false)
//
// Module name: "_seqc_compiler" (from string at 0x8fec4b)
// ============================================================================
PYBIND11_MODULE(_seqc_compiler, m) {                        // @0xf5350
    m.doc() = "Zurich Instruments LabOne SeqC Compiler.";   // 41 chars

    // Set version attributes from global strings
    // Binary reads from LaboneVersion::fullVersionWithBuild @ 0xb01ad0
    // and LaboneVersion::commitHash @ 0xb01ad8
    if (zhinst::LaboneVersion::fullVersionWithBuild) {
        m.attr("__version__") = zhinst::LaboneVersion::fullVersionWithBuild;
    }
    if (zhinst::LaboneVersion::commitHash) {
        m.attr("__commit_hash__") = zhinst::LaboneVersion::commitHash;
    }

    // Register compile_seqc function
    std::string funcName = "compile_seqc";
    zhinst::makeSeqcCompiler(m, funcName, false);
}

// Provide default definitions for the version globals
// (In the real binary these are populated at link time from LabOne build system)
namespace zhinst::LaboneVersion {
const char* fullVersionWithBuild = "0.0.0+reconstructed";
const char* commitHash = "unknown";
}

#endif  // ZHINST_HAS_PYBIND11
