// ============================================================================
// Forward declarations for flex/bison generated SeqC lexer/parser.
// (seqc_lexer.c / seqc_parser.tab.c, compiled as C++)
// ============================================================================

#pragma once

struct yy_buffer_state;

namespace zhinst { class SeqcParserContext; class Expression; }

int seqc_lex_init_extra(zhinst::SeqcParserContext* extra, void** scanner);
yy_buffer_state* seqc__scan_string(const char* str, void* scanner);
int seqc_parse(zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner);
void seqc__delete_buffer(yy_buffer_state* buf, void* scanner);
int seqc_lex_destroy(void* scanner);
