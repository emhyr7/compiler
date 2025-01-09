#include "compiler.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlogical-op-parentheses"
#pragma GCC diagnostic ignored "-Wbitwise-op-parentheses"
#pragma GCC diagnostic ignored "-Winitializer-overrides"

typedef __builtin_va_list VARGS;
#define get_vargs(...) __builtin_va_start(__VA_ARGS__)
#define end_vargs(...) __builtin_va_end(__VA_ARGS__)

#define KIBIBYTES(n) (n << 10)
#define MEBIBYTES(n) (KIBIBYTES(n) << 10)
#define GIBIBYTES(n) (MEBIBYTES(n) << 10)

static inline VOID copy(VOID *restrict destination, const VOID *restrict source, SIZE size) {
	(void)__builtin_memcpy(destination, source, size);
}

static inline VOID move(VOID *destination, const VOID *source, SIZE size) {
	(void)__builtin_memmove(destination, source, size);
}

static inline VOID fill(VOID *destination, WORD byte, SIZE size) {
	(void)__builtin_memset(destination, byte, size);
}

static inline SIZE get_size_of_string(const VOID *source) {
	return __builtin_strlen(source);
}

static inline SIZE get_forward_alignment(ADDRESS address, SIZE alignment) {
	SIZE remainder = alignment ? address & (alignment - 1) : 0;
	return remainder ? alignment - remainder : 0;
}

static inline ADDRESS align_forwards(ADDRESS address, SIZE alignment) {
	return address + get_forward_alignment(address, alignment);
}

struct BUFFER {
	SIZE reservation_size;
	SIZE commission_rate;
	VOID *data;
	SIZE data_size;
	SIZE commission_size;
};

typedef struct BUFFER C_BUFFER;

#define DEFAULT_BUFFER (struct BUFFER){ .reservation_size = 0, .commission_rate = 0, .data = 0 }

static VOID *push(SIZE size, SIZE alignment, struct BUFFER *buffer) {
	assert(alignment % 2 == 0);
	if (!buffer->data) {
		if (!buffer->reservation_size) buffer->reservation_size = GIBIBYTES(1);
		if (!buffer->commission_rate) buffer->commission_rate = query_system_page_size();
		buffer->data = reserve_virtual_memory(buffer->reservation_size);
		commit_virtual_memory(buffer->data, buffer->commission_rate);
		buffer->commission_size = buffer->commission_rate;
		buffer->data_size = 0;
	}
	SIZE forward_alignment = get_forward_alignment((ADDRESS)buffer->data, alignment);
	if (buffer->data_size + forward_alignment + size >= buffer->commission_size) {
		assert(buffer->commission_size + buffer->commission_rate <= buffer->reservation_size);
		commit_virtual_memory((BYTE *)buffer->data + buffer->commission_size, buffer->commission_rate);
		buffer->commission_size += buffer->commission_rate;
	}
	buffer->data_size += forward_alignment;
	VOID *result = buffer->data + buffer->data_size;
	buffer->data_size += size;
	fill(result, 0, size);
	return result;
}

typedef U32 COUNT;

typedef BYTE UTF8;
typedef WORD UTF32;

/* from https://bjoern.hoehrmann.de/utf-8/decoder/dfa/ */
enum UTF8_STATE {
	UTF8_STATE_accept = 0,
	UTF8_STATE_reject = 1,
};

static WORD decode_utf8_(enum UTF8_STATE *state, UTF32 *codepoint, UTF8 byte) {
	static const BYTE table[] = {
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3,
		0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,
		0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1,
		1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,
		1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1,
		1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	};

	WORD type = table[byte];
	*codepoint = *state != UTF8_STATE_accept ? byte & 0x3fu | *codepoint << 6 : 0xff >> type & byte;
	*state = table[256 + *state * 16 + type];
	return *state;
}

struct UNICODE_DECODING {
	UTF32 codepoint;
	WORD increment;
};

#define UNICODE_REPLACEMENT_CHARACTER 0xfffd

static struct UNICODE_DECODING decode_utf8(const UTF8 bytes[4]) {
	UTF32 codepoint;
	enum UTF8_STATE prior_state = 0;
	for (enum UTF8_STATE state = UTF8_STATE_accept;; prior_state = state) {
		switch (decode_utf8_(&state, &codepoint, *bytes++)) {
		case UTF8_STATE_accept:
			goto finished;
		case UTF8_STATE_reject:
			codepoint = UNICODE_REPLACEMENT_CHARACTER;
			goto finished;
		default:
			break;
		}
	}
finished:
	return (struct UNICODE_DECODING){ .codepoint = codepoint, .increment = prior_state / 3 + 1 };
}

#define MAXIMUM_PATH_SIZE 255

struct SOURCE {
	CHAR path[MAXIMUM_PATH_SIZE + 1];
	CHAR *data;
	COUNT size;
};

static struct SOURCE load_source(const CHAR *path) {
	assert(get_size_of_string(path) <= MAXIMUM_PATH_SIZE);

	HANDLE file = open_file(path);
	SIZE size = get_size_of_file(file);
	assert(size < (COUNT)-1);
	VOID *data = allocate_virtual_memory(size + sizeof(UTF32));
	(void)read_from_file(data, size, file);

	struct SOURCE source = { .data = data, .size = size };
	copy(source.path, path, get_size_of_string(path));
	return source;
}

enum CHARACTER {
	CHARACTER_unknown,
	CHARACTER_whitespace,
	CHARACTER_terminator,
	CHARACTER_full_stop,            /* . */
	CHARACTER_low_line,             /* _ */
	CHARACTER_zero,                 /* 0 */
	CHARACTER_binadigit,            /* 1 */
	CHARACTER_octadigit,            /* 2 3 4 5 6 7 */
	CHARACTER_digit,                /* 8 9 */
	CHARACTER_b,                    /* B b */
	CHARACTER_hexadigit,            /* A C D E F a c d e f */
	CHARACTER_x,                    /* X x */
	CHARACTER_letter,               /* G H I J K L M N O P Q R S T U V W Y Z g h i j k l m n o p q r s t u v w y z */
	CHARACTER_quotation_mark,       /* " */
	CHARACTER_backslash,            /* \ */
	CHARACTER_ampersand,            /* & */
	CHARACTER_apostrophe,           /* ' */
	CHARACTER_asterisk,             /* * */
	CHARACTER_at_sign,              /* @ */
	CHARACTER_circumflex_accent,    /* ^ */
	CHARACTER_colon,                /* : */
	CHARACTER_comma,                /* , */
	CHARACTER_dollar_sign,          /* $ */
	CHARACTER_equal_sign,           /* = */
	CHARACTER_exclamation_mark,     /* ! */
	CHARACTER_grave_accent,         /* ` */
	CHARACTER_greaterthan_sign,    /* > */
	CHARACTER_hyphenminus,          /* - */
	CHARACTER_left_curly_bracket,   /* { */
	CHARACTER_left_parenthesis,     /* ( */
	CHARACTER_left_square_bracket,  /* [ */
	CHARACTER_lessthan_sign,       /* < */
	CHARACTER_octothorpe,           /* # */
	CHARACTER_percent_sign,         /* % */
	CHARACTER_plus_sign,            /* + */
	CHARACTER_question_mark,        /* ? */
	CHARACTER_right_curly_bracket,  /* } */
	CHARACTER_right_parenthesis,    /* ) */
	CHARACTER_right_square_bracket, /* ) */
	CHARACTER_semicolon,            /* : */
	CHARACTER_slash,                /* / */
	CHARACTER_tilde,                /* ~ */
	CHARACTER_vertical_bar,         /* | */

	CHARACTERS_COUNT,
};

struct LEXER {
	struct SOURCE source;
	SIZE position;
	SIZE row;
	SIZE column;
	SIZE increment;
	enum CHARACTER character;
};

#define MAXIMUM_CODEPOINT_VALUE 255

/* NOTE(Emhyr): i like tables... */

static const enum CHARACTER character_from_codepoint[MAXIMUM_CODEPOINT_VALUE] = {
	[0 ...MAXIMUM_CODEPOINT_VALUE - 1] = CHARACTER_unknown,
	['\t'] = CHARACTER_whitespace,
	['\n'] = CHARACTER_whitespace,
	['\v'] = CHARACTER_whitespace,
	['\f'] = CHARACTER_whitespace,
	['\r'] = CHARACTER_whitespace,
	[' ' ] = CHARACTER_whitespace,
	['.'] = CHARACTER_full_stop,
	['_'] = CHARACTER_low_line,
	['0'] = CHARACTER_zero,
	['1'] = CHARACTER_binadigit,
	['2'] = CHARACTER_octadigit,
	['3'] = CHARACTER_octadigit,
	['4'] = CHARACTER_octadigit,
	['5'] = CHARACTER_octadigit,
	['6'] = CHARACTER_octadigit,
	['7'] = CHARACTER_octadigit,
	['8'] = CHARACTER_digit,
	['9'] = CHARACTER_digit,
	['B'] = CHARACTER_b,
	['b'] = CHARACTER_b,
	['A'] = CHARACTER_hexadigit,
	['C'] = CHARACTER_hexadigit,
	['D'] = CHARACTER_hexadigit,
	['E'] = CHARACTER_hexadigit,
	['F'] = CHARACTER_hexadigit,
	['a'] = CHARACTER_hexadigit,
	['c'] = CHARACTER_hexadigit,
	['d'] = CHARACTER_hexadigit,
	['e'] = CHARACTER_hexadigit,
	['f'] = CHARACTER_hexadigit,
	['X'] = CHARACTER_x,
	['x'] = CHARACTER_x,
	['G'] = CHARACTER_letter,
	['H'] = CHARACTER_letter,
	['I'] = CHARACTER_letter,
	['J'] = CHARACTER_letter,
	['K'] = CHARACTER_letter,
	['L'] = CHARACTER_letter,
	['M'] = CHARACTER_letter,
	['N'] = CHARACTER_letter,
	['O'] = CHARACTER_letter,
	['P'] = CHARACTER_letter,
	['Q'] = CHARACTER_letter,
	['R'] = CHARACTER_letter,
	['S'] = CHARACTER_letter,
	['T'] = CHARACTER_letter,
	['U'] = CHARACTER_letter,
	['V'] = CHARACTER_letter,
	['W'] = CHARACTER_letter,
	['Y'] = CHARACTER_letter,
	['Z'] = CHARACTER_letter,
	['g'] = CHARACTER_letter,
	['h'] = CHARACTER_letter,
	['i'] = CHARACTER_letter,
	['j'] = CHARACTER_letter,
	['k'] = CHARACTER_letter,
	['l'] = CHARACTER_letter,
	['m'] = CHARACTER_letter,
	['n'] = CHARACTER_letter,
	['o'] = CHARACTER_letter,
	['p'] = CHARACTER_letter,
	['q'] = CHARACTER_letter,
	['r'] = CHARACTER_letter,
	['s'] = CHARACTER_letter,
	['t'] = CHARACTER_letter,
	['u'] = CHARACTER_letter,
	['v'] = CHARACTER_letter,
	['w'] = CHARACTER_letter,
	['y'] = CHARACTER_letter,
	['z'] = CHARACTER_letter,
	['"' ] = CHARACTER_quotation_mark,
	['\\'] = CHARACTER_backslash,
	['&' ] = CHARACTER_ampersand,
	['\''] = CHARACTER_apostrophe,
	['*' ] = CHARACTER_asterisk,
	['@' ] = CHARACTER_at_sign,
	['^' ] = CHARACTER_circumflex_accent,
	[':' ] = CHARACTER_colon,
	[',' ] = CHARACTER_comma,
	['$' ] = CHARACTER_dollar_sign,
	['=' ] = CHARACTER_equal_sign,
	['!' ] = CHARACTER_exclamation_mark,
	['.' ] = CHARACTER_full_stop,
	['`' ] = CHARACTER_grave_accent,
	['>' ] = CHARACTER_greaterthan_sign,
	['-' ] = CHARACTER_hyphenminus,
	['{' ] = CHARACTER_left_curly_bracket,
	['(' ] = CHARACTER_left_parenthesis,
	['[' ] = CHARACTER_left_square_bracket,
	['<' ] = CHARACTER_lessthan_sign,
	['#' ] = CHARACTER_octothorpe,
	['%' ] = CHARACTER_percent_sign,
	['+' ] = CHARACTER_plus_sign,
	['?' ] = CHARACTER_question_mark,
	['}' ] = CHARACTER_right_curly_bracket,
	[')' ] = CHARACTER_right_parenthesis,
	[']' ] = CHARACTER_right_square_bracket,
	[';' ] = CHARACTER_semicolon,
	['/' ] = CHARACTER_slash,
	['~' ] = CHARACTER_tilde,
	['|' ] = CHARACTER_vertical_bar,
};

struct RANGE {
	COUNT beginning;
	COUNT ending;
	COUNT row;
	COUNT column;
};

enum TOKEN_TAG {
	TOKEN_TAG_undefined,
	TOKEN_TAG_terminator,
	TOKEN_TAG_word,
	TOKEN_TAG_binary,
	TOKEN_TAG_octal,
	TOKEN_TAG_digital,
	TOKEN_TAG_hexadecimal,
	TOKEN_TAG_decimal,
	TOKEN_TAG_text,
	TOKEN_TAG_ampersand,
	TOKEN_TAG_ampersand_2,
	TOKEN_TAG_ampersand_equal_sign,
	TOKEN_TAG_apostrophe,
	TOKEN_TAG_asterisk,
	TOKEN_TAG_asterisk_equal_sign,
	TOKEN_TAG_at_sign,
	TOKEN_TAG_backslash,
	TOKEN_TAG_circumflex_accent,
	TOKEN_TAG_circumflex_accent_equal_sign,
	TOKEN_TAG_colon,
	TOKEN_TAG_comma,
	TOKEN_TAG_dollar_sign,
	TOKEN_TAG_equal_sign,
	TOKEN_TAG_equal_sign_2,
	TOKEN_TAG_exclamation_mark,
	TOKEN_TAG_exclamation_mark_equal_sign,
	TOKEN_TAG_full_stop,
	TOKEN_TAG_grave_accent,
	TOKEN_TAG_greaterthan_sign,
	TOKEN_TAG_greaterthan_sign_2,
	TOKEN_TAG_greaterthan_sign_2_equal_sign,
	TOKEN_TAG_greaterthan_sign_equal_sign,
	TOKEN_TAG_hyphenminus,
	TOKEN_TAG_hyphenminus_equal_sign,
	TOKEN_TAG_hyphenminus_greaterthan_sign,
	TOKEN_TAG_left_curly_bracket,
	TOKEN_TAG_left_parenthesis,
	TOKEN_TAG_left_square_bracket,
	TOKEN_TAG_lessthan_sign,
	TOKEN_TAG_lessthan_sign_2,
	TOKEN_TAG_lessthan_sign_2_equal_sign,
	TOKEN_TAG_lessthan_sign_equal_sign,
	TOKEN_TAG_octothorpe,
	TOKEN_TAG_percent_sign,
	TOKEN_TAG_percent_sign_equal_sign,
	TOKEN_TAG_plus_sign,
	TOKEN_TAG_plus_sign_equal_sign,
	TOKEN_TAG_question_mark,
	TOKEN_TAG_right_curly_bracket,
	TOKEN_TAG_right_parenthesis,
	TOKEN_TAG_right_square_bracket,
	TOKEN_TAG_semicolon,
	TOKEN_TAG_slash,
	TOKEN_TAG_slash_equal_sign,
	TOKEN_TAG_tilde,
	TOKEN_TAG_vertical_bar,
	TOKEN_TAG_vertical_bar_2,
	TOKEN_TAG_vertical_bar_equal_sign,
	TOKEN_TAGS_COUNT
};

struct TOKEN {
	enum TOKEN_TAG tag;
	struct RANGE range;
};

enum LEXER_STATE {
	LEXER_STATE_initial = TOKEN_TAGS_COUNT,
	LEXER_STATE_on_unknown,
	LEXER_STATE_on_word,
	LEXER_STATE_on_zero,
	LEXER_STATE_on_decimal,
	LEXER_STATE_on_octal,
	LEXER_STATE_on_binary,
	LEXER_STATE_on_hexadecimal,
	LEXER_STATE_on_digital,
	LEXER_STATE_on_string,
	LEXER_STATE_on_string_escape,
	LEXER_STATE_on_string_terminate,
	LEXER_STATE_on_ampersand,
	LEXER_STATE_on_ampersand_2,
	LEXER_STATE_on_ampersand_equal_sign,
	LEXER_STATE_on_apostrophe,
	LEXER_STATE_on_asterisk,
	LEXER_STATE_on_asterisk_equal_sign,
	LEXER_STATE_on_at_sign,
	LEXER_STATE_on_backslash,
	LEXER_STATE_on_circumflex_accent,
	LEXER_STATE_on_circumflex_accent_equal_sign,
	LEXER_STATE_on_colon,
	LEXER_STATE_on_comma,
	LEXER_STATE_on_dollar_sign,
	LEXER_STATE_on_equal_sign,
	LEXER_STATE_on_equal_sign_2,
	LEXER_STATE_on_exclamation_mark,
	LEXER_STATE_on_exclamation_mark_equal_sign,
	LEXER_STATE_on_full_stop,
	LEXER_STATE_on_grave_accent,
	LEXER_STATE_on_greaterthan_sign,
	LEXER_STATE_on_greaterthan_sign_2,
	LEXER_STATE_on_greaterthan_sign_2_equal_sign,
	LEXER_STATE_on_greaterthan_sign_equal_sign,
	LEXER_STATE_on_hyphenminus,
	LEXER_STATE_on_hyphenminus_equal_sign,
	LEXER_STATE_on_hyphenminus_greaterthan_sign,
	LEXER_STATE_on_left_curly_bracket,
	LEXER_STATE_on_left_parenthesis,
	LEXER_STATE_on_left_square_bracket,
	LEXER_STATE_on_lessthan_sign,
	LEXER_STATE_on_lessthan_sign_2,
	LEXER_STATE_on_lessthan_sign_2_equal_sign,
	LEXER_STATE_on_lessthan_sign_equal_sign,
	LEXER_STATE_on_octothorpe,
	LEXER_STATE_on_percent_sign,
	LEXER_STATE_on_percent_sign_equal_sign,
	LEXER_STATE_on_plus_sign,
	LEXER_STATE_on_plus_sign_equal_sign,
	LEXER_STATE_on_question_mark,
	LEXER_STATE_on_right_curly_bracket,
	LEXER_STATE_on_right_parenthesis,
	LEXER_STATE_on_right_square_bracket,
	LEXER_STATE_on_semicolon,
	LEXER_STATE_on_slash,
	LEXER_STATE_on_slash_equal_sign,
	LEXER_STATE_on_tilde,
	LEXER_STATE_on_vertical_bar,
	LEXER_STATE_on_vertical_bar_2,
	LEXER_STATE_on_vertical_bar_equal_sign,
	LEXER_STATES_COUNT
};

/* NOTE(Emhyr): i like tables... */

static const WORD lexer_state_from_character[LEXER_STATES_COUNT][CHARACTERS_COUNT] = {
	[LEXER_STATE_initial                          ][0 ...CHARACTERS_COUNT - 1                ] = LEXER_STATE_on_unknown,
	[LEXER_STATE_on_unknown                       ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_undefined,
	[LEXER_STATE_initial                          ][CHARACTER_terminator                     ] = TOKEN_TAG_terminator,

	[LEXER_STATE_initial                          ][CHARACTER_low_line                       ] = LEXER_STATE_on_word,
	[LEXER_STATE_initial                          ][CHARACTER_b ...CHARACTER_letter          ] = LEXER_STATE_on_word,
	[LEXER_STATE_on_word                          ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_word,
	[LEXER_STATE_on_word                          ][CHARACTER_low_line ...CHARACTER_letter   ] = LEXER_STATE_on_word,

	[LEXER_STATE_initial                          ][CHARACTER_zero                           ] = LEXER_STATE_on_zero,
	[LEXER_STATE_on_zero                          ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_digital,
	[LEXER_STATE_on_zero                          ][CHARACTER_full_stop                      ] = LEXER_STATE_on_decimal,
	[LEXER_STATE_on_zero                          ][CHARACTER_low_line ...CHARACTER_octadigit] = LEXER_STATE_on_octal,
	[LEXER_STATE_on_zero                          ][CHARACTER_b                              ] = LEXER_STATE_on_binary,
	[LEXER_STATE_on_zero                          ][CHARACTER_x                              ] = LEXER_STATE_on_hexadecimal,
	[LEXER_STATE_on_decimal                       ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_decimal,
	[LEXER_STATE_on_decimal                       ][CHARACTER_low_line ...CHARACTER_digit    ] = LEXER_STATE_on_decimal,
	[LEXER_STATE_on_octal                         ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_octal,
	[LEXER_STATE_on_octal                         ][CHARACTER_low_line ...CHARACTER_octadigit] = LEXER_STATE_on_octal,
	[LEXER_STATE_on_binary                        ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_binary,
	[LEXER_STATE_on_binary                        ][CHARACTER_low_line ...CHARACTER_binadigit] = LEXER_STATE_on_binary,
	[LEXER_STATE_on_hexadecimal                   ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_hexadecimal,
	[LEXER_STATE_on_hexadecimal                   ][CHARACTER_low_line ...CHARACTER_hexadigit] = LEXER_STATE_on_hexadecimal,
	[LEXER_STATE_initial                          ][CHARACTER_binadigit ...CHARACTER_digit   ] = LEXER_STATE_on_digital,
	[LEXER_STATE_on_digital                       ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_digital,
	[LEXER_STATE_on_digital                       ][CHARACTER_full_stop                      ] = LEXER_STATE_on_decimal,
 	[LEXER_STATE_on_digital                       ][CHARACTER_low_line ...CHARACTER_digit    ] = LEXER_STATE_on_digital,

 	[LEXER_STATE_initial                          ][CHARACTER_quotation_mark                 ] = LEXER_STATE_on_string,
 	[LEXER_STATE_on_string                        ][0 ...CHARACTERS_COUNT - 1                ] = LEXER_STATE_on_string,
 	[LEXER_STATE_on_string                        ][CHARACTER_terminator                     ] = TOKEN_TAG_text,
 	[LEXER_STATE_on_string                        ][CHARACTER_backslash                      ] = LEXER_STATE_on_string_escape,
 	[LEXER_STATE_on_string                        ][CHARACTER_quotation_mark                 ] = LEXER_STATE_on_string_terminate,
 	[LEXER_STATE_on_string_escape                 ][0 ...CHARACTERS_COUNT - 1                ] = LEXER_STATE_on_string,
 	[LEXER_STATE_on_string_terminate              ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_text,

 	[LEXER_STATE_initial                          ][CHARACTER_ampersand                      ] = LEXER_STATE_on_ampersand,
 	[LEXER_STATE_on_ampersand                     ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_ampersand,
 	[LEXER_STATE_on_ampersand                     ][CHARACTER_ampersand                      ] = LEXER_STATE_on_ampersand_2,
 	[LEXER_STATE_on_ampersand                     ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_ampersand_equal_sign,
 	[LEXER_STATE_on_ampersand_2                   ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_ampersand_2,
 	[LEXER_STATE_on_ampersand_equal_sign          ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_ampersand_equal_sign,
 	[LEXER_STATE_initial                          ][CHARACTER_apostrophe                     ] = LEXER_STATE_on_apostrophe,
 	[LEXER_STATE_on_apostrophe                    ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_apostrophe,
 	[LEXER_STATE_initial                          ][CHARACTER_asterisk                       ] = LEXER_STATE_on_asterisk,
 	[LEXER_STATE_on_asterisk                      ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_asterisk,
 	[LEXER_STATE_on_asterisk                      ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_asterisk_equal_sign,
 	[LEXER_STATE_on_asterisk_equal_sign           ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_asterisk_equal_sign,
	[LEXER_STATE_initial                          ][CHARACTER_at_sign                        ] = LEXER_STATE_on_at_sign,
	[LEXER_STATE_on_at_sign                       ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_at_sign,
	[LEXER_STATE_initial                          ][CHARACTER_backslash                      ] = LEXER_STATE_on_backslash,
	[LEXER_STATE_on_backslash                     ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_backslash,
	[LEXER_STATE_initial                          ][CHARACTER_circumflex_accent              ] = LEXER_STATE_on_circumflex_accent,
	[LEXER_STATE_on_circumflex_accent             ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_circumflex_accent,
	[LEXER_STATE_on_circumflex_accent             ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_circumflex_accent_equal_sign,
	[LEXER_STATE_on_circumflex_accent_equal_sign  ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_circumflex_accent_equal_sign,
	[LEXER_STATE_initial                          ][CHARACTER_colon                          ] = LEXER_STATE_on_colon,
	[LEXER_STATE_on_colon                         ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_colon,
	[LEXER_STATE_initial                          ][CHARACTER_comma                          ] = LEXER_STATE_on_comma,
	[LEXER_STATE_on_comma                         ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_comma,
	[LEXER_STATE_initial                          ][CHARACTER_dollar_sign                    ] = LEXER_STATE_on_dollar_sign,
	[LEXER_STATE_on_dollar_sign                   ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_dollar_sign,
	[LEXER_STATE_initial                          ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_equal_sign,
	[LEXER_STATE_on_equal_sign                    ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_equal_sign,
	[LEXER_STATE_on_equal_sign                    ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_equal_sign_2,
	[LEXER_STATE_on_equal_sign_2                  ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_equal_sign_2,
	[LEXER_STATE_initial                          ][CHARACTER_exclamation_mark               ] = LEXER_STATE_on_exclamation_mark,
	[LEXER_STATE_on_exclamation_mark              ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_exclamation_mark,
	[LEXER_STATE_on_exclamation_mark              ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_exclamation_mark_equal_sign,
	[LEXER_STATE_on_exclamation_mark_equal_sign   ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_exclamation_mark_equal_sign,
	[LEXER_STATE_initial                          ][CHARACTER_full_stop                      ] = LEXER_STATE_on_full_stop,
	[LEXER_STATE_on_full_stop                     ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_full_stop,
	[LEXER_STATE_initial                          ][CHARACTER_grave_accent                   ] = LEXER_STATE_on_grave_accent,
	[LEXER_STATE_on_grave_accent                  ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_grave_accent,
	[LEXER_STATE_initial                          ][CHARACTER_greaterthan_sign               ] = LEXER_STATE_on_greaterthan_sign,
	[LEXER_STATE_on_greaterthan_sign              ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_greaterthan_sign,
	[LEXER_STATE_on_greaterthan_sign              ][CHARACTER_greaterthan_sign               ] = LEXER_STATE_on_greaterthan_sign_2,
	[LEXER_STATE_on_greaterthan_sign_2            ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_greaterthan_sign_2,
	[LEXER_STATE_on_greaterthan_sign_2            ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_greaterthan_sign_2_equal_sign,
	[LEXER_STATE_on_greaterthan_sign_2_equal_sign ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_greaterthan_sign_2_equal_sign,
	[LEXER_STATE_on_greaterthan_sign              ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_greaterthan_sign_equal_sign,
	[LEXER_STATE_on_greaterthan_sign_equal_sign   ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_greaterthan_sign_equal_sign,
	[LEXER_STATE_initial                          ][CHARACTER_hyphenminus                    ] = LEXER_STATE_on_hyphenminus,
	[LEXER_STATE_on_hyphenminus                   ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_hyphenminus,
	[LEXER_STATE_on_hyphenminus                   ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_hyphenminus_equal_sign,
	[LEXER_STATE_on_hyphenminus_equal_sign        ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_hyphenminus_equal_sign,
	[LEXER_STATE_on_hyphenminus                   ][CHARACTER_greaterthan_sign               ] = LEXER_STATE_on_hyphenminus_greaterthan_sign,
	[LEXER_STATE_on_hyphenminus_greaterthan_sign  ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_hyphenminus_greaterthan_sign,
	[LEXER_STATE_initial                          ][CHARACTER_left_curly_bracket             ] = LEXER_STATE_on_left_curly_bracket,
	[LEXER_STATE_on_left_curly_bracket            ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_left_curly_bracket,
	[LEXER_STATE_initial                          ][CHARACTER_left_parenthesis               ] = LEXER_STATE_on_left_parenthesis,
	[LEXER_STATE_on_left_parenthesis              ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_left_parenthesis,
	[LEXER_STATE_initial                          ][CHARACTER_left_square_bracket            ] = LEXER_STATE_on_left_square_bracket,
	[LEXER_STATE_on_left_square_bracket           ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_left_square_bracket,
	[LEXER_STATE_initial                          ][CHARACTER_lessthan_sign                  ] = LEXER_STATE_on_lessthan_sign,
	[LEXER_STATE_on_lessthan_sign                 ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_lessthan_sign,
	[LEXER_STATE_on_lessthan_sign                 ][CHARACTER_lessthan_sign                  ] = LEXER_STATE_on_lessthan_sign_2,
	[LEXER_STATE_on_lessthan_sign_2               ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_lessthan_sign_2,
	[LEXER_STATE_on_lessthan_sign_2               ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_lessthan_sign_2_equal_sign,
	[LEXER_STATE_on_lessthan_sign_2_equal_sign    ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_lessthan_sign_2_equal_sign,
	[LEXER_STATE_on_lessthan_sign                 ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_lessthan_sign_equal_sign,
	[LEXER_STATE_on_lessthan_sign_equal_sign      ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_lessthan_sign_equal_sign,
	[LEXER_STATE_initial                          ][CHARACTER_octothorpe                     ] = LEXER_STATE_on_octothorpe,
	[LEXER_STATE_on_octothorpe                    ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_octothorpe,
	[LEXER_STATE_initial                          ][CHARACTER_percent_sign                   ] = LEXER_STATE_on_percent_sign,
	[LEXER_STATE_on_percent_sign                  ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_percent_sign,
	[LEXER_STATE_on_percent_sign                  ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_percent_sign_equal_sign,
	[LEXER_STATE_on_percent_sign_equal_sign       ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_percent_sign_equal_sign,
	[LEXER_STATE_initial                          ][CHARACTER_plus_sign                      ] = LEXER_STATE_on_plus_sign,
	[LEXER_STATE_on_plus_sign                     ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_plus_sign,
	[LEXER_STATE_on_plus_sign                     ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_plus_sign_equal_sign,
	[LEXER_STATE_on_plus_sign_equal_sign          ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_plus_sign_equal_sign,
	[LEXER_STATE_initial                          ][CHARACTER_question_mark                  ] = LEXER_STATE_on_question_mark,
	[LEXER_STATE_on_question_mark                 ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_question_mark,
	[LEXER_STATE_initial                          ][CHARACTER_right_curly_bracket            ] = LEXER_STATE_on_right_curly_bracket,
	[LEXER_STATE_on_right_curly_bracket           ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_right_curly_bracket,
	[LEXER_STATE_initial                          ][CHARACTER_right_parenthesis              ] = LEXER_STATE_on_right_parenthesis,
	[LEXER_STATE_on_right_parenthesis             ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_right_parenthesis,
	[LEXER_STATE_initial                          ][CHARACTER_right_square_bracket           ] = LEXER_STATE_on_right_square_bracket,
	[LEXER_STATE_on_right_square_bracket          ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_right_square_bracket,
	[LEXER_STATE_initial                          ][CHARACTER_semicolon                      ] = LEXER_STATE_on_semicolon,
	[LEXER_STATE_on_semicolon                     ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_semicolon,
	[LEXER_STATE_initial                          ][CHARACTER_slash                          ] = LEXER_STATE_on_slash,
	[LEXER_STATE_on_slash                         ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_slash,
	[LEXER_STATE_on_slash                         ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_slash_equal_sign,
	[LEXER_STATE_on_slash_equal_sign              ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_slash_equal_sign,
	[LEXER_STATE_initial                          ][CHARACTER_tilde                          ] = LEXER_STATE_on_tilde,
	[LEXER_STATE_on_tilde                         ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_tilde,
	[LEXER_STATE_initial                          ][CHARACTER_vertical_bar                   ] = LEXER_STATE_on_vertical_bar,
	[LEXER_STATE_on_vertical_bar                  ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_vertical_bar,
	[LEXER_STATE_on_vertical_bar                  ][CHARACTER_vertical_bar                   ] = LEXER_STATE_on_vertical_bar_2,
	[LEXER_STATE_on_vertical_bar_2                ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_vertical_bar_2,
	[LEXER_STATE_on_vertical_bar                  ][CHARACTER_equal_sign                     ] = LEXER_STATE_on_vertical_bar_equal_sign,
	[LEXER_STATE_on_vertical_bar_equal_sign       ][0 ...CHARACTERS_COUNT - 1                ] = TOKEN_TAG_vertical_bar_equal_sign
};

static VOID advance_lexer(struct LEXER *lexer) {
	lexer->position += lexer->increment;
	if (lexer->character == '\n') {
		++lexer->row;
		lexer->column = 0;
	}
	++lexer->column;

	if (lexer->position < lexer->source.size) {
		const UTF8 bytes[4] = {
			lexer->source.data[lexer->position + 0],
			lexer->source.data[lexer->position + 1],
			lexer->source.data[lexer->position + 2],
			lexer->source.data[lexer->position + 3]
		};
		struct UNICODE_DECODING decoding = decode_utf8(bytes);
		lexer->increment = decoding.increment;
		lexer->character = decoding.codepoint <= MAXIMUM_CODEPOINT_VALUE ? character_from_codepoint[decoding.codepoint] : CHARACTER_unknown;
	} else {
		lexer->increment = 0;
		lexer->character = CHARACTER_terminator;
	}
}

static struct TOKEN lex(struct LEXER *lexer) {
	while (lexer->character == CHARACTER_whitespace)
		advance_lexer(lexer);
	
	struct TOKEN token;
	token.range.beginning = lexer->position;
	token.range.row       = lexer->row;
	token.range.column    = lexer->column;

	WORD state = LEXER_STATE_initial;
	for (;;) {
		state = lexer_state_from_character[state][lexer->character];
		if (state < LEXER_STATE_initial)
			break;
		advance_lexer(lexer);
	}
	assert(state < TOKEN_TAGS_COUNT);

	token.tag          = state;
	token.range.ending = lexer->position;

	return token;
}

static struct LEXER create_lexer(const CHAR *path) {
	struct LEXER lexer = {
		.source    = load_source(path),
		.position  = 0,
		.row       = 0,
		.column    = 0,
		.increment = 0,
		.character = '\n'
	};
	advance_lexer(&lexer);
	return lexer;
}

static const CHAR string_from_token_tag[][32] = {
	[TOKEN_TAG_undefined                     ] = "undefined",
	[TOKEN_TAG_terminator                    ] = "terminator",

	[TOKEN_TAG_word                          ] = "word",
	[TOKEN_TAG_binary                        ] = "binary",
	[TOKEN_TAG_octal                         ] = "octal",
	[TOKEN_TAG_digital                       ] = "digital",
	[TOKEN_TAG_hexadecimal                   ] = "hexadecimal",
	[TOKEN_TAG_decimal                       ] = "decimal",
	[TOKEN_TAG_text                          ] = "text",

	[TOKEN_TAG_ampersand                     ] = "ampersand",
	[TOKEN_TAG_ampersand_2                   ] = "ampersand_2",
	[TOKEN_TAG_ampersand_equal_sign          ] = "ampersand_equal_sign",
	[TOKEN_TAG_apostrophe                    ] = "apostrophe",
	[TOKEN_TAG_asterisk                      ] = "asterisk",
	[TOKEN_TAG_asterisk_equal_sign           ] = "asterisk_equal_sign",
	[TOKEN_TAG_at_sign                       ] = "at_sign",
	[TOKEN_TAG_backslash                     ] = "backslash",
	[TOKEN_TAG_circumflex_accent             ] = "circumflex_accent",
	[TOKEN_TAG_circumflex_accent_equal_sign  ] = "circumflex_accent_equal_sign",
	[TOKEN_TAG_colon                         ] = "colon",
	[TOKEN_TAG_comma                         ] = "comma",
	[TOKEN_TAG_dollar_sign                   ] = "dollar_sign",
	[TOKEN_TAG_equal_sign                    ] = "equal_sign",
	[TOKEN_TAG_equal_sign_2                  ] = "equal_sign_2",
	[TOKEN_TAG_exclamation_mark              ] = "exclamation_mark",
	[TOKEN_TAG_exclamation_mark_equal_sign   ] = "exclamation_mark_equal_sign",
	[TOKEN_TAG_full_stop                     ] = "full_stop",
	[TOKEN_TAG_grave_accent                  ] = "grave_accent",
	[TOKEN_TAG_greaterthan_sign              ] = "greaterthan_sign",
	[TOKEN_TAG_greaterthan_sign_2            ] = "greaterthan_sign_2",
	[TOKEN_TAG_greaterthan_sign_2_equal_sign ] = "greaterthan_sign_2_equal_sign",
	[TOKEN_TAG_greaterthan_sign_equal_sign   ] = "greaterthan_sign_equal_sign",
	[TOKEN_TAG_hyphenminus                   ] = "hyphenminus",
	[TOKEN_TAG_hyphenminus_equal_sign        ] = "hyphenminus_equal_sign",
	[TOKEN_TAG_hyphenminus_greaterthan_sign  ] = "hyphenminus_greaterthan_sign",
	[TOKEN_TAG_left_curly_bracket            ] = "left_curly_bracket",
	[TOKEN_TAG_left_parenthesis              ] = "left_parenthesis",
	[TOKEN_TAG_left_square_bracket           ] = "left_square_bracket",
	[TOKEN_TAG_lessthan_sign                 ] = "lessthan_sign",
	[TOKEN_TAG_lessthan_sign_2               ] = "lessthan_sign_2",
	[TOKEN_TAG_lessthan_sign_2_equal_sign    ] = "lessthan_sign_2_equal_sign",
	[TOKEN_TAG_lessthan_sign_equal_sign      ] = "lessthan_sign_equal_sign",
	[TOKEN_TAG_octothorpe                    ] = "octothorpe",
	[TOKEN_TAG_percent_sign                  ] = "percent_sign",
	[TOKEN_TAG_percent_sign_equal_sign       ] = "percent_sign_equal_sign",
	[TOKEN_TAG_plus_sign                     ] = "plus_sign",
	[TOKEN_TAG_plus_sign_equal_sign          ] = "plus_sign_equal_sign",
	[TOKEN_TAG_question_mark                 ] = "question_mark",
	[TOKEN_TAG_right_curly_bracket           ] = "right_curly_bracket",
	[TOKEN_TAG_right_parenthesis             ] = "right_parenthesis",
	[TOKEN_TAG_right_square_bracket          ] = "right_square_bracket",
	[TOKEN_TAG_semicolon                     ] = "semicolon",
	[TOKEN_TAG_slash                         ] = "slash",
	[TOKEN_TAG_slash_equal_sign              ] = "slash_equal_sign",
	[TOKEN_TAG_tilde                         ] = "tilde",
	[TOKEN_TAG_vertical_bar                  ] = "vertical_bar",
	[TOKEN_TAG_vertical_bar_2                ] = "vertical_bar_2",
	[TOKEN_TAG_vertical_bar_equal_sign       ] = "vertical_bar_equal_sign",
};

enum SEVERITY {
	SEVERITY_verbose,
	SEVERITY_comment,
	SEVERITY_caution,
	SEVERITY_failure,
};

#include <stdio.h> /* TODO(Emhyr): ditch <stdio.h> */

static VOID report_v(enum SEVERITY severity, const struct SOURCE *source, const struct RANGE *range, const CHAR *message, VARGS vargs) {
	static const CHAR string_from_severity[][8] = {
		[SEVERITY_verbose] = "verbose",
		[SEVERITY_comment] = "comment",
		[SEVERITY_caution] = "caution",
		[SEVERITY_failure] = "failure"
	};

	fprintf(stdout, ":: %s: ", string_from_severity[severity]);
	if (source && range) {
		fprintf(stdout, "%s:%u,%u:%u,%u: ", source->path, range->beginning, range->ending, range->row, range->column);
		vfprintf(stdout, message, vargs);
		fputc('\n', stdout);

		size_t size = range->ending - range->beginning;
		if (size) fprintf(stdout, "\t%.*s\n", (int)size, source->data + range->beginning);
	} else {
		vfprintf(stdout, message, vargs);
		fputc('\n', stdout);
	}
}

static VOID report(enum SEVERITY severity, const struct SOURCE *source, const struct RANGE *range, const CHAR *message, ...) {
	VARGS vargs;
	get_vargs(vargs, message);
	report_v(severity, source, range, message, vargs);
	end_vargs(vargs);
}

__attribute__((noreturn))
extern VOID _exit(WORD);

/*
TODO(Emhyr): instead of exiting, `longjmp` to an "error handler" that
simpley deallocates memory and skips to a terminator token. the error handler
would usually be within the iteration that parses statements.
*/

__attribute__((noreturn))
static VOID fail(const struct SOURCE *source, const struct RANGE *range, const CHAR *message, ...) {
	if (message) {
		VARGS vargs;
		get_vargs(vargs, message);
		report_v(SEVERITY_failure, source, range, message, vargs);
		end_vargs(vargs);
	}
	_exit(-1);
}

enum NODE_TAG {
	NODE_TAG_undefined = 0,
	NODE_TAG_nil,

	/* WARNING(Emhyr): it's crucial that the order of the literals are as is */

	/* literals */
	NODE_TAG_natural,                   /* binary | octal | digital | hexadecimal */
	NODE_TAG_real,                      /* decimal | scientific */
	NODE_TAG_string,                    /* text */
	NODE_TAG_reference,                 /* word */
	/* bitwise */
	NODE_TAG_not,                       /* `~` expression */
	NODE_TAG_and,                       /* expression `&` expression */
	NODE_TAG_or,                        /* expression `|` expression */
	NODE_TAG_xor,                       /* expression `^` expression */
	NODE_TAG_lsh,                       /* expression `<<` expression */
	NODE_TAG_rsh,                       /* expression `>>` expression */
	/* arithmetic */
	NODE_TAG_negative,                  /* `-` expression */
	NODE_TAG_addition,                  /* expression `+` expression */
	NODE_TAG_subtraction,               /* expression `-` expression */
	NODE_TAG_multiplication,            /* expression `*` expression */
	NODE_TAG_division,                  /* expression `/` expression */
	NODE_TAG_remainder,                 /* expression `%` expression */
	/* logical */
	NODE_TAG_negation,                  /* `!` expression */
	NODE_TAG_conjunction,               /* expression `&&` expression */
	NODE_TAG_disjunction,               /* expression `||` expression */
	NODE_TAG_implication,               /* expression `?` expression `!` expression */
	/* relational */
	NODE_TAG_equality,                  /* expression `==` expression */
	NODE_TAG_inequality,                /* expression `!=` expression */
	NODE_TAG_greater,                   /* expression `>` expression */
	NODE_TAG_lesser,                    /* expression `<` expression */
	NODE_TAG_greater_equality,          /* expression `>=` expression */
	NODE_TAG_lesser_equality,           /* expression `<=` expression */
	/* controlflow */
	NODE_TAG_jump,                      /* `^` expression */
	/* resolution */
	NODE_TAG_address,                   /* `@` expression */
	NODE_TAG_indexation,                /* `[` expression `]` */
	NODE_TAG_resolution,                /* expression `.` expression */
	/* typing */
	NODE_TAG_record,                    /* `(` [value-declaration {`,` value-declaration}] `)` */
	NODE_TAG_pointer,                   /* `@` type */
	NODE_TAG_array,                     /* `[` expression `]` type */
	NODE_TAG_lambda,                    /* `(` [value-declaration {`,` value-declaration}] `)` `->` type */
	NODE_TAG_cast,                      /* expression `:` type */
	/* assignment */
	NODE_TAG_assignment,                /* expression `=` expression */
	NODE_TAG_addition_assignment,       /* expression `+=` expression */
	NODE_TAG_subtraction_assignment,    /* expression `-=` expression */
	NODE_TAG_multiplication_assignment, /* expression `*=` expression */
	NODE_TAG_division_assignment,       /* expression `/=` expression */
	NODE_TAG_remainder_assignment,      /* expression `%=` expression */
	NODE_TAG_and_assignment,            /* expression `&=` expression */
	NODE_TAG_or_assignment,             /* expression `|=` expression */
	NODE_TAG_xor_assignment,            /* expression `^=` expression */
	NODE_TAG_lsh_assignment,            /* expression `<<=` expression */
	NODE_TAG_rsh_assignment,            /* expression `>>=` expression */
	/* misc. */
	NODE_TAG_subexpression,             /* `(` expression `)` */
	NODE_TAG_invocation,                /* expression expression */
	NODE_TAG_junction,                  /* expression `,` expression */
	NODE_TAG_argument,                  /* expression */

	NODE_TAGS_COUNT,
};

struct NODE {
	enum NODE_TAG tag;
	struct RANGE range;
};

typedef BYTE PRECEDENCE;

static const PRECEDENCE precedence_from_node_tag[] = {
	[NODE_TAG_resolution               ] = 15,
	[NODE_TAG_jump                     ] = 14,
	[NODE_TAG_negative                 ] = 14,
	[NODE_TAG_negation                 ] = 14,
	[NODE_TAG_not                      ] = 14,
	[NODE_TAG_address                  ] = 14,
	[NODE_TAG_invocation               ] = 14,
	[NODE_TAG_multiplication           ] = 13,
	[NODE_TAG_division                 ] = 13,
	[NODE_TAG_remainder                ] = 13,
	[NODE_TAG_addition                 ] = 12,
	[NODE_TAG_subtraction              ] = 12,
	[NODE_TAG_lsh                      ] = 11,
	[NODE_TAG_rsh                      ] = 11,
	[NODE_TAG_greater                  ] = 10,
	[NODE_TAG_lesser                   ] = 10,
	[NODE_TAG_greater_equality         ] = 10,
	[NODE_TAG_lesser_equality          ] = 10,
	[NODE_TAG_equality                 ] = 9,
	[NODE_TAG_inequality               ] = 9,
	[NODE_TAG_and                      ] = 8,
	[NODE_TAG_xor                      ] = 7,
	[NODE_TAG_or                       ] = 6,
	[NODE_TAG_conjunction              ] = 5,
	[NODE_TAG_disjunction              ] = 4,
	[NODE_TAG_implication              ] = 3,
	[NODE_TAG_assignment               ] = 2,
	[NODE_TAG_addition_assignment      ] = 2,
	[NODE_TAG_multiplication_assignment] = 2,
	[NODE_TAG_division_assignment      ] = 2,
	[NODE_TAG_remainder_assignment     ] = 2,
	[NODE_TAG_addition_assignment      ] = 2,
	[NODE_TAG_subtraction_assignment   ] = 2,
	[NODE_TAG_lsh_assignment           ] = 2,
	[NODE_TAG_rsh_assignment           ] = 2,
	[NODE_TAG_and_assignment           ] = 2,
	[NODE_TAG_xor_assignment           ] = 2,
	[NODE_TAG_or_assignment            ] = 2,
	[NODE_TAG_junction                 ] = 1,
};

struct PARSER {
	struct LEXER lexer;
	struct TOKEN token;
};

static struct NODE *parse_expression(C_BUFFER *buffer, PRECEDENCE other_precedence, struct PARSER *parser);
static struct NODE *parse_type      (C_BUFFER *buffer, struct PARSER *parser);

/* NOTE(Emhyr): i like tables... */

static const enum NODE_TAG unary_node_tag_from_token_tag[TOKEN_TAGS_COUNT] = {
	[0 ...TOKEN_TAGS_COUNT - 1      ] = NODE_TAG_undefined,
	[TOKEN_TAG_right_square_bracket] = NODE_TAG_nil,
	[TOKEN_TAG_right_parenthesis   ] = NODE_TAG_nil,
	[TOKEN_TAG_binary              ] = NODE_TAG_natural,
	[TOKEN_TAG_octal               ] = NODE_TAG_natural,
	[TOKEN_TAG_digital             ] = NODE_TAG_natural,
	[TOKEN_TAG_hexadecimal         ] = NODE_TAG_natural,
	[TOKEN_TAG_decimal             ] = NODE_TAG_real,
	[TOKEN_TAG_text                ] = NODE_TAG_string,
	[TOKEN_TAG_word                ] = NODE_TAG_reference,
	[TOKEN_TAG_tilde               ] = NODE_TAG_not,
	[TOKEN_TAG_hyphenminus         ] = NODE_TAG_negative,
	[TOKEN_TAG_exclamation_mark    ] = NODE_TAG_negation,
	[TOKEN_TAG_circumflex_accent   ] = NODE_TAG_jump,
	[TOKEN_TAG_at_sign             ] = NODE_TAG_address,
	[TOKEN_TAG_left_square_bracket ] = NODE_TAG_indexation,
	[TOKEN_TAG_left_parenthesis    ] = NODE_TAG_subexpression,
};

static const enum NODE_TAG binary_node_tag_from_token_tag[TOKEN_TAGS_COUNT] = {
	[0 ...TOKEN_TAGS_COUNT - 1            ] = NODE_TAG_invocation,
	[TOKEN_TAG_ampersand                  ] = NODE_TAG_and,
	[TOKEN_TAG_vertical_bar               ] = NODE_TAG_or,
	[TOKEN_TAG_circumflex_accent          ] = NODE_TAG_xor,
	[TOKEN_TAG_lessthan_sign_2            ] = NODE_TAG_lsh,
	[TOKEN_TAG_greaterthan_sign_2         ] = NODE_TAG_rsh,
	[TOKEN_TAG_plus_sign                  ] = NODE_TAG_addition,
	[TOKEN_TAG_hyphenminus                ] = NODE_TAG_subtraction,
	[TOKEN_TAG_asterisk                   ] = NODE_TAG_multiplication,
	[TOKEN_TAG_slash                      ] = NODE_TAG_division,
	[TOKEN_TAG_percent_sign               ] = NODE_TAG_remainder,
	[TOKEN_TAG_ampersand_2                ] = NODE_TAG_conjunction,
	[TOKEN_TAG_vertical_bar_2             ] = NODE_TAG_disjunction,
	[TOKEN_TAG_equal_sign_2               ] = NODE_TAG_equality,
	[TOKEN_TAG_exclamation_mark_equal_sign] = NODE_TAG_inequality,
	[TOKEN_TAG_greaterthan_sign           ] = NODE_TAG_greater,
	[TOKEN_TAG_lessthan_sign              ] = NODE_TAG_lesser,
	[TOKEN_TAG_greaterthan_sign_equal_sign] = NODE_TAG_greater_equality,
	[TOKEN_TAG_lessthan_sign_equal_sign   ] = NODE_TAG_lesser_equality,
	[TOKEN_TAG_comma                      ] = NODE_TAG_junction,
	[TOKEN_TAG_equal_sign                 ] = NODE_TAG_assignment,
	[TOKEN_TAG_question_mark              ] = NODE_TAG_implication,
	[TOKEN_TAG_colon                      ] = NODE_TAG_cast,
	[TOKEN_TAG_semicolon                  ] = NODE_TAG_nil,
	[TOKEN_TAG_right_parenthesis          ] = NODE_TAG_nil,
	[TOKEN_TAG_right_square_bracket       ] = NODE_TAG_nil,
};

static const CHAR string_from_node_tag[][32] = {
	[NODE_TAG_undefined                ] = "undefined",
	[NODE_TAG_nil                      ] = "nil",
	[NODE_TAG_natural                  ] = "natural",
	[NODE_TAG_real                     ] = "real",
	[NODE_TAG_string                   ] = "string",
	[NODE_TAG_reference                ] = "reference",
	[NODE_TAG_not                      ] = "not",
	[NODE_TAG_and                      ] = "and",
	[NODE_TAG_or                       ] = "or",
	[NODE_TAG_xor                      ] = "xor",
	[NODE_TAG_lsh                      ] = "lsh",
	[NODE_TAG_rsh                      ] = "rsh",
	[NODE_TAG_negative                 ] = "negative",
	[NODE_TAG_addition                 ] = "addition",
	[NODE_TAG_subtraction              ] = "subtraction",
	[NODE_TAG_multiplication           ] = "multiplication",
	[NODE_TAG_division                 ] = "division",
	[NODE_TAG_remainder                ] = "remainder",
	[NODE_TAG_negation                 ] = "negation",
	[NODE_TAG_conjunction              ] = "conjunction",
	[NODE_TAG_disjunction              ] = "disjunction",
	[NODE_TAG_implication              ] = "implication",
	[NODE_TAG_equality                 ] = "equality",
	[NODE_TAG_inequality               ] = "inequality",
	[NODE_TAG_greater                  ] = "greater",
	[NODE_TAG_lesser                   ] = "lesser",
	[NODE_TAG_greater_equality         ] = "greater_equality",
	[NODE_TAG_lesser_equality          ] = "lesser_equality",
	[NODE_TAG_jump                     ] = "jump",
	[NODE_TAG_address                  ] = "address",
	[NODE_TAG_indexation               ] = "indexation",
	[NODE_TAG_resolution               ] = "resolution",
	[NODE_TAG_record                   ] = "record",
	[NODE_TAG_pointer                  ] = "pointer",
	[NODE_TAG_array                    ] = "array",
	[NODE_TAG_lambda                   ] = "lambda",
	[NODE_TAG_cast                     ] = "cast",
	[NODE_TAG_assignment               ] = "assignment",
	[NODE_TAG_addition_assignment      ] = "addition_assignment",
	[NODE_TAG_subtraction_assignment   ] = "subtraction_assignment",
	[NODE_TAG_multiplication_assignment] = "multiplication_assignment",
	[NODE_TAG_division_assignment      ] = "division_assignment",
	[NODE_TAG_remainder_assignment     ] = "remainder_assignment",
	[NODE_TAG_and_assignment           ] = "and_assignment",
	[NODE_TAG_or_assignment            ] = "or_assignment",
	[NODE_TAG_xor_assignment           ] = "xor_assignment",
	[NODE_TAG_lsh_assignment           ] = "lsh_assignment",
	[NODE_TAG_rsh_assignment           ] = "rsh_assignment",
	[NODE_TAG_subexpression            ] = "subexpression",
	[NODE_TAG_invocation               ] = "invocation",
	[NODE_TAG_junction                 ] = "junction",
	[NODE_TAG_argument                 ] = "argument",
};

/*
NOTE(Emhyr): literals aren't converted into an actual operand when parsing
because why? it isn't like we're going to operate on them; we just care that
the token is used syntactically correctly. the checker will check if it's a
valid operand.

NOTE(Emhyr): maybe instead of repeating nodes with the same tag, encode a
`multiplier`? this would reduce the common case of parsing arguments where a
junction nodes are many.
*/

/* #recursive */
static struct NODE *parse_expression(C_BUFFER *buffer, PRECEDENCE other_precedence, struct PARSER *parser) {
	struct RANGE beginning_range = parser->token.range;
	SIZE beginning_data_size = buffer->data_size;
	struct NODE *node = 0, *other_node;
	enum NODE_TAG node_tag = unary_node_tag_from_token_tag[parser->token.tag];
	switch (node_tag) {
	case NODE_TAG_undefined:
		fail(&parser->lexer.source, &parser->token.range, "unexpected token when parsing expression");
	case NODE_TAG_nil:
		node = push(sizeof(struct NODE), alignof(struct NODE), buffer);
		node->tag = NODE_TAG_nil;
		goto finished;
	default:
		BOOLEAN is_literal = node_tag >= NODE_TAG_natural && node_tag <= NODE_TAG_reference;
		if (!is_literal) parser->token = lex(&parser->lexer);
		node = push(sizeof(struct NODE), alignof(struct NODE), buffer);
		node->tag = node_tag;
		node->range = beginning_range;
		if (is_literal)
			parser->token = lex(&parser->lexer);
		else if (node_tag == NODE_TAG_indexation || node_tag == NODE_TAG_subexpression) {
			(VOID)parse_expression(buffer, 0, parser);
			if (parser->token.tag == (node_tag == NODE_TAG_indexation ? TOKEN_TAG_right_square_bracket : TOKEN_TAG_right_parenthesis)) {
				node->range.ending = parser->token.range.ending;
				parser->token = lex(&parser->lexer);
			}
		} else {
			other_node = parse_expression(buffer, other_precedence, parser);
			node->range.ending = other_node->range.ending;
		}
		break;
	}
	for (;;) {
		node_tag = binary_node_tag_from_token_tag[parser->token.tag];
		switch (node_tag) {
		case NODE_TAG_nil:
			goto finished;
		default:
			PRECEDENCE precedence = precedence_from_node_tag[node_tag];
			if (precedence < other_precedence) goto finished;
			if (node_tag != NODE_TAG_invocation) parser->token = lex(&parser->lexer);
			(VOID)push(sizeof(struct NODE), alignof(struct NODE), buffer);
			node = buffer->data + beginning_data_size;
			move(node + 1, node, buffer->data_size - beginning_data_size);
			node->tag = node_tag;
			node->range = beginning_range;
			if (node_tag == NODE_TAG_implication) {
				(VOID)parse_expression(buffer, 0, parser);
				if (parser->token.tag == TOKEN_TAG_exclamation_mark) parser->token = lex(&parser->lexer);
			}
			if (node_tag == NODE_TAG_cast) other_node = parse_type(buffer, parser);
			else other_node = parse_expression(buffer, precedence, parser);
			node->range.ending = other_node->range.ending;
			break;
		}
	}

finished:
	return node;
}

static struct NODE *parse_type(C_BUFFER *buffer, struct PARSER *parser) {
	struct NODE *node = 0;
	struct RANGE range = parser->token.range;
	switch (parser->token.tag) {
	case TOKEN_TAG_word:
		node = push(sizeof(struct NODE), alignof(struct NODE), buffer);
		node->tag = NODE_TAG_reference;
		node->range = range;
		node->range.ending = parser->token.range.ending;
		parser->token = lex(&parser->lexer);
		break;
	case TOKEN_TAG_at_sign:
		parser->token = lex(&parser->lexer);
		node = push(sizeof(struct NODE), alignof(struct NODE), buffer);
		node->tag = NODE_TAG_address;
		node->range = range;
		node->range.ending = parse_type(buffer, parser)->range.ending;
		break;
	case TOKEN_TAG_left_square_bracket:
	case TOKEN_TAG_left_parenthesis:
	default:
		fail(&parser->lexer.source, &parser->token.range, "unexpected token when parsing type");
	}
	return node;
}

/*
NOTE(Emhyr): parsing things such as declarations aren't stored normally like an
expression node, but rather a deticated medium for efficient lookup.

NOTE(Emhyr): even statements can
*/

static struct PARSER create_parser(const CHAR *path)
{
	struct PARSER parser = {
		.lexer = create_lexer(path),
		.token = lex(&parser.lexer)
	};
	return parser;
}

static VOID dump(const struct SOURCE *source, struct NODE *nodes, SIZE nodes_count) {
	for (SIZE i = 0; i < nodes_count; ++i) {
		report(SEVERITY_comment, source, &nodes[i].range, "%s", string_from_node_tag[nodes[i].tag]);
	}
}


int main(int argc, char *argv[]) {
	if (argc <= 1) fail(0, 0, "a path must be given");

	const CHAR *path = argv[1];

	struct PARSER parser = create_parser(path);
	struct BUFFER buffer = DEFAULT_BUFFER;

	do {
		parse_expression(&buffer, 0, &parser);
		dump(&parser.lexer.source, buffer.data, buffer.data_size / sizeof(struct NODE));
		buffer.data_size = 0;
		if (parser.token.tag == TOKEN_TAG_semicolon) parser.token = lex(&parser.lexer);
		puts("--------------------------\n");
	} while (parser.token.tag != TOKEN_TAG_terminator);

	
	
	return 0;
}

#pragma GCC diagnostic pop
