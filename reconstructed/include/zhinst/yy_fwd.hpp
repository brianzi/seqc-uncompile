// ============================================================================
// Forward declarations for flex/bison generated types and functions.
//
// The .c files are compiled as C++ (LANGUAGE CXX in CMake), so
// signatures must match the actual definitions exactly.
// ============================================================================

#ifndef ZHINST_YY_FWD_HPP
#define ZHINST_YY_FWD_HPP

struct yy_buffer_state;

// SeqC lexer/parser (seqc_lexer.c / seqc_parser.tab.c)
namespace zhinst { class SeqcParserContext; class Expression; }
int seqc_lex_init_extra(zhinst::SeqcParserContext* extra, void** scanner);
yy_buffer_state* seqc__scan_string(const char* str, void* scanner);
int seqc_parse(zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner);
void seqc__delete_buffer(yy_buffer_state* buf, void* scanner);
int seqc_lex_destroy(void* scanner);

// Assembler lexer/parser (asm_lexer.c / asm_parser.tab.c)
namespace zhinst { class AsmParserContext; class AsmExpression; }
int asmlex_init_extra(zhinst::AsmParserContext* extra, void** scanner);
yy_buffer_state* asm_scan_string(const char* str, void* scanner);
void asm_delete_buffer(yy_buffer_state* buf, void* scanner);
int asmlex_destroy(void* scanner);
int asmparse(zhinst::AsmParserContext* ctx, zhinst::AsmExpression** result, void* scanner);

#endif // ZHINST_YY_FWD_HPP
