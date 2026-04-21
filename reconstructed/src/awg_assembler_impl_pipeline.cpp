// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AWGAssemblerImpl — assembly pipeline methods
// ============================================================================

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include <boost/filesystem.hpp>

// Forward declarations for types used
namespace zhinst {

class AsmExpression;
class AsmParserContext;
class ElfWriter;

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
//   +0xA0  bool field_A0          — "noOpt" flag as set by assembleString
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
//   +0x70  uint64_t lineNumber_           — current line being processed
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
        std::string msg = ErrorMessages::format<std::string>(
            static_cast<ErrorMessageT>(0x71), path);
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

    lineNumber_ = 0;

    // Process each line
    while (std::getline(iss, line)) {
        // Parse one line → returns shared_ptr<AsmExpression>
        std::shared_ptr<AsmExpression> ast = getAST(line);
        lineNumber_++;

        if (ast) {
            // Set the noOpt flag from parser context
            ast->noOpt = !parserCtx_.doOpt();

            // Add to expressions vector
            expressions.push_back(ast);

            // If command == 2 (LABEL), record the line number
            if (ast->command == 2) {
                lineNumbers.push_back(lineNumber_);
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

    lineNumber_ = 0;

    while (std::getline(iss, line)) {
        std::shared_ptr<AsmExpression> ast = getAST(line);
        lineNumber_++;

        if (ast) {
            // Has a parsed expression — push source line
            sourceLines_.push_back(line);

            // Extract comment after "//" using $_1 lambda
            std::string comment = extractComment(line);

            // Set comment field on the AST node (at +0x20)
            ast->comment = comment;

            // Set noOpt flag
            ast->noOpt = !parserCtx_.doOpt();

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
// Converts a vector of AssemblerInstr objects into AsmExpression trees and
// calls assembleExpressions.
// =============================================================================
void AWGAssemblerImpl::assembleAsmList(const std::vector<AssemblerInstr>& asmList) {  // 0x2876a0
    std::vector<std::shared_ptr<AsmExpression>> expressions;
    std::vector<uint64_t> lineNumbers;

    parserCtx_.clearSyntaxError();
    parserCtx_.setErrorCallback([this](int code, const std::string& msg) {
        parserMessage(code, msg);
    });

    lineNumber_ = 0;
    int labelCounter = 0;

    for (const auto& instr : asmList) {
        lineNumber_++;

        // Initialize a shared_ptr<AsmExpression> for this instruction
        std::shared_ptr<AsmExpression> expr;

        // Check the command type
        if (instr.cmd == Assembler::ERROR_MSG || instr.cmd == Assembler::MESSAGE) {
            // Skip with a message to cout
            std::cout << "Skipping line: " << lineNumber_
                      << " — instruction not supported in assembleAsmList\n";
            continue;
        }

        if (instr.cmd == Assembler::LABEL) {
            // Create a LABEL expression
            auto exprObj = std::make_shared<AsmExpression>();
            exprObj->command = instr.cmd;
            expr = exprObj;

            // Copy the label string from the instruction
            std::string labelStr = instr.label;

            // Register the label in the parser context
            parserCtx_.Label(labelCounter, labelStr);

            // Set label-related fields on the expression
            exprObj->lineNumber = labelCounter;  // stored at +0x70 (word at label index offset)

            // Depending on labelType (at +0x90 == 1 means "use/ref")
            if (exprObj->labelType == 1) {
                // Copy label name to +0x78
                exprObj->labelName = labelStr;
            } else {
                // Move label name; mark as definition
                exprObj->labelName = std::move(labelStr);
                exprObj->labelType = 1;  // mark as has-label
            }
        } else {
            // General instruction: create expression with matching command
            auto exprObj = std::make_shared<AsmExpression>();
            exprObj->command = instr.cmd;
            expr = exprObj;

            // Add immediate operands as child value expressions
            for (const auto& imm : instr.immediates) {
                int val = static_cast<int>(imm);
                AsmExpression* valExpr = createValue(val);
                std::shared_ptr<AsmExpression> child(valExpr);
                exprObj->children.push_back(std::move(child));
            }
        }

        // Add destination register (reg2 at +0x28 in AssemblerInstr)
        if (instr.reg0.isValid()) {
            int regIdx = static_cast<int>(instr.reg0);
            AsmExpression* regExpr = createRegister(regIdx);
            std::shared_ptr<AsmExpression> child(regExpr);
            expr->children.push_back(std::move(child));
        }

        // Add source register 1 (reg1 at +0x30)
        if (instr.reg1.isValid()) {
            int regIdx = static_cast<int>(instr.reg1);
            AsmExpression* regExpr = createRegister(regIdx);
            std::shared_ptr<AsmExpression> child(regExpr);
            expr->children.push_back(std::move(child));
        }

        // Add source register 0 (reg2 at +0x20 — destination)
        if (instr.reg2.isValid()) {
            int regIdx = static_cast<int>(instr.reg2);
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

        labelCounter++;

        // Record line numbers (for label resolution)
        lineNumbers.push_back(lineNumber_);

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

    // Reset opcodes vector: save old begin, set write ptr = begin
    uint32_t* oldBegin = opcodes_.data();
    uint32_t* oldEnd = opcodes_.data() + opcodes_.capacity();
    opcodes_end_ = opcodes_.data();  // reset write position

    // Ensure capacity for expressions.size() opcodes
    size_t numExprs = expressions.size();
    size_t capacity = (oldEnd - oldBegin) / sizeof(uint32_t);
    if (numExprs > capacity) {
        // Reallocate
        uint32_t* newBuf = new uint32_t[numExprs];
        opcodes_begin_ = newBuf;
        opcodes_end_ = newBuf;
        opcodes_capacity_ = newBuf + numExprs;
        delete[] oldBegin;
    }

    // Clear the label bimap
    auto& labelBimap = *reinterpret_cast<LabelBimap*>(
        reinterpret_cast<char*>(this) + 0xB0);
    labelBimap.clear();

    // First pass: register all labels
    for (const auto& expr : expressions) {
        AsmExpression* e = expr.get();
        if (e->labelType == 1) {  // label definition
            int labelIndex = e->lineNumber;  // at +0x58
            std::string labelName = e->labelName;  // at +0x60
            // Insert into bimap: (name -> index)
            labelBimap.insert({labelName, labelIndex});
        }
    }

    // Second pass: evaluate each expression
    size_t i = 0;
    for (const auto& expr : expressions) {
        // Set line number from lineNumbers vector
        lineNumber_ = lineNumbers[i];

        AsmExpression* e = expr.get();
        if (e->command == 2) {
            // LABEL — skip (no opcode generated)
            i++;
            continue;
        }

        // Evaluate the expression (generates opcode(s))
        int opcode = evaluate(expr);

        // Store opcode
        *opcodes_end_++ = opcode;
        i++;
    }

    // Check for syntax errors after evaluation
    if (parserCtx_.hadSyntaxError()) {
        std::string report = getReport();
        std::cout << report << std::endl;
        throw ZIAWGCompilerException("Syntax error in assembly source");
    }

    // Append trailing 0 (END/NOP) if last opcode is non-zero
    if (opcodes_end_ != opcodes_begin_ && *(opcodes_end_ - 1) != 0) {
        // Push a 0
        *opcodes_end_++ = 0;
    }
}

// =============================================================================
// evaluate — 0x285b20
// Dispatches to opcode0..opcode5 based on getOpcodeType(command).
// =============================================================================
int AWGAssemblerImpl::evaluate(const std::shared_ptr<AsmExpression>& expr) {  // 0x285b20
    AsmExpression* e = expr.get();
    if (!e || e->unknown0 == 0) {
        return 0;  // null or empty expression
    }

    int cmd = e->command;
    if (cmd == -1) {
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
    // Each message is 0x20 bytes: uint64_t lineNo at +0x00, string at +0x08
    for (const auto& msg : messages_) {
        oss << "Assembler message at " << msg.lineNumber
            << " : " << msg.text << "\n";
    }

    return oss.str();
}

// =============================================================================
// errorMessage — 0x289070
// Appends a message using the current lineNumber_, then sets syntax error flag.
// =============================================================================
void AWGAssemblerImpl::errorMessage(const std::string& text) {  // 0x289070
    // Create a Message: {lineNumber_, text}
    Message msg;
    msg.lineNumber = lineNumber_;
    msg.text = text;

    // Push to messages_ vector (at +0x90..+0xA8)
    messages_.push_back(msg);

    // Set syntax error flag
    parserCtx_.setSyntaxError();
}

// =============================================================================
// parserMessage — 0x289190
// Same as errorMessage but takes an explicit line number (from parser callback).
// =============================================================================
void AWGAssemblerImpl::parserMessage(int line, const std::string& text) {  // 0x289190
    Message msg;
    msg.lineNumber = static_cast<uint64_t>(line);
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
    if (opcodes_begin_ == opcodes_end_) return;  // no code generated

    // Create ELF writer (machine type 2)
    ElfWriter elf(2);
    elf.setMemoryOffset(memoryOffset_ + 0x80);

    // Add the code section from opcodes vector
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

    // Reset opcodes_end_ to begin (consumed)
    opcodes_end_ = opcodes_begin_;

    // Write the ELF file
    if (!elf.writeFile(outputPath)) {
        // Failed to write
        throw ZIAWGCompilerException(
            ErrorMessages::format<std::string>(
                static_cast<ErrorMessageT>(0x94), outputPath));
    }
}

// =============================================================================
// printOpcode — 0x288b50
// Prints all opcodes to stdout with formatting (hex, labels, source lines).
// Parameter 'startIndex' is added to the displayed instruction index.
// =============================================================================
void AWGAssemblerImpl::printOpcode(int startIndex) const {  // 0x288b50
    uint64_t idx = 0;
    size_t numOpcodes = opcodes_end_ - opcodes_begin_;

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
        size_t numLines = (sourceLines_end_ - sourceLines_begin_) / 3;  // string is 0x18
        if (idx < numLines) {
            // Format: index (hex, padded), ": ", opcode (hex, padded), " ", source_line, "\n"
            std::cout << std::setw(8) << std::setfill('0') << std::hex
                      << (idx + startIndex)
                      << ": "
                      << std::setw(8) << std::setfill('0')
                      << opcodes_begin_[idx]
                      << " " << sourceLines_[idx] << "\n";
        } else {
            // No source line — check if opcode is 0 (padding/NOP)
            if (opcodes_begin_[idx] == 0) {
                std::cout << std::setw(8) << std::setfill('0') << std::hex
                          << (idx + startIndex)
                          << ": "
                          << std::setw(8) << std::setfill('0')
                          << opcodes_begin_[idx]
                          << " " << "nop" << "\n";
            }
        }

        idx++;
    }
}

} // namespace zhinst
