// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AWGAssemblerImpl — assembly pipeline methods
// ============================================================================

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include <boost/filesystem.hpp>

#include "zhinst/codegen/awg_assembler_impl.hpp"
#include "zhinst/asm/asm_expression.hpp"
#include "zhinst/asm/asm_parser_context.hpp"
#include "zhinst/asm/assembler.hpp"
#include "zhinst/io/elf_writer.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/device/device_constants.hpp"

#include "zhinst/infra/log_macros.hpp"

#include "zhinst/asm/asm_parser_fwd.hpp"

namespace zhinst {

// AsmExpression layout (from shared_ptr_emplace allocation = 0xC0 bytes total,
// object starts at +0x18 from control block):
//   +0x00  int unknown0           — checked for 0 in evaluate() to skip null exprs
//   +0x04..+0x1F  (padding/fields)
//   +0x20  std::string field_20   — comment string (set from $_1 lambda)
//   +0x38  int command            — Assembler::Command enum value
//   +0x40  vector<shared_ptr<AsmExpression>> children  — child operand list
//   +0x58  int lineNumber         — source line number
//   +0x60  std::string labelName  — label identifier
//   +0x78  bool labelType         — 0=definition(defLabel), 1=reference(useLabel)
//   +0x80..+0x8F  (more fields)
//   +0x90  bool noOpt             — set from !doOpt()
//   +0x98  bool field_98
//   +0xA0  bool noOptOverride_          — "noOpt" flag as set by assembleString
//   +0xB0  bool field_B0
//   +0xB8  bool field_B8
// Total object size: ~0xA8 bytes (within the 0xC0 alloc)

// AWGAssemblerImpl layout (size = 0x170):
//   +0x00  (vtable or other)
//   +0x08  std::string filename_          — source filename
//   +0x20  (padding?)
//   +0x38  std::string asmSource_         — assembled source text
//   +0x50  vector<uint32_t> opcodes_      — generated opcodes
//   +0x58  uint32_t* opcodes_end          — current write position
//   +0x60  uint32_t* opcodes_capacity
//   +0x68  int memoryOffset_              — memory offset for output
//   +0x70  uint64_t currentLine_           — current line being processed
//   +0x78  vector<string> sourceLines_    — per-line source strings
//   +0x90  vector<Message> messages_      — error/warning messages
//   +0xA8  (more fields)
//   +0xB0  bimap (labels)                 — boost::bimap<string, multiset_of<int>>
//   +0xE0  bimap pointer (?)
//   +0xF0  AsmParserContext parserCtx_    — parser context (inline)

// Message struct (size = 0x20):
//   +0x00  uint64_t lineNumber
//   +0x08  std::string text               — 0x18 bytes (SSO)

} // namespace zhinst


namespace zhinst {

// =============================================================================
// assembleFile — 0x285ec0
// Reads a file, streams contents to string, then calls assembleString.
// =============================================================================
void AWGAssemblerImpl::assembleFile(const std::string& path) {  // 0x285ec0
    // Check that the file exists using boost::filesystem::status
    boost::filesystem::file_status status = boost::filesystem::detail::status(path, nullptr);
    if (status.type() <= boost::filesystem::file_not_found) {
        // File doesn't exist — throw with ErrorMessageT 0x71
        std::string pathCopy = path;
        std::string msg = ErrorMessages::format(
            FileNotExist, std::move(pathCopy));
        throw ZIAWGCompilerException(msg);
    }

    // Store filename at this+0x08
    filename_ = path;

    // Open the file and read entire contents into ostringstream
    std::ifstream ifs(path, std::ios::in);
    std::ostringstream oss;
    oss << ifs.rdbuf();
    ifs.close();

    // Extract the string from the ostringstream
    std::string contents = oss.str();

    // Store contents in asmSource_ (at this+0x38)
    asmSource_ = contents;

    // Call assembleString on the source
    assembleString(asmSource_);
}

// =============================================================================
// assembleString — 0x286490
// Parses source line-by-line using getAST, collects expressions, then calls
// assembleExpressions at the end.
// =============================================================================
void AWGAssemblerImpl::assembleString(const std::string& src) {  // 0x286490
    std::istringstream iss(src, std::ios::in | std::ios::binary);

    // Local vectors for collecting per-line results
    std::vector<std::shared_ptr<AsmExpression>> expressions;  // collected AST nodes
    std::vector<uint64_t> lineNumbers;                        // line# for each expression
    std::string line;

    // Clear parser state and set error callback (lambda $_0 captures 'this')
    parserCtx_.clearSyntaxError();
    parserCtx_.setErrorCallback([this](int code, const std::string& msg) {
        parserMessage(code, msg);
    });

    currentLine_ = 0;

    // Process each line
    while (std::getline(iss, line)) {
        // Parse one line → returns shared_ptr<AsmExpression>
        std::shared_ptr<AsmExpression> ast = getAST(line);
        currentLine_++;

        if (ast) {
            // Set the noOpt flag from parser context
            ast->noOpt() = !parserCtx_.doOpt();

            // Add to expressions vector
            expressions.push_back(ast);

            // If command == 2 (LABEL), record the line number
            if (ast->command == 2) {
                lineNumbers.push_back(currentLine_);
            }
        }

        // Regardless, push source line to sourceLines_
        sourceLines_.push_back(line);

        // Record line numbers for non-label expressions too
        // (from disassembly: lineNumbers are pushed for LABEL commands)

        parserCtx_.endLineComment();

        // Release the shared_ptr (decrements refcount)
        // (automatic at loop iteration)
    }

    // After processing all lines, assemble the collected expressions
    assembleExpressions(expressions, lineNumbers);
    parserCtx_.cleanStringCopies();
}

// =============================================================================
// getAST — 0x286ca0
// Parses a single line of assembly text using flex/bison (asmlex/asmparse).
// Returns shared_ptr<AsmExpression> by value (sret).
// =============================================================================
std::shared_ptr<AsmExpression> AWGAssemblerImpl::getAST(const std::string& line) {  // 0x286ca0
    std::shared_ptr<AsmExpression> result;  // initialized to nullptr

    void* scanner = nullptr;
    if (asmlex_init_extra(&parserCtx_, &scanner) != 0) {
        LOG_ERROR("Error initializing flex scanner");
        return result;
    }

    const char* lineStr = line.c_str();
    auto buf = asm_scan_string(lineStr, scanner);

    AsmExpression* rawExpr = nullptr;
    int parseResult = asmparse(&parserCtx_, &rawExpr, scanner);

    if (parseResult != 0) {
        LOG_ERROR("Error parsing line");
        asm_delete_buffer(buf, scanner);
        asmlex_destroy(scanner);
        return result;
    }

    // Wrap raw pointer in shared_ptr
    result.reset(rawExpr);

    asm_delete_buffer(buf, scanner);
    asmlex_destroy(scanner);
    return result;
}

// =============================================================================
// assembleStringToExpressionsVec — 0x286e40
// Similar to assembleString but returns the expressions vector (sret)
// instead of calling assembleExpressions internally.
// Also handles comment extraction and creates NOP expressions for comment-only lines.
// =============================================================================
std::vector<std::shared_ptr<AsmExpression>>
AWGAssemblerImpl::assembleStringToExpressionsVec(const std::string& src) {  // 0x286e40
    std::istringstream iss(src, std::ios::in | std::ios::binary);
    std::string line;
    std::vector<std::shared_ptr<AsmExpression>> result;

    parserCtx_.clearSyntaxError();
    parserCtx_.setErrorCallback([this](int code, const std::string& msg) {
        parserMessage(code, msg);
    });

    currentLine_ = 0;

    while (std::getline(iss, line)) {
        std::shared_ptr<AsmExpression> ast = getAST(line);
        currentLine_++;

        if (ast) {
            // Has a parsed expression — push source line
            sourceLines_.push_back(line);

            // Extract comment after "//" using $_1 lambda
            std::string comment = extractComment(line);

            // Set comment field on the AST node (at +0x20)
            ast->comment = comment;

            // Set noOpt flag
            ast->noOpt() = !parserCtx_.doOpt();

            // Add to result
            result.push_back(ast);
        } else if (parserCtx_.isLineComment()) {
            // Line is a comment-only line — still record source and create a NOP expr
            sourceLines_.push_back(line);

            // Allocate a new AsmExpression via make_shared (emplace: 0xC0 bytes)
            auto nopExpr = std::make_shared<AsmExpression>();
            nopExpr->command = 4;  // NOP/comment placeholder

            // Extract and set comment
            std::string comment = extractComment(line);
            nopExpr->comment = comment;

            // Increment program counter in parser
            parserCtx_.incrementProgramCounter();

            // Add to result
            result.push_back(nopExpr);
        }

        parserCtx_.endLineComment();
        // Release ast shared_ptr
    }

    parserCtx_.cleanStringCopies();

    // Check for syntax errors
    if (parserCtx_.hadSyntaxError()) {
        std::string report = getReport();
        std::cout << report << std::endl;
        throw ZIAWGCompilerException("Syntax error in assembly source");
    }

    return result;
}

// =============================================================================
// assembleAsmList — 0x2876a0
// Converts a vector of Assembler objects into AsmExpression trees and
// calls assembleExpressions.
// =============================================================================
void AWGAssemblerImpl::assembleAsmList(const std::vector<Assembler>& asmList) {  // 0x2876a0
    std::vector<std::shared_ptr<AsmExpression>> expressions;
    std::vector<uint64_t> lineNumbers;

    parserCtx_.clearSyntaxError();
    parserCtx_.setErrorCallback([this](int code, const std::string& msg) {
        parserMessage(code, msg);
    });

    currentLine_ = 0;
    int labelCounter = 0;

    for (const auto& instr : asmList) {
        currentLine_++;

        // Initialize a shared_ptr<AsmExpression> for this instruction
        std::shared_ptr<AsmExpression> expr;

        // Check the command type
        if (instr.cmd == Assembler::ERROR_MSG || instr.cmd == Assembler::MESSAGE) {
            // Print warning to cout and skip  (binary strings at .rodata 0x906a54, 0x8fe737, 0x906a64)
            std::cout << "Warning (line: " << currentLine_
                      << "): "
                      << "contains asm print statement (msg or err)! \n";
            continue;
        }

        if (instr.cmd == Assembler::LABEL) {
            // Create a LABEL expression
            auto exprObj = std::make_shared<AsmExpression>();
            exprObj->command = instr.cmd;
            expr = exprObj;

            // Copy the label string from the instruction
            std::string labelStr = instr.label;

            // Note on binary structure: at 0x287e33 the original constructs a
            // local AsmParserContext::Label{labelCounter, labelStr}. That Label
            // exists ONLY to hold a copy of the string until it is moved into
            // exprObj->labelName below; its construction has no other observable
            // side effect (and there is no addLabel/hasLabel registration here).
            // We therefore omit the temporary and assign labelName directly,
            // which is semantically equivalent.

            // Set label-related fields on the expression
            exprObj->labelIndex = labelCounter;  // +0x58 = labelIndex (verified mov [r14+0x70],ecx where r14=ctrl block, so expr+0x58)

            // Depending on hasLabel (at +0x90 == 1 means "use/ref")
            if (exprObj->hasLabel == 1) {
                // Copy label name to +0x78
                exprObj->labelName = labelStr;
            } else {
                // Move label name; mark as definition
                exprObj->labelName = std::move(labelStr);
                exprObj->hasLabel = 1;  // mark as has-label
            }
        } else {
            // General instruction: create expression with matching command
            auto exprObj = std::make_shared<AsmExpression>();
            exprObj->command = instr.cmd;
            expr = exprObj;
        }

        // Child ordering: immediates → regDst → regAux → regSrc → outputs → label
        //
        // Binary verification (2025-04-26): the binary's assembleAsmList at
        // 0x287946 adds children in this exact order:
        //   1. immediates vector (+0x08)     — createValue per element
        //   2. regDst register  (+0x28)      — createRegister if isValid
        //   3. regAux register  (+0x30)      — createRegister if isValid
        //   4. regSrc register  (+0x20)      — createRegister if isValid
        //   5. outputs vector   (+0x38)      — createValue per element
        //   6. label string                  — createName if non-empty
        //
        // The asm_commands functions place values in either immediates or
        // outputs depending on context:
        //   - aluiu/alui: values go in outputs(+0x38), regs in regDst/regSrc
        //     → children = [regDst(=dst), regSrc(=src), output(=imm)]
        //   - alur: no values, regs in regAux/regSrc
        //     → children = [regAux(=dst), regSrc(=src)]
        //   - st: reg in regSrc(+0x20), addr in outputs(+0x38)
        //     → children = [regSrc(=reg), output(=addr)]

        // Add immediate operands as child value expressions (+0x08)
        for (const auto& imm : instr.immediates) {
            int val = static_cast<int>(imm);
            AsmExpression* valExpr = createValue(val);
            std::shared_ptr<AsmExpression> child(valExpr);
            expr->children.push_back(std::move(child));
        }

        // Add destination register (binary offset +0x28 in Assembler)
        if (instr.regDst.isValid()) {
            int regIdx = static_cast<int>(instr.regDst);
            AsmExpression* regExpr = createRegister(regIdx);
            std::shared_ptr<AsmExpression> child(regExpr);
            expr->children.push_back(std::move(child));
        }

        // Add auxiliary register (+0x30)
        if (instr.regAux.isValid()) {
            int regIdx = static_cast<int>(instr.regAux);
            AsmExpression* regExpr = createRegister(regIdx);
            std::shared_ptr<AsmExpression> child(regExpr);
            expr->children.push_back(std::move(child));
        }

        // Add source register (binary offset +0x20)
        if (instr.regSrc.isValid()) {
            int regIdx = static_cast<int>(instr.regSrc);
            AsmExpression* regExpr = createRegister(regIdx);
            std::shared_ptr<AsmExpression> child(regExpr);
            expr->children.push_back(std::move(child));
        }

        // Add output operands (vector at +0x38)
        for (const auto& imm : instr.outputs) {
            int val = static_cast<int>(imm);
            AsmExpression* valExpr = createValue(val);
            std::shared_ptr<AsmExpression> child(valExpr);
            expr->children.push_back(std::move(child));
        }

        // If the instruction has a label reference (branch target)
        if (!instr.label.empty()) {
            AsmExpression* nameExpr = createName(instr.label.c_str());
            std::shared_ptr<AsmExpression> child(nameExpr);
            expr->children.push_back(std::move(child));
        }

        // Get the string representation and push to sourceLines_
        std::string instrStr = instr.str(true);
        sourceLines_.push_back(std::move(instrStr));

        // Only increment labelCounter for non-LABEL instructions.
        // Labels don't generate opcodes, so the label index must track
        // the opcode position (i.e., skip labels in the count).
        // Binary evidence: at 0x287e33 the label gets the current
        // labelCounter value, but the increment at 0x2879xx only
        // executes for the non-LABEL path.
        if (instr.cmd != Assembler::LABEL) {
            labelCounter++;
        }

        // Record line numbers (for label resolution)
        lineNumbers.push_back(currentLine_);

        // Push expression
        expressions.push_back(expr);
    }

    // Final assembly pass
    assembleExpressions(expressions, lineNumbers);
}

// =============================================================================
// assembleExpressions — 0x285620
// Core assembly: evaluates each expression, producing opcodes.
// First pass: collect labels into bimap. Second pass: evaluate.
// Appends a trailing 0 opcode (NOP/END).
// =============================================================================
void AWGAssemblerImpl::assembleExpressions(
    const std::vector<std::shared_ptr<AsmExpression>>& expressions,
    const std::vector<uint64_t>& lineNumbers) {  // 0x285620

    // Reset opcodes vector and reserve capacity for expressions.size() entries.
    // The original disassembly manipulated raw begin/end/cap pointers in
    // opcodes_; here we use the equivalent std::vector interface.
    size_t numExprs = expressions.size();
    opcodes_.clear();
    if (opcodes_.capacity() < numExprs) {
        opcodes_.reserve(numExprs);
    }

    // Clear the label bimap
    labelBimap_.clear();

    // First pass: register all labels
    for (const auto& expr : expressions) {
        AsmExpression* e = expr.get();
        if (e->hasLabel == 1) {  // label definition
            int labelIndex = e->labelIndex;  // +0x58 (labelIndex; verified disasm 0x285732 mov r15d,[rax+0x58])
            std::string labelName = e->labelName;  // at +0x60
            // Insert into bimap: (name -> index)
            labelBimap_.insert({labelName, labelIndex});
        }
    }

    // Second pass: evaluate each expression
    size_t i = 0;
    for (const auto& expr : expressions) {
        // Set line number from lineNumbers vector
        currentLine_ = static_cast<int32_t>(lineNumbers[i]);

        AsmExpression* e = expr.get();
        if (e->command == 2) {
            // LABEL — skip (no opcode generated)
            i++;
            continue;
        }

        // Evaluate the expression (generates opcode(s))
        int opcode = evaluate(expr);

        // Store opcode
        opcodes_.push_back(opcode);
        i++;
    }

    // Check for syntax errors after evaluation
    if (parserCtx_.hadSyntaxError()) {
        std::string report = getReport();
        std::cout << report << std::endl;
        throw ZIAWGCompilerException("Syntax error in assembly source");
    }

    // Append trailing 0 (END/NOP) if last opcode is non-zero
    if (!opcodes_.empty() && opcodes_.back() != 0) {
        opcodes_.push_back(0);
    }
}

// =============================================================================
// evaluate — 0x285b20
// Dispatches to opcode0..opcode5 based on getOpcodeType(command).
// =============================================================================
int AWGAssemblerImpl::evaluate(const std::shared_ptr<AsmExpression>& expr) {  // 0x285b20
    AsmExpression* e = expr.get();
    if (!e || e->type != 0) {
        return 0;  // non-container expressions (reg/label/int) don't generate opcodes
    }

    int cmd = e->command;
    if (cmd == static_cast<int>(Assembler::INVALID)) {
        return 0;  // INVALID command
    }

    int opcodeType = Assembler::getOpcodeType(static_cast<Assembler::Command>(cmd));
    if (opcodeType > 5) {
        return 0;  // unknown type
    }

    // Dispatch based on opcodeType (jump table at 0x95d094)
    switch (opcodeType) {
        case 0: return opcode0(cmd, expr);   // 0x2895c0
        case 1: return opcode1(cmd, expr);   // 0x289860
        case 2: return opcode2(cmd, expr);   // 0x289a10
        case 3: return opcode3(cmd, expr);   // 0x289c90
        case 4: return opcode4(cmd, expr);   // 0x28a010
        case 5: return opcode5(cmd, expr);   // 0x28a610
        default: return 0;
    }
}

// =============================================================================
// getReport — 0x285bc0
// Formats all accumulated messages into a single string.
// Format: "Assembler message at N: <text>\n" for each message.
// =============================================================================
std::string AWGAssemblerImpl::getReport() const {  // 0x285bc0
    std::ostringstream oss;

    // Iterate over messages_ vector (at +0x90)
    // Each message is 0x20 bytes: int code at +0x00 (line# or level), string at +0x08
    for (const auto& msg : messages_) {
        oss << "Assembler message at " << msg.code
            << " : " << msg.text << "\n";
    }

    return oss.str();
}

// =============================================================================
// errorMessage — 0x289070
// Appends a message using the current currentLine_, then sets syntax error flag.
// =============================================================================
void AWGAssemblerImpl::errorMessage(const std::string& text) {  // 0x289070
    // Create a Message: {currentLine_, text}
    Message msg;
    msg.code = currentLine_;
    msg.text = text;

    // Push to messages_ vector (at +0x90..+0xA8)
    messages_.push_back(msg);

    // Set syntax error flag
    parserCtx_.setSyntaxError();
}

// =============================================================================
// parserMessage — 0x289190
// Same emit pattern as errorMessage, but the leading int of the Message is
// the `level` argument (severity), not the current line number.
// =============================================================================
void AWGAssemblerImpl::parserMessage(int level, const std::string& text) {  // 0x289190
    Message msg;
    msg.code = level;
    msg.text = text;

    messages_.push_back(msg);
    parserCtx_.setSyntaxError();
}

// =============================================================================
// writeToFile — 0x288570
// Writes the compiled output to an ELF file using ElfWriter.
// Includes sections for code, comment (ostringstream contents), filename, and asm source.
// =============================================================================
void AWGAssemblerImpl::writeToFile(const std::string& outputPath) {  // 0x288570
    // Check preconditions
    if (parserCtx_.hadSyntaxError()) return;
    if (opcodes_.empty()) return;  // no code generated

    // Create ELF writer (machine type 2)
    ElfWriter elf(2);
    elf.setMemoryOffset(memoryOffset_ + 0x80);

    // Add the code section directly — opcodes_ is already vector<uint32_t>.
    // 0x2885cf: lea rsi,[r14+0x50]; 0x2885da: call ElfWriter::addCode.
    elf.addCode(opcodes_);

    // Build comment string
    std::ostringstream commentOss;
    commentOss << "ZI AWG Sequencer Compiler" << " 1.4\n";  // version string
    std::string comment = commentOss.str();

    // Add comment section: data = comment string, section name = ".comment"
    std::string sectionName = ".comment";
    elf.addData(comment.c_str(), comment.size(), sectionName);

    // Add filename section
    std::string filenameStr = filename_;
    std::string basename = boost::filesystem::path(filenameStr).filename().string();
    std::string filenameSec = ".filename";
    elf.addData(basename.c_str(), basename.size(), filenameSec);

    // Add assembly source section
    std::string asmSec = ".asm";
    elf.addData(asmSource_.c_str(), asmSource_.size(), asmSec);

    // Reset opcodes (consumed)
    opcodes_.clear();

    // Write the ELF file
    if (!elf.writeFile(outputPath)) {
        // Failed to write
        throw ZIAWGCompilerException(
            ErrorMessages::format(
                CantWriteFile, std::string(outputPath)));
    }
}

// =============================================================================
// printOpcode — 0x288b50
// Prints all opcodes to stdout with formatting (hex, labels, source lines).
// Parameter 'startIndex' is added to the displayed instruction index.
// =============================================================================
void AWGAssemblerImpl::printOpcode(int startIndex) const {  // 0x288b50
    uint64_t idx = 0;
    size_t numOpcodes = opcodes_.size();

    if (numOpcodes == 0) return;

    while (idx < numOpcodes) {
        // Look up label name from the bimap (right index = program counter)
        std::string labelName;
        auto& labelBimap = getLabelBimap();
        if (labelBimap.right.count(idx) > 0) {
            // Find the label with this index
            auto it = labelBimap.right.find(idx);
            if (it != labelBimap.right.end()) {
                labelName = it->second;  // the string key
            }
        }

        // Print label if found
        if (!labelName.empty()) {
            std::cout << labelName << "\n";
        }

        // Check if this index has a corresponding source line
        size_t numLines = sourceLines_.size();
        if (idx < numLines) {
            // Format: index (hex, padded), ": ", opcode (hex, padded), " ", source_line, "\n"
            std::cout << std::setw(8) << std::setfill('0') << std::hex
                      << (idx + startIndex)
                      << ": "
                      << std::setw(8) << std::setfill('0')
                      << opcodes_[idx]
                      << " " << sourceLines_[idx] << "\n";
        } else {
            // No source line — check if opcode is 0 (padding/NOP)
            if (opcodes_[idx] == 0) {
                std::cout << std::setw(8) << std::setfill('0') << std::hex
                          << (idx + startIndex)
                          << ": "
                          << std::setw(8) << std::setfill('0')
                          << opcodes_[idx]
                          << " " << "nop" << "\n";
            }
        }

        idx++;
    }
}

// AWGAssemblerImpl::extractComment(string const&)
//
// Inlined in the binary into both call sites in
// assembleStringToExpressionsVec @0x286e40 (`$_1` lambda mentioned in the
// call-site comments above). No standalone symbol exists in the binary;
// we provide an out-of-line definition here so the reconstructed
// pipeline TU can link.
//
// Behaviour: locate the first occurrence of `//` and return the
// substring after it. If absent (or `//` is at end of line), return
// empty. The binary's lambda variants behave identically — they don't
// strip whitespace and don't handle `;` (asm comments use `//`).
std::string AWGAssemblerImpl::extractComment(std::string const& line)
{
    auto pos = line.find("//");
    if (pos == std::string::npos) {
        return std::string();
    }
    return line.substr(pos + 2);
}

} // namespace zhinst
