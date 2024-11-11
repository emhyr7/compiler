#include "compiler.h"

#define UNIMPLEMENTED() ({ ASSERT(!"unimplemented"); UNREACHABLE(); })

#define KIBIBYTE(x) ((uint64)(x) << 10)
#define MEBIBYTE(x) (KIBIBYTE(x) << 10)
#define GIBIBYTE(x) (MEBIBYTE(x) << 10)

#define UMAXOF(type) ((type)-1)

uint32 format_v(utf8 *buffer, uint32 size, const utf8 *format, vargs vargs);
uint32 format(utf8 *buffer, uint32 size, const utf8 *format, ...);
void print_v(const utf8 *format, vargs vargs);
void print(const utf8 *format, ...);

uint8 decode_utf8(utf32 *character, const utf8 bytes[4]);

void copy_memory(void *left, const void *right, uint32 size);
void move_memory(void *left, const void *right, uint32 size);
void fill_memory(void *buffer, uint32 size, uint8 Value);
void zero_memory(void *buffer, uint32 size);
sint16 compare_memory(const void *left, const void *right, uint32 size);

uint64 get_size_of_string(const utf8 *String);

constexpr uint32 universal_alignment = alignof(long double);

uint32 get_backward_alignment(uint64 address, uint32 alignment);
uint32 get_forward_alignment(uint64 address, uint32 alignment);

typedef struct
{
	uint8 *memory;
	uint32 reservation_size;
	uint32 commission_rate;
	uint32 commission_size;
	uint32 mass;

	alignas(universal_alignment) uint8 base[];
} Buffer;

Buffer *allocate_buffer(uint32 reservation_size, uint32 commission_rate);
void deallocate_buffer(Buffer *buffer);
void *push_into_buffer(uint32 size, uint32 alignment, Buffer *buffer);

typedef struct
{
	const utf8 *path;
	utf8       *data;
	uint32      data_size;
	uint16      path_size;
} Source;

static void load_source(Source *source, const utf8 *path)
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
} Range;

typedef struct
{
	uint32 position;
	uint32 row;
	uint32 column;
} Location;

typedef struct
{
	const Source *source;
	Location      location;
	utf32         character;
	uint8         increment;
} Caret;

// NOTE(Emhyr): didn't feel like using X macros :/. fingers hurt
typedef enum : uint8
{
	ETX                  = '\3',
	EXCLAMATION_MARK     = '!',
	OCTOTHORPE           = '#',
	DOLLAR_SIGN          = '$',
	PERCENT_SIGN         = '%',
	AMPERSAND            = '&',
	APOSTROPHE           = '\'',
	LEFT_PARENTHESIS     = '(',
	RIGHT_PARENTHESIS    = ')',
	ASTERISK             = '*',
	PLUS_SIGN            = '+',
	COMMA                = ',',
	HYPHEN_MINUS         = '-',
	FULL_STOP            = '.',
	SLASH                = '/',
	COLON                = ':',
	SEMICOLON            = ';',
	LESS_THAN_SIGN       = '<',
	EQUAL_SIGN           = '=',
	GREATER_THAN_SIGN    = '>',
	QUESTION_MARK        = '?',
	AT_SIGN              = '@',
	LEFT_SQUARE_BRACKET  = '[',
	BACKSLASH            = '\\',
	RIGHT_SQUARE_BRACKET = ']',
	CIRCUMFLEX_ACCENT    = '^',
	GRAVE_ACCENT         = '`',
	LEFT_CURLY_BRACKET   = '{',
	VERTICAL_BAR         = '|',
	RIGHT_CURLY_BRACKET  = '}',
	TILDE                = '~',
	NAME                 = 'A',
	BINARY,
	DIGITAL,
	HEXADECIMAL,
	DECIMAL,
	TEXT,
	EXCLAMATION_MARK_EQUAL_SIGN,
	PERCENT_SIGN_EQUAL_SIGN,
	AMPERSAND_EQUAL_SIGN,
	AMPERSAND_2,
	ASTERISK_EQUAL_SIGN,
	PLUS_SIGN_EQUAL_SIGN,
	HYPHEN_MINUS_EQUAL_SIGN,
	FULL_STOP_2,
	SLASH_EQUAL_SIGN,
	LESS_THAN_SIGN_EQUAL_SIGN,
	LESS_THAN_SIGN_2,
	LESS_THAN_SIGN_2_EQUAL_SIGN,
	EQUAL_SIGN_2,
	GREATER_THAN_SIGN_EQUAL_SIGN,
	GREATER_THAN_SIGN_2,
	GREATER_THAN_SIGN_2_EQUAL_SIGN,
	CIRCUMFLEX_ACCENT_EQUAL_SIGN,
	VERTICAL_BAR_EQUAL_SIGN,
	VERTICAL_BAR_2,
} Token_Tag;

static utf8 *representations_of_token_tags[] =
{
	[ETX]                            = "ETX",
	[EXCLAMATION_MARK]               = "`!`",
	[OCTOTHORPE]                     = "`#`",
	[DOLLAR_SIGN]                    = "`$`",
	[PERCENT_SIGN]                   = "`%`",
	[AMPERSAND]                      = "`&`",
	[APOSTROPHE]                     = "`'`",
	[LEFT_PARENTHESIS]               = "`(`",
	[RIGHT_PARENTHESIS]              = "`)`",
	[ASTERISK]                       = "`*`",
	[PLUS_SIGN]                      = "`+`",
	[COMMA]                          = "`,`",
	[HYPHEN_MINUS]                   = "`-`",
	[FULL_STOP]                      = "`.`",
	[SLASH]                          = "`/`",
	[COLON]                          = "`:`",
	[SEMICOLON]                      = "`;`",
	[LESS_THAN_SIGN]                 = "`<`",
	[EQUAL_SIGN]                     = "`=`",
	[GREATER_THAN_SIGN]              = "`>`",
	[QUESTION_MARK]                  = "`?`",
	[AT_SIGN]                        = "`@`",
	[LEFT_SQUARE_BRACKET]            = "`[`",
	[BACKSLASH]                      = "`\\`",
	[RIGHT_SQUARE_BRACKET]           = "`]`",
	[CIRCUMFLEX_ACCENT]              = "`^`",
	[GRAVE_ACCENT]                   = "```",
	[LEFT_CURLY_BRACKET]             = "`{`",
	[VERTICAL_BAR]                   = "`|`",
	[RIGHT_CURLY_BRACKET]            = "`}`",
	[TILDE]                          = "`~`",
	[NAME]                           = "name",
	[BINARY]                         = "binary",
	[DIGITAL]                        = "digital",
	[HEXADECIMAL]                    = "hexadecimal",
	[DECIMAL]                        = "decimal",
	[TEXT]                           = "text",
	[EXCLAMATION_MARK_EQUAL_SIGN]    = "`!=`",
	[PERCENT_SIGN_EQUAL_SIGN]        = "`%=`",
	[AMPERSAND_EQUAL_SIGN]           = "`&=`",
	[AMPERSAND_2]                    = "`&&`",
	[ASTERISK_EQUAL_SIGN]            = "`*=`",
	[PLUS_SIGN_EQUAL_SIGN]           = "`+=`",
	[HYPHEN_MINUS_EQUAL_SIGN]        = "`-=`",
	[FULL_STOP_2]                    = "`..`",
	[SLASH_EQUAL_SIGN]               = "`/=`",
	[LESS_THAN_SIGN_EQUAL_SIGN]      = "`<=`",
	[LESS_THAN_SIGN_2]               = "`<<`",
	[LESS_THAN_SIGN_2_EQUAL_SIGN]    = "`<<=`",
	[EQUAL_SIGN_2]                   = "`==`",
	[GREATER_THAN_SIGN_EQUAL_SIGN]   = "`>=`",
	[GREATER_THAN_SIGN_2]            = "`>>`",
	[GREATER_THAN_SIGN_2_EQUAL_SIGN] = "`>>=`",
	[CIRCUMFLEX_ACCENT_EQUAL_SIGN]   = "`^=`",
	[VERTICAL_BAR_EQUAL_SIGN]        = "`|=`",
	[VERTICAL_BAR_2]                 = "`||`"
};

typedef struct
{
	Token_Tag tag;
	Range     range;
} Token;

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

static utf32 peek_character(uint8 *increment, const Caret *caret)
{
	utf32 character;
	uint32 position = caret->location.position + caret->increment;
	if(position < caret->source->data_size)
	{
		utf8 bytes[4] = {};
		for(uint8 i = 0; i < sizeof(bytes) / sizeof(bytes[0]); i += 1)
		{
			bytes[i] = caret->source->data[position];
			if(position >= caret->source->data_size) break;
			position += 1;
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

static utf32 get_character(Caret *caret)
{
	uint8 increment;
	utf32 character = peek_character(&increment, caret);

	caret->location.position += caret->increment;
	if(caret->character == '\n')
	{
		caret->location.row += 1;
		caret->location.column = 0;
	}
	caret->location.column += 1;
	caret->character = character;
	caret->increment = increment;
	return character;
}

typedef enum
{
	VERBOSE,
	COMMENT,
	CAUTION,
	FAILURE,
} Severity;

static void report_v(Severity severity, const Source *source, const Range *range, const utf8 *message, vargs vargs)
{
	const utf8 *severities[] =
	{
		[VERBOSE] = "verbose",
		[COMMENT] = "comment",
		[CAUTION] = "caution",
		[FAILURE] = "failure",
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

static inline void report(Severity severity, const Source *source, const Range *range, const utf8 *message, ...)
{
	vargs vargs;
	GET_VARGS(vargs, message);
	report_v(severity, source, range, message, vargs);
	END_VARGS(vargs);
}

_Noreturn static inline void fail(const Source *source, const Range *range, const utf8 *message, ...)
{
	vargs vargs;
	GET_VARGS(vargs, message);
	report_v(FAILURE, source, range, message, vargs);
	END_VARGS(vargs);
	terminate(-1);
	UNREACHABLE();
}

static Token_Tag get_token(Token *token, Caret *caret)
{
	const utf8 *failure_message;

repeat:
	while(check_whitespace(caret->character)) get_character(caret);

	token->range.beginning = caret->location.position;
	token->range.row = caret->location.row;
	token->range.column = caret->location.column;

	if(check_letter(caret->character))
	{
		token->tag = NAME;
		do get_character(caret);
		while(check_letter(caret->character) || check_digital(caret->character) || caret->character == '-');
		// TODO(Emhyr): disallow names ending wiht `-`
	}
	else if(check_digital(caret->character))
	{
		bit (*checker)(utf32) = &check_digital;
		token->tag = DIGITAL;
		if(caret->character == '0')
		{
			switch(get_character(caret))
			{
			case 'b':
				token->tag = BINARY;
				checker = &check_binary;
				get_character(caret);
				break;
			case 'x':
				token->tag = HEXADECIMAL;
				checker = &check_hexadecimal;
				get_character(caret);
				break;
			default:
				break;
			}
		}

		while(checker(caret->character) || caret->character == '_')
		{
			if(get_character(caret) == '.')
			{
				switch(token->tag)
				{
				case DECIMAL:
				case BINARY:
				case HEXADECIMAL:
					failure_message = "weird ass number";
					goto failed;
				default:
					token->tag = DECIMAL;
					get_character(caret);
					break;
				}
			}
		}
	}
	else switch(caret->character)
	{
	case '\3':
		token->tag = ETX;
		break;
	case '"':
		for(;;) switch(get_character(caret))
		{
		case '\\':
			get_character(caret);
			break;
		case '\3':
			failure_message = "unterminated text";
			goto failed;
		case '"':
			goto text_terminated;
		}
	text_terminated:
		get_character(caret);
		token->tag = TEXT;
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
	case '.':
		uint8 peeked_increment;
		utf32 second_character = peek_character(&peeked_increment, caret);
		if(second_character == '=')
		{
			switch(caret->character)
			{
			case '!': token->tag = EXCLAMATION_MARK_EQUAL_SIGN;  break;
			case '%': token->tag = PERCENT_SIGN_EQUAL_SIGN;      break;
			case '&': token->tag = AMPERSAND_EQUAL_SIGN;         break;
			case '*': token->tag = ASTERISK_EQUAL_SIGN;          break;
			case '+': token->tag = PLUS_SIGN_EQUAL_SIGN;         break;
			case '-': token->tag = HYPHEN_MINUS_EQUAL_SIGN;      break;
			case '/': token->tag = SLASH_EQUAL_SIGN;             break;
			case '<': token->tag = LESS_THAN_SIGN_EQUAL_SIGN;    break;
			case '=': token->tag = EQUAL_SIGN_2;                 break;
			case '>': token->tag = GREATER_THAN_SIGN_EQUAL_SIGN; break;
			case '^': token->tag = CIRCUMFLEX_ACCENT_EQUAL_SIGN; break;
			case '|': token->tag = VERTICAL_BAR_EQUAL_SIGN;      break;
			}
			goto twice;
		}
		else if(second_character == caret->character)
		{
			switch(second_character)
			{
			case '&': token->tag = AMPERSAND_2;    break;
			case '|': token->tag = VERTICAL_BAR_2; break;
			case '.': token->tag = FULL_STOP_2;    break;
			case '<':
			case '>':
				get_character(caret);
				if(peek_character(&peeked_increment, caret) == '=')
				{
					switch(second_character)
					{
					case '<': token->tag = LESS_THAN_SIGN_2_EQUAL_SIGN;    break;
					case '>': token->tag = GREATER_THAN_SIGN_2_EQUAL_SIGN; break;
					}
				}
				else
				{
					switch(second_character)
					{
					case '<': token->tag = LESS_THAN_SIGN_2;    break;
					case '>': token->tag = GREATER_THAN_SIGN_2; break;
					}
				}
				break;
			}
			goto twice;
		}
		else goto single;

	case '#':
		if(get_character(caret) == ' ')
		{
			while(get_character(caret) != '\n');
			goto repeat;
		}
	case '$':
	case '\'':
	case '(':
	case ')':
	case ',':
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
	single:
		token->tag = caret->character;
		goto once;
	twice:
		get_character(caret);
	once:
		get_character(caret);
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

typedef enum
{
	INVOCATION,
	NEGATIVE,
	NEGATION,
	NOT,
	ADDRESS,
	INDIRECTION,
	JUMP,
	INFERENCE,
	
	LIST,
	RANGE,
	RESOLUTION,
	ADDITION,
	SUBTRACTION,
	MULTIPLICATION,
	DIVISION,
	REMAINDER,
	AND,
	OR,
	XOR,
	LSH,
	RSH,
	CONJUNCTION,
	DISJUNCTION,
	EQUALITY,
	INEQUALITY,
	MAJORITY,
	MINORITY,
	INCLUSIVE_MAJORITY,
	INCLUSIVE_MINORITY,
	
	ASSIGNMENT,
	ADDITION_ASSIGNMENT,
	SUBTRACTION_ASSIGNMENT,
	MULTIPLICATION_ASSIGNMENT,
	DIVISION_ASSIGNMENT,
	REMAINDER_ASSIGNMENT,
	AND_ASSIGNMENT,
	OR_ASSIGNMENT,
	XOR_ASSIGNMENT,
	LSH_ASSIGNMENT,
	RSH_ASSIGNMENT,
	
	CONDITION,

	VALUE,
	LABEL,
	ROUTINE,
	SCOPE,

	INTEGER,
	REAL,
	STRING,
	REFERENCE,

	SUBEXPRESSION,
} Node_Tag;

static const utf8 *representations_of_node_tags[] =
{
	[INVOCATION]                = "invocation",
	[NEGATIVE]                  = "negative",
	[NEGATION]                  = "negation",
	[NOT]                       = "NOT",
	[ADDRESS]                   = "address",
	[INDIRECTION]               = "indirection",
	[JUMP]                      = "jump",
	[INFERENCE]                 = "inference",
	[LIST]                      = "list",
	[RANGE]                     = "range",
	[RESOLUTION]                = "resolution",
	[ADDITION]                  = "addition",
	[SUBTRACTION]               = "subtraction",
	[MULTIPLICATION]            = "multiplication",
	[DIVISION]                  = "division",
	[REMAINDER]                 = "remainder",
	[AND]                       = "AND",
	[OR]                        = "OR",
	[XOR]                       = "XOR",
	[LSH]                       = "LSH",
	[RSH]                       = "RSH",
	[CONJUNCTION]               = "conjunction",
	[DISJUNCTION]               = "disjunction",
	[EQUALITY]                  = "equality",
	[INEQUALITY]                = "inequality",
	[MAJORITY]                  = "majority",
	[MINORITY]                  = "minority",
	[INCLUSIVE_MAJORITY]        = "inclusive majority",
	[INCLUSIVE_MINORITY]        = "inclusive minority",
	[ASSIGNMENT]                = "assignment",
	[ADDITION_ASSIGNMENT]       = "addition assignment",
	[SUBTRACTION_ASSIGNMENT]    = "subtraction assignment",
	[MULTIPLICATION_ASSIGNMENT] = "multiplication assignment",
	[DIVISION_ASSIGNMENT]       = "division assignment",
	[REMAINDER_ASSIGNMENT]      = "remainder assignment",
	[AND_ASSIGNMENT]            = "AND assignment",
	[OR_ASSIGNMENT]             = "OR assignment",
	[XOR_ASSIGNMENT]            = "XOR assignment",
	[LSH_ASSIGNMENT]            = "LSH assignment",
	[RSH_ASSIGNMENT]            = "RSH assignment",
	[CONDITION]                 = "condition",
	[VALUE]                     = "value",
	[LABEL]                     = "label",
	[ROUTINE]                   = "routine",
	[SCOPE]                     = "scope",
	[INTEGER]                   = "integer",
	[REAL]                      = "real",
	[STRING]                    = "string",
	[REFERENCE]                 = "reference",
	[SUBEXPRESSION]             = "subexpression",
};

typedef uint8 Precedence;

enum : uint8
{
	TYPE_PRECEDENCE       = 100,
	ASSIGNMENT_PRECEDENCE,
};

static Precedence precedences[] =
{
	[RESOLUTION]                = 14,
	
	[INVOCATION]                = 13,
	[NEGATIVE]                  = 13,
	[NEGATION]                  = 13,
	[NOT]                       = 13,
	[ADDRESS]                   = 13,
	[INDIRECTION]               = 13,
	[JUMP]                      = 13,
	[INFERENCE]                 = 13,
	
	[MULTIPLICATION]            = 12,
	[DIVISION]                  = 12,
	[REMAINDER]                 = 12,
	
	[ADDITION]                  = 11,
	[SUBTRACTION]               = 11,
	
	[LSH]                       = 10,
	[RSH]                       = 10,
	
	[MAJORITY]                  = 9,
	[MINORITY]                  = 9,
	[INCLUSIVE_MAJORITY]        = 9,
	[INCLUSIVE_MINORITY]        = 9,
	
	[EQUALITY]                  = 8,
	[INEQUALITY]                = 8,
	
	[AND]                       = 7,
	
	[XOR]                       = 6,
	
	[OR]                        = 5,
	
	[CONJUNCTION]               = 4,
	
	[DISJUNCTION]               = 3,
	
	[RANGE]                     = 2,
	[CONDITION]                 = 2,
	[ASSIGNMENT]                = 2,
	[ADDITION_ASSIGNMENT]       = 2,
	[SUBTRACTION_ASSIGNMENT]    = 2,
	[MULTIPLICATION_ASSIGNMENT] = 2,
	[DIVISION_ASSIGNMENT]       = 2,
	[REMAINDER_ASSIGNMENT]      = 2,
	[AND_ASSIGNMENT]            = 2,
	[OR_ASSIGNMENT]             = 2,
	[XOR_ASSIGNMENT]            = 2,
	[LSH_ASSIGNMENT]            = 2,
	[RSH_ASSIGNMENT]            = 2,
	
	[LIST]                      = 1,
};

typedef struct Node Node;

typedef struct
{
	const utf8 *value;
	uint32      size;
} Identifier;

typedef struct
{
	Identifier identifier;
	Node      *type;
	Node      *assignment;
	bit        constant : 1;
} Value;

typedef struct
{
	Identifier identifier;
	uint32     position;
} Label;

typedef struct Routine Routine;

typedef struct Scope Scope;

struct Scope
{
	Scope   *parent;
	Routine *owner;
	Value   *values;
	Label   *labels;
	Routine *routines;
	Node   **statements;
	uint32   values_count;
	uint32   labels_count;
	uint32   routines_count;
	uint32   statements_count;
};

struct Routine
{
	Identifier identifier;
	Value     *parameters;
	uint32     parameters_count;
	uint32     arguments_count;
	Scope      scope;
};

typedef struct
{
	uint64 value;
} Integer;

typedef struct
{
	real64 value;
} Real;

typedef struct
{
	uint8 *value;
	uint32 size;
} String;

struct Node
{
	Node_Tag tag;
	Range    range;
	union
	{
		Identifier identifier;
		Integer    integer;
		Real       real;
		String     string;
		Node      *unary;
		Node      *binary[2];
		Node      *ternary[3];
		Value     *value;
		Scope      scope;
	} data[];
};

void parse_integer(Integer *integer, Token *token, Caret *caret)
{
	ASSERT(token->tag == BINARY || token->tag == DIGITAL || token->tag == HEXADECIMAL);

	uint8 base;
	switch(token->tag)
	{
	case BINARY:
		base = 2;
		break;
	case DIGITAL:
		base = 10;
		break;
	case HEXADECIMAL:
		base = 16;
		break;
	default:
		UNREACHABLE();
	}

	integer->value = 0;
	for(const utf8 *pointer = caret->source->data + token->range.beginning,
	               *ending  = caret->source->data + token->range.ending;
	    pointer < ending;
	    pointer += 1)
	{
		integer->value = integer->value * base + *pointer - (*pointer >= '0' && *pointer <= '9' ? '0' : *pointer >= 'A' && *pointer <= 'F' ? 'A' : 'a');
	}

	get_token(token, caret);
}

#include <stdlib.h> // TODO(Emhyr): ditch <stdlib.h>

void parse_real(Real *real, Token *token, Caret *caret)
{
	ASSERT(token->tag == DECIMAL);
	char *ending;
	real->value = strtod(caret->source->data + token->range.beginning, &ending);
	get_token(token, caret);
}

void parse_string(String *string, Token *token, Caret *caret)
{
	ASSERT(token->tag == TEXT);
	const uint8 *input  = (uint8 *)caret->source->data + token->range.beginning + 1;
	const uint8 *ending = (uint8 *)caret->source->data + token->range.ending;
	if(input == ending) fail(caret->source, &token->range, "empty string");
	ending -= 1;
	string->value = allocate_memory(ending - input);
	uint8 *output = string->value;
	while(input < ending)
	{
		uint8 byte = *input;
		input += 1;
		if(byte == '\\')
		{
			uint8 escape = *input;
			input += 1;
			switch(escape)
			{
			case 'b':
				byte = 0x7;
				break;
			case 'f':
				byte = 0xc;
				break;
			case 'n':
				byte = 0xa;
				break;
			case 'r':
				byte = 0xd;
				break;
			case 't':
				byte = 0x9;
				break;
			case 'v':
				byte = 0xb;
				break;
			default:
				uint8 buffer = byte;
				if(check_digital(byte))
				{
					buffer = 0;
					do
					{
						buffer = buffer * 10 + '0' - byte;
						byte = *input;
						input += 1;
					}
					while(check_digital(byte));
					byte = buffer;
				}
				break;
			}
		}
		*output = byte;
		output += 1;
	}
	string->size = output - string->value;
	get_token(token, caret);
}

void parse_identifier(Identifier *Identifier, Token *token, Caret *caret)
{
	ASSERT(token->tag == NAME);
	Identifier->value = caret->source->data + token->range.beginning;
	Identifier->size = token->range.ending - token->range.beginning;
	get_token(token, caret);
}

Node *parse_expression(Precedence other_precedence, Token *token, Caret *caret, Buffer *buffer)
{
	uint32 beginning = token->range.beginning;
	uint32 row = token->range.row;
	uint32 column = token->range.column;

	Node *left = 0;
	switch(token->tag)
	{
	case BINARY:
	case DIGITAL:
	case HEXADECIMAL:
		left = push_into_buffer(sizeof(Node) + sizeof(Integer), alignof(Node), buffer);
		left->tag = INTEGER;
		parse_integer(&left->data->integer, token, caret);
		break;
		// TODO(Emhyr): allow scientific and hex notation
	case DECIMAL:
		left = push_into_buffer(sizeof(Node) + sizeof(Real), alignof(Node), buffer);
		left->tag = REAL;
		parse_real(&left->data->real, token, caret);
		break;
	case TEXT:
		left = push_into_buffer(sizeof(Node) + sizeof(String), alignof(Node), buffer);
		left->tag = STRING;
		parse_string(&left->data->string, token, caret);
		break;
	case NAME:
		left = push_into_buffer(sizeof(Node) + sizeof(Identifier), alignof(Node), buffer);
		left->tag = REFERENCE;
		parse_identifier(&left->data->identifier, token, caret);
		break;
	case LEFT_PARENTHESIS:
		left = push_into_buffer(sizeof(Node) + sizeof(Node *), alignof(Node), buffer);
		left->tag = SUBEXPRESSION;
		get_token(token, caret);
		left->data->unary = parse_expression(0, token, caret, buffer);
		if(token->tag == RIGHT_PARENTHESIS) get_token(token, caret);
		else fail(caret->source, &token->range, "unterminated scope; expected `%c`", left->tag == SUBEXPRESSION ? ')' : ']');
		break;

		Node_Tag left_tag;
	case HYPHEN_MINUS:      left_tag = NEGATIVE;    goto unary;
	case EXCLAMATION_MARK:  left_tag = NEGATION;    goto unary;
	case TILDE:             left_tag = NOT;         goto unary;
	case AT_SIGN:           left_tag = ADDRESS;     goto unary;
	case BACKSLASH:         left_tag = INDIRECTION; goto unary;
	case CIRCUMFLEX_ACCENT: left_tag = JUMP;        goto unary;
	case APOSTROPHE:        left_tag = INFERENCE;   goto unary;
	unary:
		left = push_into_buffer(sizeof(Node) + sizeof(Node *), alignof(Node), buffer);
		left->tag = left_tag;
		get_token(token, caret);
		left->data->unary = parse_expression(precedences[left_tag], token, caret, buffer);
		break;

	case COLON:
	case SEMICOLON:
	case RIGHT_PARENTHESIS:
	case LEFT_CURLY_BRACKET:
	case RIGHT_CURLY_BRACKET:
		goto finished;

	default:
		fail(caret->source, &token->range, "unexpected token");
	}

	left->range.beginning = beginning;
	left->range.ending = token->range.ending;
	left->range.row = row;
	left->range.column = column;

	for(;;)
	{
		Node *right;
		switch(token->tag)
		{
			Node_Tag right_tag;
		case COMMA:                           right_tag = LIST;                      goto binary;
		case FULL_STOP:                       right_tag = RESOLUTION;                goto binary;
		case FULL_STOP_2:                     right_tag = RANGE;                     goto binary;
		case PLUS_SIGN:                       right_tag = ADDITION;                  goto binary;
		case HYPHEN_MINUS:                    right_tag = SUBTRACTION;               goto binary;
		case ASTERISK:                        right_tag = MULTIPLICATION;            goto binary;
		case SLASH:                           right_tag = DIVISION;                  goto binary;
		case PERCENT_SIGN:                    right_tag = REMAINDER;                 goto binary;
		case AMPERSAND:                       right_tag = AND;                       goto binary;
		case VERTICAL_BAR:                    right_tag = OR;                        goto binary;
		case CIRCUMFLEX_ACCENT:               right_tag = XOR;                       goto binary;
		case LESS_THAN_SIGN_2:                right_tag = LSH;                       goto binary;
		case GREATER_THAN_SIGN_2:             right_tag = RSH;                       goto binary;
		case AMPERSAND_2:                     right_tag = CONJUNCTION;               goto binary;
		case VERTICAL_BAR_2:                  right_tag = DISJUNCTION;               goto binary;
		case EQUAL_SIGN_2:                    right_tag = EQUALITY;                  goto binary;
		case EXCLAMATION_MARK_EQUAL_SIGN:     right_tag = INEQUALITY;                goto binary;
		case GREATER_THAN_SIGN:               right_tag = MAJORITY;                  goto binary;
		case LESS_THAN_SIGN:                  right_tag = MINORITY;                  goto binary;
		case GREATER_THAN_SIGN_EQUAL_SIGN:    right_tag = INCLUSIVE_MAJORITY;        goto binary;
		case LESS_THAN_SIGN_EQUAL_SIGN:       right_tag = INCLUSIVE_MINORITY;        goto binary;
		case EQUAL_SIGN:                      right_tag = ASSIGNMENT;                goto binary;
		case PLUS_SIGN_EQUAL_SIGN:            right_tag = ADDITION_ASSIGNMENT;       goto binary;
		case HYPHEN_MINUS_EQUAL_SIGN:         right_tag = SUBTRACTION_ASSIGNMENT;    goto binary;
		case ASTERISK_EQUAL_SIGN:             right_tag = MULTIPLICATION_ASSIGNMENT; goto binary;
		case SLASH_EQUAL_SIGN:                right_tag = DIVISION_ASSIGNMENT;       goto binary;
		case PERCENT_SIGN_EQUAL_SIGN:         right_tag = REMAINDER_ASSIGNMENT;      goto binary;
		case AMPERSAND_EQUAL_SIGN:            right_tag = AND_ASSIGNMENT;            goto binary;
		case VERTICAL_BAR_EQUAL_SIGN:         right_tag = OR_ASSIGNMENT;             goto binary;
		case CIRCUMFLEX_ACCENT_EQUAL_SIGN:    right_tag = XOR_ASSIGNMENT;            goto binary;
		case LESS_THAN_SIGN_2_EQUAL_SIGN:     right_tag = LSH_ASSIGNMENT;            goto binary;
		case GREATER_THAN_SIGN_2_EQUAL_SIGN:  right_tag = RSH_ASSIGNMENT;            goto binary;
		default:                              right_tag = INVOCATION;                goto binary;
		case QUESTION_MARK:                   right_tag = CONDITION;                 goto binary;
		binary:
			if(other_precedence == TYPE_PRECEDENCE)
			{
				if(right_tag >= ASSIGNMENT && right_tag <= RSH_ASSIGNMENT || right_tag == LIST) goto finished;
				else other_precedence = 0;
			}
			else if(other_precedence == ASSIGNMENT_PRECEDENCE)
			{
				if(right_tag == LIST) goto finished;
				else other_precedence = 0;
			}
			Precedence right_precedence = precedences[right_tag];
			if(right_precedence <= other_precedence) goto finished;
			if(right_tag != INVOCATION) get_token(token, caret);
			right = push_into_buffer(sizeof(Node) + (right_tag == CONDITION ? sizeof(Node *[3]) : sizeof(Node *[2])), alignof(Node), buffer);
			right->tag = right_tag;
			right->data->binary[0] = left;
			right->data->binary[1] = parse_expression(right_tag == CONDITION ? 0 : right_precedence, token, caret, buffer);
			if(right_tag == CONDITION)
			{
				if(token->tag == EXCLAMATION_MARK)
				{
					get_token(token, caret);
					right->data->ternary[2] = parse_expression(right_precedence, token, caret, buffer);
				}
				else right->data->ternary[2] = 0;
			}
			break;
			
		case COLON:
		case SEMICOLON:
		case RIGHT_PARENTHESIS:
		case LEFT_CURLY_BRACKET:
		case RIGHT_CURLY_BRACKET:
		case EXCLAMATION_MARK:
			goto finished;

		case ETX:
			fail(caret->source, &token->range, "unfinished expression");
		}

		right->range.beginning = beginning;
		right->range.ending = token->range.ending;
		right->range.row = row;
		right->range.column = column;
		left = right;
	}

finished:
	return left;
}

void parse_value(Value *value, Token *token, Caret *caret, Buffer *buffer)
{
	ASSERT(token->tag == NAME);
	parse_identifier(&value->identifier, token, caret);
	if(token->tag != COLON) fail(caret->source, &token->range, "expected `:`");
	get_token(token, caret);
	switch(token->tag)
	{
	case EQUAL_SIGN:
	case COLON:
		break;
	default:
		value->type = parse_expression(TYPE_PRECEDENCE, token, caret, buffer);
		break;
	}
	value->constant = 0;
	switch(token->tag)
	{
	case COLON:
		value->constant = 1;
	case EQUAL_SIGN:
		get_token(token, caret);
		value->assignment = parse_expression(ASSIGNMENT_PRECEDENCE, token, caret, buffer);
		break;
	default:
		if(!value->type) fail(caret->source, &token->range, "untyped and uninitialized value");
		break;
	}
}

void parse_scope(Scope *scope, Scope *parent, Routine *owner, Token *token, Caret *caret)
{
	ASSERT(token->tag == LEFT_CURLY_BRACKET);

	scope->parent = parent;
	scope->owner = owner;
	
	// TODO(Emhyr): ditch `allocate_buffer`
	Buffer *values = allocate_buffer(GIBIBYTE(1), system_page_size);
	Buffer *labels = allocate_buffer(GIBIBYTE(1), system_page_size);
	Buffer *routines = allocate_buffer(GIBIBYTE(1), system_page_size);
	Buffer *statements = allocate_buffer(GIBIBYTE(1), system_page_size);
	Buffer *buffer = allocate_buffer(GIBIBYTE(1), system_page_size);
	scope->values = (Value *)values->base;
	scope->labels = (Label *)labels->base;
	scope->routines = (Routine *)routines->base; scope->statements = (Node **)statements->base;
	scope->values_count = 0;
	scope->labels_count = 0;
	scope->routines_count = 0;
	scope->statements_count = 0;
	get_token(token, caret);
	for(;;)
	{
		Node *node;
		Token onsetting_token = *token;
		Caret onsetting_caret = *caret;
		switch(onsetting_token.tag)
		{
		case NAME:
			if(get_token(token, caret) == COLON)
			{
				// TODO(Emhyr): parse modules
				*token = onsetting_token;
				*caret = onsetting_caret;
				Value *value = push_into_buffer(sizeof(Value), alignof(Value), values);
				++scope->values_count;
				parse_value(value, token, caret, buffer);
				if(value->assignment && !value->constant)
				{
					node = push_into_buffer(sizeof(Node) + sizeof(Value *), alignof(Node), buffer);
					node->tag = VALUE;
					node->data->value = value;
					goto statement;
				}
			}
			else
			{
				*token = onsetting_token;
				*caret = onsetting_caret;
				goto expression;
			}
			continue;
	
		case FULL_STOP:
			if(get_token(token, caret) == NAME)
			{
				Identifier identifier;
				parse_identifier(&identifier, token, caret);
				if(token->tag == COLON)
				{
					Routine *routine = push_into_buffer(sizeof(Routine), alignof(Routine), routines);
					++scope->routines_count;
					routine->identifier = identifier;
					get_token(token, caret);
					Value *parameter;
					Buffer *parameters = 0;
					if(token->tag == LEFT_PARENTHESIS)
					{
						parameters = allocate_buffer(MEBIBYTE(1), system_page_size);
						get_token(token, caret);
						for(;;)
						{
							switch(token->tag)
							{
							case NAME:
								parameter = push_into_buffer(sizeof(Value), alignof(Value), parameters);
								++routine->parameters_count;
								parse_value(parameter, token, caret, buffer);
								break;
							case COMMA:
								get_token(token, caret);
								break;
							case RIGHT_PARENTHESIS:
								get_token(token, caret);
								goto finished_arguments;
							default:
								fail(caret->source, &token->range, "expected name, `,`, or `)`");
								break;
							}
						}
					finished_arguments:
					}
					routine->arguments_count = routine->parameters_count;
					switch(token->tag)
					{
					case SEMICOLON:
					case LEFT_CURLY_BRACKET:
						break;
					default:
						if(!parameters) parameters = allocate_buffer(MEBIBYTE(1), system_page_size);
						for(;;)
						{
							switch(token->tag)
							{
							case NAME:
								parameter = push_into_buffer(sizeof(Value), alignof(Value), parameters);
								++routine->parameters_count;
								parse_value(parameter, token, caret, buffer);
								break;
							case COMMA:
								get_token(token, caret);
								break;
							case LEFT_CURLY_BRACKET:
								goto finished_results;
							default:
								fail(caret->source, &token->range, "expected name, `,`, or `{`");
								break;
							}
						}
					finished_results:
						break;
					}
					routine->parameters = (Value *)parameters->base;
					if(token->tag == LEFT_CURLY_BRACKET) parse_scope(&routine->scope, scope, routine, token, caret);
				}
				else
				{
					Label *label = push_into_buffer(sizeof(Node) + sizeof(Label), alignof(Label), labels);
					++scope->labels_count;
					label->identifier = identifier;
					label->position = scope->statements_count;
				}
			}
			else fail(caret->source, &token->range, "unexpected token; expected name");
			continue;

		case LEFT_CURLY_BRACKET:
			node = push_into_buffer(sizeof(Node) + sizeof(Scope), alignof(Node), buffer);
			node->tag = SCOPE;
			parse_scope(&node->data->scope, scope, 0, token, caret);
			break;

		default:
		expression:
			node = parse_expression(0, token, caret, buffer);
			break;
	
		case SEMICOLON:
			get_token(token, caret);
			continue;
		case RIGHT_CURLY_BRACKET:
			get_token(token, caret);
			goto finished;
		}

	statement:
		Node **statement = push_into_buffer(sizeof(Node *), alignof(Node *), statements);
		*statement = node;
		++scope->statements_count;
	}

finished:
}

typedef enum
{
	VOID,
	UINT8,
	UINT16,
	UINT32,
	UINT64,
	SINT8,
	SINT16,
	SINT32,
	SINT64,
	REAL32,
	REAL64,
	POINTER,
	ARRAY,
	TUPLE,
} Type_Class;

typedef struct Type Type;

typedef struct
{
	Type *subtype;
} Address;

typedef struct
{
	Type  *subtype;
	uint64 count;
} Array;

typedef struct
{
	Type **subtypes;
	uint64 count;
} Tuple;

struct Type
{
	Type_Class class;
	union
	{
		Address pointer;
		Array   array;
		Tuple   tuple;
	} data[];
};

struct
{
	Type uint8;
	Type uint16;
	Type uint32;
	Type uint64;
	Type sint8;
	Type sint16;
	Type sint32;
	Type sint64;
	Type real32;
	Type real64;
} primitives =
{
	{UINT8},
	{UINT16},
	{UINT32},
	{UINT64},
	{SINT8},
	{SINT16},
	{SINT32},
	{SINT64},
	{REAL32},
	{REAL64},
};

typedef struct
{
	Type *pointers;
	Type *arrays;
	Type *tuples;
	uint32 pointers_count;
	uint32 arrays_count;
	uint32 tuples_count;
} Type_Table;

typedef struct
{
	Buffer *pointers;
	Buffer *arrays;
	Buffer *tuples;
} Type_Buffers;

Type *check_expression(Node *node, Type_Table *table, Buffer *buffer)
{
	Type *type = 0;
	switch(node->tag)
	{
	case INTEGER:
		     if(node->data->integer.value <= UMAXOF(uint8))  type = &primitives.uint8;
		else if(node->data->integer.value <= UMAXOF(uint16)) type = &primitives.uint16;
		else if(node->data->integer.value <= UMAXOF(uint32)) type = &primitives.uint32;
		else                                                 type = &primitives.uint64;
		break;
	case REAL:
		type = &primitives.real32;
		break;
	case STRING:
		{
			Array comparitor =
			{
				.subtype = &primitives.uint8,
				.count   = node->value->string.size,
			};
			bit found = 0;
			for(uint32 i = 0; i < table->arrays_count; ++i)
			{
				Array *array = table->arrays + i;
				found = !compare_memory(array, &comparitor, sizeof(Array));
			}
			if(!found)
			{
				type = push_into_buffer(sizeof(Array), alignof(Type), buffer);
			}
		}
		break;
	case REFERENCE:
		break;
	default:
		UNIMPLEMENTED();
	}
	return type;
}

typedef struct Module Module;

struct Module
{
	Value   *values;
	Label   *labels;
	Routine *routines;
	uint32   values_count;
	uint32   labels_count;
	uint32   routines_count;
};

void dump(Node *);

void dump_value(Value *value)
{
	print("{\"identifier\":\"%.*s\"", value->identifier.size, value->identifier.value);
	if(value->type) print(",\"type\":"), dump(value->type);
	if(value->assignment) print(",\"assignment\":"), dump(value->assignment);
	print(",\"constant\":%i}", value->constant);
}

void dump_label(Label *label)
{
	print("{\"identifier\":\"%.*s\"", label->identifier.size, label->identifier.value);
	print(",\"position\":%i}", label->position);
}

void dump_scope(Scope *scope);

void dump_routine(Routine *routine)
{
	print("{\"identifier\":\"%.*s\"", routine->identifier.size, routine->identifier.value);
	print(",\"arguments\":[");
	if(routine->parameters_count)
	{
		for(uint32 i = 0; i < routine->parameters_count - 1; ++i)
		{
			Value *value = routine->parameters + i;
			dump_value(value);
			print(",");
		}
		dump_value(routine->parameters + routine->parameters_count - 1);
	}
	print("],\"scope\":");
	dump_scope(&routine->scope);
	print("}");
}

void dump_scope(Scope *scope)
{
	print("{");
	print("\"values\":[");
	if(scope->values_count)
	{
		for(uint32 i = 0; i < scope->values_count - 1; ++i)
		{
			Value *value = scope->values + i;
			dump_value(value);
			print(",");
		}
		dump_value(scope->values + scope->values_count - 1);
	}
	print("],\"labels\":[");
	if(scope->labels_count)
	{
		for(uint32 i = 0; i < scope->labels_count - 1; ++i)
		{
			Label *Label = scope->labels + i;
			dump_label(Label);
			print(",");
		}
		dump_label(scope->labels + scope->labels_count - 1);
	}
	print("],\"routines\":[");
	if(scope->routines_count)
	{
		for(uint32 i = 0; i < scope->routines_count - 1; ++i)
		{
			Routine *Routine = scope->routines + i;
			dump_routine(Routine);
			print(",");
		}
		dump_routine(scope->routines + scope->routines_count - 1);
	}
	print("],\"statements\":[");
	if(scope->statements_count)
	{
		for(uint32 i = 0; i < scope->statements_count - 1; ++i)
		{
			Node *statement = scope->statements[i];
			dump(statement);
			print(",");
		}
		dump(scope->statements[scope->statements_count - 1]);
	}
	print("]}");
}

void dump(Node *node)
{
	print("{");
	if(node)
	{
		const utf8 *representation = representations_of_node_tags[node->tag];

		print("\"%s\":", representation);
	
		switch(node->tag)
		{
		case SUBEXPRESSION:
		case NEGATIVE:
		case NEGATION:
		case NOT:
		case ADDRESS:
		case INDIRECTION:
		case JUMP:
		case INFERENCE:
			dump(node->data->unary);
			break;

		case INVOCATION:
		case LIST:
		case RANGE:
		case RESOLUTION:
		case ADDITION:
		case SUBTRACTION:
		case MULTIPLICATION:
		case DIVISION:
		case REMAINDER:
		case AND:
		case OR:
		case XOR:
		case LSH:
		case RSH:
		case CONJUNCTION:
		case DISJUNCTION:
		case EQUALITY:
		case INEQUALITY:
		case MAJORITY:
		case MINORITY:
		case INCLUSIVE_MAJORITY:
		case INCLUSIVE_MINORITY:
		case ASSIGNMENT:
		case ADDITION_ASSIGNMENT:
		case SUBTRACTION_ASSIGNMENT:
		case MULTIPLICATION_ASSIGNMENT:
		case DIVISION_ASSIGNMENT:
		case REMAINDER_ASSIGNMENT:
		case AND_ASSIGNMENT:
		case OR_ASSIGNMENT:
		case XOR_ASSIGNMENT:
		case LSH_ASSIGNMENT:
		case RSH_ASSIGNMENT:
			print("[");
			dump(node->data->binary[0]);
			print(",");
			dump(node->data->binary[1]);
			print("]");
			break;
	
		case CONDITION:
			print("[");
			dump(node->data->ternary[0]);
			print(",");
			dump(node->data->ternary[1]);
			print(",");
			dump(node->data->ternary[2]);
			print("]");
			break;

		case VALUE:
			dump_value(node->data->value);
			break;
		case SCOPE:
			dump_scope(&node->data->scope);
			break;

		case INTEGER:
			print("%lu", node->data->integer.value);
			break;
		case REAL:
			print("%f", node->data->real.value);
			break;
		case STRING:
			print("\"%.*s\"", node->data->string.size, node->data->string.value);
			break;
		case REFERENCE:
			print("\"%.*s\"", node->data->identifier.size, node->data->identifier.value);
			break;

		default:
			UNIMPLEMENTED();
		}
	}
	print("}");
}

int start(int argc, utf8 *argv[])
{
	int status = 0;
	if(argc > 1)
	{
		Source source;
		load_source(&source, argv[1]);
		Caret caret = {&source, {0, 0, 0}, '\n', 0};
		get_character(&caret);
		Token token;
		get_token(&token, &caret);
#if 1
		Scope Scope;
		parse_scope(&Scope, 0, 0, &token, &caret);
		dump_scope(&Scope);
#else
		buffer *buffer = allocate_buffer(GIBIBYTE(1), system_page_size);
		print("{\"statements\":[");
		while(token.tag != ETX)
		{
			node *node = parse_expression(0, &token, &caret, buffer);
			dump(node);
			print(",\n");
			//report(VERBOSE, &source, &node->range, "%s", representations_of_node_tags[node->tag]);
			if(token.tag == SEMICOLON) get_token(&token, &caret);
		}
		print("]}");
#endif
		
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
} Dfa_Utf8_State;

static inline uint16 dfa_decode_utf8(Dfa_Utf8_State *state, utf32 *character, utf8 byte)
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
	Dfa_Utf8_State state = DFA_UTF8_STATE_ACCEPT;
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

static inline void fill_memory(void *buffer, uint32 size, uint8 Value)
{
	__builtin_memset(buffer, Value, size);
}

static inline void zero_memory(void *buffer, uint32 size)
{
	fill_memory(buffer, size, 0);
}

static inline sint16 compare_memory(const void *left, const void *right, uint32 size)
{
	return __builtin_memcmp(left, right, size);
}

static inline uint64 get_size_of_string(const utf8 *String)
{
	return __builtin_strlen(String);
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

static Buffer *allocate_buffer(uint32 reservation_size, uint32 commission_rate)
{
	reservation_size += get_forward_alignment(reservation_size, system_page_size);
	commission_rate += get_forward_alignment(commission_rate, system_page_size);
	Buffer *buffer = reserve_memory(reservation_size);
	commit_memory(buffer, commission_rate);
	buffer->memory           = (uint8 *)buffer;
	buffer->reservation_size = reservation_size;
	buffer->commission_rate  = commission_rate;
	buffer->commission_size  = commission_rate;
	buffer->mass             = sizeof(Buffer);
	return buffer;
}

static inline void deallocate_buffer(Buffer *buffer)
{
	release_memory(buffer, buffer->reservation_size);
}

static void *push_into_buffer(uint32 size, uint32 alignment, Buffer *buffer)
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
	void *result = buffer->memory + buffer->mass;
	zero_memory(result, size);
	buffer->mass += size;
	return result;
}


