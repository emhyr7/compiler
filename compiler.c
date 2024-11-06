#include "compiler.h"

uint32 format_v(utf8 *buffer, uint32 size, const utf8 *format, vargs vargs);
uint32 format(utf8 *buffer, uint32 size, const utf8 *format, ...);
void print_v(const utf8 *format, vargs vargs);
void print(const utf8 *format, ...);

uint8 decode_utf8(utf32 *character, const utf8 bytes[4]);

void copy_memory(void *left, const void *right, uint32 size);
void move_memory(void *left, const void *right, uint32 size);
void fill_memory(void *buffer, uint32 size, uint8 value);
void zero_memory(void *buffer, uint32 size);
sint16 compare_memory(const void *left, const void *right, uint32 size);

uint64 get_size_of_string(const utf8 *string);

constexpr uint32 universal_alignment = alignof(long double);

uint32 get_backward_alignment(uint64 address, uint32 alignment);
uint32 get_forward_alignment(uint64 address, uint32 alignment);

typedef struct
{
	uint32 reservation_size;
	uint32 commission_rate;
	uint32 commission_size;
	uint32 mass;

	alignas(universal_alignment) uint8 base[];
} buffer;

buffer *allocate_buffer(uint32 reservation_size, uint32 commission_rate);
void deallocate_buffer(buffer *buffer);
void *push_into_buffer(uint32 size, uint32 alignment, buffer *buffer);

typedef struct
{
	const utf8 *path;
	utf8 *data;
	uint32 path_size;
	uint32 data_size;
} source;

static void load_source(source *source, const utf8 *path)
{
	source->path = path;
	source->path_size = get_size_of_string(source->path);
	handle file = open_file(path);
	source->data_size = get_size_of_file(file);
	source->data = allocate_memory(source->data_size);
	read_from_file(source->data, source->data_size, file);
	close_file(file);
}

typedef struct
{
	uint32 beginning;
	uint32 ending;
	uint32 row;
	uint32 column;
} range;

typedef struct
{
	uint32 position;
	uint32 row;
	uint32 column;
} location;

typedef struct
{
	const source *source;
	location location;
	utf32 character;
	uint8 increment;
} caret;

typedef enum : uint8
{
	token_tag_etx                  = '\3',
	token_tag_exclamation_mark     = '!',
	token_tag_octothorpe           = '#',
	token_tag_dollar_sign          = '$',
	token_tag_percent_sign         = '%',
	token_tag_ampersand            = '&',
	token_tag_apostrophe           = '\'',
	token_tag_left_parenthesis     = '(',
	token_tag_right_parenthesis    = ')',
	token_tag_asterisk             = '*',
	token_tag_plus_sign            = '+',
	token_tag_comma                = ',',
	token_tag_hyphen_minus         = '-',
	token_tag_full_stop            = '.',
	token_tag_slash                = '/',
	token_tag_colon                = ':',
	token_tag_semicolon            = ';',
	token_tag_less_than_sign       = '<',
	token_tag_equal_sign           = '=',
	token_tag_greater_than_sign    = '>',
	token_tag_question_mark        = '?',
	token_tag_at_sign              = '@',
	token_tag_left_square_bracket  = '[',
	token_tag_backslash            = '\\',
	token_tag_right_square_bracket = ']',
	token_tag_circumflex_accent    = '^',
	token_tag_grave_accent         = '`',
	token_tag_left_curly_bracket   = '{',
	token_tag_vertical_bar         = '|',
	token_tag_right_curly_bracket  = '}',
	token_tag_tilde                = '~',
	token_tag_name                 = 'A',
	token_tag_binary,
	token_tag_octal,
	token_tag_digital,
	token_tag_hexadecimal,
	token_tag_decimal,
	token_tag_text,
	token_tag_exclamation_mark_equal_sign,
	token_tag_percent_sign_equal_sign,
	token_tag_ampersand_equal_sign,
	token_tag_ampersand_2,
	token_tag_asterisk_equal_sign,
	token_tag_plus_sign_equal_sign,
	token_tag_hyphen_minus_equal_sign,
	token_tag_slash_equal_sign,
	token_tag_less_than_sign_equal_sign,
	token_tag_less_than_sign_2,
	token_tag_less_than_sign_2_equal_sign,
	token_tag_equal_sign_2,
	token_tag_greater_than_sign_equal_sign,
	token_tag_greater_than_sign_2,
	token_tag_greater_than_sign_2_equal_sign,
	token_tag_circumflex_accent_equal_sign,
	token_tag_vertical_bar_equal_sign,
	token_tag_vertical_bar_2,
} token_tag;

static utf8 *representations_of_token_tags[] =
{
	[token_tag_etx]                            = "ETX",
	[token_tag_exclamation_mark]               = "`!`",
	[token_tag_octothorpe]                     = "`#`",
	[token_tag_dollar_sign]                    = "`$`",
	[token_tag_percent_sign]                   = "`%`",
	[token_tag_ampersand]                      = "`&`",
	[token_tag_apostrophe]                     = "`'`",
	[token_tag_left_parenthesis]               = "`(`",
	[token_tag_right_parenthesis]              = "`)`",
	[token_tag_asterisk]                       = "`*`",
	[token_tag_plus_sign]                      = "`+`",
	[token_tag_comma]                          = "`,`",
	[token_tag_hyphen_minus]                   = "`-`",
	[token_tag_full_stop]                      = "`.`",
	[token_tag_slash]                          = "`/`",
	[token_tag_colon]                          = "`:`",
	[token_tag_semicolon]                      = "`;`",
	[token_tag_less_than_sign]                 = "`<`",
	[token_tag_equal_sign]                     = "`=`",
	[token_tag_greater_than_sign]              = "`>`",
	[token_tag_question_mark]                  = "`?`",
	[token_tag_at_sign]                        = "`@`",
	[token_tag_left_square_bracket]            = "`[`",
	[token_tag_backslash]                      = "`\\`",
	[token_tag_right_square_bracket]           = "`]`",
	[token_tag_circumflex_accent]              = "`^`",
	[token_tag_grave_accent]                   = "```",
	[token_tag_left_curly_bracket]             = "`{`",
	[token_tag_vertical_bar]                   = "`|`",
	[token_tag_right_curly_bracket]            = "`}`",
	[token_tag_tilde]                          = "`~`",
	[token_tag_name]                           = "name",
	[token_tag_binary]                         = "binary",
	[token_tag_octal]                          = "octal",
	[token_tag_digital]                        = "digital",
	[token_tag_hexadecimal]                    = "hexadecimal",
	[token_tag_decimal]                        = "decimal",
	[token_tag_text]                           = "text",
	[token_tag_exclamation_mark_equal_sign]    = "`!=`",
	[token_tag_percent_sign_equal_sign]        = "`%=`",
	[token_tag_ampersand_equal_sign]           = "`&=`",
	[token_tag_ampersand_2]                    = "`&&`",
	[token_tag_asterisk_equal_sign]            = "`*=`",
	[token_tag_plus_sign_equal_sign]           = "`+=`",
	[token_tag_hyphen_minus_equal_sign]        = "`-=`",
	[token_tag_slash_equal_sign]               = "`/=`",
	[token_tag_less_than_sign_equal_sign]      = "`<=`",
	[token_tag_less_than_sign_2]               = "`<<`",
	[token_tag_less_than_sign_2_equal_sign]    = "`<<=`",
	[token_tag_equal_sign_2]                   = "`==`",
	[token_tag_greater_than_sign_equal_sign]   = "`>=`",
	[token_tag_greater_than_sign_2]            = "`>>`",
	[token_tag_greater_than_sign_2_equal_sign] = "`>>=`",
	[token_tag_circumflex_accent_equal_sign]   = "`^=`",
	[token_tag_vertical_bar_equal_sign]        = "`|=`",
	[token_tag_vertical_bar_2]                 = "`||`"
};

typedef struct
{
	token_tag tag;
	range range;
} token;

static inline bit check_whitespace(utf32 character)
{
	return character >= '\t' && character <= '\r' || character == ' ';
}

static inline bit check_letter(utf32 character)
{
	return character == '_' || character >= 'A' && character <= 'Z' || character >= 'a' && character <= 'z';
}

static inline bit check_binary(utf32 character)
{
	return character == '0' || character == '1';
}

static inline bit check_digital(utf32 character)
{
	return character >= '0' && character <= '9';
}

static inline bit check_hexadecimal(utf32 character)
{
	return check_digital(character) || character >= 'A' && character <= 'F' || character >= 'a' && character <= 'f';
}

static utf32 peek(uint8 *increment, const caret *caret)
{
	utf32 character;
	uint32 position = caret->location.position + caret->increment;
	if(position < caret->source->data_size)
	{
		utf8 bytes[4] = {};
		for(uint8 i = 0; i < sizeof(bytes) / sizeof(bytes[0]); ++i)
		{
			bytes[i] = caret->source->data[position];
			if(++position >= caret->source->data_size) break;
		}
		*increment = decode_utf8(&character, bytes);
	}
	else
	{
		*increment = 0;
		character = '\3';
	}
	return character;
}

static utf32 advance(caret *caret)
{
	uint8 increment;
	utf32 character = peek(&increment, caret);

	caret->location.position += caret->increment;
	if(caret->character == '\n')
	{
		++caret->location.row;
		caret->location.column = 0;
	}
	++caret->location.column;
	caret->character = character;
	caret->increment = increment;
	return character;
}

typedef enum
{
	severity_verbose,
	severity_comment,
	severity_caution,
	severity_failure,
} severity;

static void report_v(severity severity, const source *source, const range *range, const utf8 *message, vargs vargs)
{
	const utf8 *severities[] =
	{
		[severity_verbose] = "verbose",
		[severity_comment] = "comment",
		[severity_caution] = "caution",
		[severity_failure] = "failure",
	};
	if(source && range)
	{
		print("%.*s[%u-%u|%u,%u]: %s: ", source->path_size, source->path, range->beginning, range->ending, range->row, range->column, severities[severity]);
		print_v(message, vargs);
		print("\n");

		uint32 range_size = range->ending - range->beginning;
		if(range_size)
		{
			// TODO(Emhyr): better printing
			utf8 view[range_size];
			copy_memory(view, source->data + range->beginning, range_size);
			print("\t%.*s\n", range_size, view);
		}
	}
	else
	{
		print("%s: ", severities[severity]);
		print_v(message, vargs);
		print("\n");
	}
}

static inline void report(severity severity, const source *source, const range *range, const utf8 *message, ...)
{
	vargs vargs;
	GET_VARGS(vargs, message);
	report_v(severity, source, range, message, vargs);
	END_VARGS(vargs);
}

_Noreturn static inline void fail(const source *source, const range *range, const utf8 *message, ...)
{
	vargs vargs;
	GET_VARGS(vargs, message);
	report_v(severity_failure, source, range, message, vargs);
	END_VARGS(vargs);
	TRAP();
	UNREACHABLE();
}

static token_tag tokenize(token *token, caret *caret)
{
	const utf8 *failure_message;

repeat:
	while(check_whitespace(caret->character)) advance(caret);

	token->range.beginning = caret->location.position;
	token->range.row = caret->location.row;
	token->range.column = caret->location.column;

	if(check_letter(caret->character))
	{
		token->tag = token_tag_name;
		do advance(caret);
		while(check_letter(caret->character) || check_digital(caret->character) || caret->character == '-');
		// TODO(Emhyr): disallow names ending wiht `-`
	}
	else if(check_digital(caret->character))
	{
		bit (*checker)(utf32) = &check_digital;
		token->tag = token_tag_digital;
		if(caret->character == '0')
		{
			switch(advance(caret))
			{
			case 'b':
				token->tag = token_tag_binary;
				checker = &check_binary;
				advance(caret);
				break;
			case 'x':
				token->tag = token_tag_hexadecimal;
				checker = &check_hexadecimal;
				advance(caret);
				break;
			default:
				break;
			}
		}

		while(checker(caret->character) || caret->character == '_')
		{
			if(advance(caret) == '.')
			{
				switch(token->tag)
				{
				case token_tag_decimal:
				case token_tag_binary:
				case token_tag_hexadecimal:
					failure_message = "weird ass number";
					goto failed;
				default:
					token->tag = token_tag_decimal;
					advance(caret);
					break;
				}
			}
		}
	}
	else switch(caret->character)
	{
	case '\3':
		token->tag = token_tag_etx;
		break;
	case '"':
		for(;;) switch(advance(caret))
		{
		case '\\':
			advance(caret);
			break;
		case '\3':
			failure_message = "unterminated text";
			goto failed;
		case '"':
			goto text_terminated;
		}
	text_terminated:
		advance(caret);
		token->tag = token_tag_text;
		break;

	case '!':
	case '%':
	case '&':
	case '*':
	case '+':
	case '-':
	case '/':
	case '<':
	case '=':
	case '>':
	case '^':
	case '|':
		uint8 peeked_increment;
		utf32 second_character = peek(&peeked_increment, caret);
		if(second_character == '=')
		{
			switch(caret->character)
			{
			case '!': token->tag = token_tag_exclamation_mark_equal_sign;  break;
			case '%': token->tag = token_tag_percent_sign_equal_sign;      break;
			case '&': token->tag = token_tag_ampersand_equal_sign;         break;
			case '*': token->tag = token_tag_asterisk_equal_sign;          break;
			case '+': token->tag = token_tag_plus_sign_equal_sign;         break;
			case '-': token->tag = token_tag_hyphen_minus_equal_sign;      break;
			case '/': token->tag = token_tag_slash_equal_sign;             break;
			case '<': token->tag = token_tag_less_than_sign_equal_sign;    break;
			case '=': token->tag = token_tag_equal_sign_2;                 break;
			case '>': token->tag = token_tag_greater_than_sign_equal_sign; break;
			case '^': token->tag = token_tag_circumflex_accent_equal_sign; break;
			case '|': token->tag = token_tag_vertical_bar_equal_sign;      break;
			}
			goto advance_twice;
		}
		else if(second_character == caret->character)
		{
			switch(second_character)
			{
			case '&': token->tag = token_tag_ampersand_2;    break;
			case '|': token->tag = token_tag_vertical_bar_2; break;
			case '<':
			case '>':
				advance(caret);
				if(peek(&peeked_increment, caret) == '=')
				{
					switch(second_character)
					{
					case '<': token->tag = token_tag_less_than_sign_2_equal_sign;    break;
					case '>': token->tag = token_tag_greater_than_sign_2_equal_sign; break;
					}
				}
				else
				{
					switch(second_character)
					{
					case '<': token->tag = token_tag_less_than_sign_2;    break;
					case '>': token->tag = token_tag_greater_than_sign_2; break;
					}
				}
				break;
			}
			goto advance_twice;
		}
		else goto set_single;

	case '#':
		if(advance(caret) == ' ')
		{
			while(advance(caret) != '\n');
			goto repeat;
		}
	case '$':
	case '\'':
	case '(':
	case ')':
	case ',':
	case '.':
	case ':':
	case ';':
	case '?':
	case '@':
	case '[':
	case '\\':
	case ']':
	case '`':
	case '{':
	case '~':
	case '}':
	set_single:
		token->tag = caret->character;
		goto advance_once;
	advance_twice:
		advance(caret);
	advance_once:
		advance(caret);
		break;

	default:
		failure_message = "unknown character";
		goto failed;
	}

	token->range.ending = caret->location.position;
	return token->tag;

failed:
	token->range.ending = caret->location.position;
	fail(caret->source, &token->range, failure_message);
}

int start(int argc, utf8 *argv[])
{
	int status = 0;
	if(argc > 1)
	{
		source source;
		load_source(&source, argv[1]);
		caret caret = {&source, {0, 0, 0}, '\n', 0};
		advance(&caret);
		token token;
		while(tokenize(&token, &caret) != token_tag_etx)
		{
			report(severity_verbose, &source, &token.range, "%s", representations_of_token_tags[token.tag]);
		}
	}
	else fail(0, 0, "missing source");
	return status;
}

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

static inline uint32 format_v(utf8 *buffer, uint32 size, const utf8 *format, vargs vargs)
{
	return stbsp_vsnprintf(buffer, size, format, vargs);
}

static inline uint32 format(utf8 *buffer, uint32 size, const utf8 *format, ...)
{
	vargs vargs;
	GET_VARGS(vargs, format);
	size = format_v(buffer, size, format, vargs);
	END_VARGS(vargs);
	return size;
}

static void print_v(const utf8 *format, vargs vargs)
{
	typeof(vargs) vargs_copy;
	COPY_VARGS(vargs_copy, vargs);
	uint32 size = format_v(0, 0, format, vargs_copy) + 1;
	utf8 *message = allocate_memory(size);
	format_v(message, size, format, vargs);
	write_into_file(message, size, stdout_handle);
	release_memory(message, size);
}

static inline void print(const utf8 *format, ...)
{
	vargs vargs;
	GET_VARGS(vargs, format);
	print_v(format, vargs);
	END_VARGS(vargs);
}

// from https://bjoern.hoehrmann.de/utf-8/decoder/dfa/
typedef enum : uint16
{
	DFA_UTF8_STATE_ACCEPT = 0,
	DFA_UTF8_STATE_REJECT = 1,
} dfa_utf8_state;

static inline uint16 dfa_decode_utf8(dfa_utf8_state *state, utf32 *character, utf8 byte)
{
	static const uint8 table[] =
	{
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

	uint16 type = table[byte];
	*character = *state != DFA_UTF8_STATE_ACCEPT ? byte & 0x3fu | *character << 6 : 0xff >> type & byte;
	*state = table[256 + *state * 16 + type];
	return *state;
}

static uint8 decode_utf8(utf32 *character, const utf8 bytes[4])
{
	dfa_utf8_state state = DFA_UTF8_STATE_ACCEPT;
	uint16 i = 0;
	do if(dfa_decode_utf8(&state, character, bytes[i++]) == DFA_UTF8_STATE_ACCEPT) break;
	while(i < 4);
	return i;
}

static inline void copy_memory(void *left, const void *right, uint32 size)
{
	__builtin_memcpy(left, right, size);
}

static inline void move_memory(void *left, const void *right, uint32 size)
{
	__builtin_memmove(left, right, size);
}

static inline void fill_memory(void *buffer, uint32 size, uint8 value)
{
	__builtin_memset(buffer, value, size);
}

static inline void zero_memory(void *buffer, uint32 size)
{
	fill_memory(buffer, size, 0);
}

static inline sint16 compare_memory(const void *left, const void *right, uint32 size)
{
	return __builtin_memcmp(left, right, size);
}

static inline uint64 get_size_of_string(const utf8 *string)
{
	return __builtin_strlen(string);
}

static inline uint32 get_backward_alignment(uint64 address, uint32 alignment)
{
	ASSERT(alignment % 2 == 0);
	return alignment ? address & (alignment - 1) : 0;
}

static inline uint32 get_forward_alignment(uint64 address, uint32 alignment)
{
	uint32 remainder = get_backward_alignment(address, alignment);
	return remainder ? alignment - remainder : 0;
}

static buffer *allocate_buffer(uint32 reservation_size, uint32 commission_rate)
{
	reservation_size += get_forward_alignment(reservation_size, system_page_size);
	commission_rate += get_forward_alignment(commission_rate, system_page_size);
	buffer *buffer = reserve_memory(reservation_size);
	commit_memory(buffer, commission_rate);
	buffer->reservation_size = reservation_size;
	buffer->commission_rate  = commission_rate;
	buffer->commission_size  = commission_rate;
	buffer->mass             = sizeof(buffer);
	return buffer;
}

static inline void deallocate_buffer(buffer *buffer)
{
	release_memory(buffer, buffer->reservation_size);
}

static void *push_into_buffer(uint32 size, uint32 alignment, buffer *buffer)
{
	ASSERT(alignment % 2 == 0);
	uint32 forward_alignment = get_forward_alignment((uint64)buffer + buffer->mass, alignment);
	if(buffer->mass + forward_alignment + size > buffer->commission_size)
	{
		if(buffer->commission_size + buffer->commission_rate > buffer->reservation_size)
		{
			print("buffer overflows: [%p]\n\treservation_size: %u\n\tcommission_rate: %u\n\tcommission_size: %u\n\tmass: %u\n",
				buffer, buffer->reservation_size, buffer->commission_rate, buffer->commission_size, buffer->mass);
			TRAP();
		}
		commit_memory((uint8 *)buffer + buffer->commission_size, buffer->commission_rate);
		buffer->commission_size += buffer->commission_rate;
	}
	buffer->mass += forward_alignment;
	void *result = (void *)((uint64)buffer + buffer->mass);
	zero_memory(result, size);
	buffer->mass += size;
	return result;
}

