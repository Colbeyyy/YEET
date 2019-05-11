#pragma once

#include "types.h"

bool is_eof(u8 c);
bool is_eol(u8 c);
bool is_whitespace(u8 c);
bool is_whitespace_not_eol(u8 c);
bool is_letter(u8 c);   
bool is_oct_digit(u8 c);
bool is_hex_digit(u8 c);
bool is_number(u8 c);
bool is_symbol(u8 c);
bool is_lowercase(u8 c);
bool is_uppercase(u8 c);
u8 to_lowercase(u8 c);
u8 to_uppercase(u8 c);

#include "array.h"

typedef enum Syntax_Highlight_Type {
    SHT_NONE,
    SHT_COMMENT,
    SHT_IDENT,
    SHT_KEYWORD, 
    SHT_OPERATOR,
    SHT_TYPE,
    SHT_FUNCTION,
    SHT_GENERIC_TYPE,
    SHT_GENERIC_FUNCTION,
    SHT_MACRO,
    SHT_NUMERIC_LITERAL,
    SHT_STRING_LITERAL,
    SHT_DIRECTIVE,
    SHT_ANNOTATION,
} Syntax_Highlight_Type;
typedef struct Syntax_Highlight {
    u64 where;
    u64 size;
    Syntax_Highlight_Type type;
} Syntax_Highlight;
void parse_syntax(Array<Syntax_Highlight> *result, const struct Buffer *buffer, const char *language);
