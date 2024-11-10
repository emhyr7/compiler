#include "compiler.h"

#define UNIMPLEMENTED() ({ ASSERT(!"unimplemented"); UNREACHABLE(); })

#define KIBIBYTE(x) ((uint64)(x) << 10)
#define MEBIBYTE(x) (KIBIBYTE(x) << 10)
#define GIBIBYTE(x) (MEBIBYTE(x) << 10)

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
	utf8 *data;
	uint32 path_size;
	uint32 data_size;
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
	Location location;
	utf32 character;
	uint8 increment;
} Caret;

// NOTE(Emhyr): didn't feel like using X macros :/. fingers hurt
typedef enum : uint8
{
	TOKEN_TAG_ETX                  = '\3',
	TOKEN_TAG_EXCLAMATION_MARK     = '!',
	TOKEN_TAG_OCTOTHORPE           = '#',
	TOKEN_TAG_DOLLAR_SIGN          = '$',
	TOKEN_TAG_PERCENT_SIGN         = '%',
	TOKEN_TAG_AMPERSAND            = '&',
	TOKEN_TAG_APOSTROPHE           = '\'',
	TOKEN_TAG_LEFT_PARENTHESIS     = '(',
	TOKEN_TAG_RIGHT_PARENTHESIS    = ')',
	TOKEN_TAG_ASTERISK             = '*',
	TOKEN_TAG_PLUS_SIGN            = '+',
	TOKEN_TAG_COMMA                = ',',
	TOKEN_TAG_HYPHEN_MINUS         = '-',
	TOKEN_TAG_FULL_STOP            = '.',
	TOKEN_TAG_SLASH                = '/',
	TOKEN_TAG_COLON                = ':',
	TOKEN_TAG_SEMICOLON            = ';',
	TOKEN_TAG_LESS_THAN_SIGN       = '<',
	TOKEN_TAG_EQUAL_SIGN           = '=',
	TOKEN_TAG_GREATER_THAN_SIGN    = '>',
	TOKEN_TAG_QUESTION_MARK        = '?',
	TOKEN_TAG_AT_SIGN              = '@',
	TOKEN_TAG_LEFT_SQUARE_BRACKET  = '[',
	TOKEN_TAG_BACKSLASH            = '\\',
	TOKEN_TAG_RIGHT_SQUARE_BRACKET = ']',
	TOKEN_TAG_CIRCUMFLEX_ACCENT    = '^',
	TOKEN_TAG_GRAVE_ACCENT         = '`',
	TOKEN_TAG_LEFT_CURLY_BRACKET   = '{',
	TOKEN_TAG_VERTICAL_BAR         = '|',
	TOKEN_TAG_RIGHT_CURLY_BRACKET  = '}',
	TOKEN_TAG_TILDE                = '~',
	TOKEN_TAG_NAME                 = 'A',
	TOKEN_TAG_BINARY,
	TOKEN_TAG_DIGITAL,
	TOKEN_TAG_HEXADECIMAL,
	TOKEN_TAG_DECIMAL,
	TOKEN_TAG_TEXT,
	TOKEN_TAG_EXCLAMATION_MARK_EQUAL_SIGN,
	TOKEN_TAG_PERCENT_SIGN_EQUAL_SIGN,
	TOKEN_TAG_AMPERSAND_EQUAL_SIGN,
	TOKEN_TAG_AMPERSAND_2,
	TOKEN_TAG_ASTERISK_EQUAL_SIGN,
	TOKEN_TAG_PLUS_SIGN_EQUAL_SIGN,
	TOKEN_TAG_HYPHEN_MINUS_EQUAL_SIGN,
	TOKEN_TAG_FULL_STOP_2,
	TOKEN_TAG_SLASH_EQUAL_SIGN,
	TOKEN_TAG_LESS_THAN_SIGN_EQUAL_SIGN,
	TOKEN_TAG_LESS_THAN_SIGN_2,
	TOKEN_TAG_LESS_THAN_SIGN_2_EQUAL_SIGN,
	TOKEN_TAG_EQUAL_SIGN_2,
	TOKEN_TAG_GREATER_THAN_SIGN_EQUAL_SIGN,
	TOKEN_TAG_GREATER_THAN_SIGN_2,
	TOKEN_TAG_GREATER_THAN_SIGN_2_EQUAL_SIGN,
	TOKEN_TAG_CIRCUMFLEX_ACCENT_EQUAL_SIGN,
	TOKEN_TAG_VERTICAL_BAR_EQUAL_SIGN,
	TOKEN_TAG_VERTICAL_BAR_2,
} Token_Tag;

static utf8 *representations_of_token_tags[] =
{
	[TOKEN_TAG_ETX]                            = "ETX",
	[TOKEN_TAG_EXCLAMATION_MARK]               = "`!`",
	[TOKEN_TAG_OCTOTHORPE]                     = "`#`",
	[TOKEN_TAG_DOLLAR_SIGN]                    = "`$`",
	[TOKEN_TAG_PERCENT_SIGN]                   = "`%`",
	[TOKEN_TAG_AMPERSAND]                      = "`&`",
	[TOKEN_TAG_APOSTROPHE]                     = "`'`",
	[TOKEN_TAG_LEFT_PARENTHESIS]               = "`(`",
	[TOKEN_TAG_RIGHT_PARENTHESIS]              = "`)`",
	[TOKEN_TAG_ASTERISK]                       = "`*`",
	[TOKEN_TAG_PLUS_SIGN]                      = "`+`",
	[TOKEN_TAG_COMMA]                          = "`,`",
	[TOKEN_TAG_HYPHEN_MINUS]                   = "`-`",
	[TOKEN_TAG_FULL_STOP]                      = "`.`",
	[TOKEN_TAG_SLASH]                          = "`/`",
	[TOKEN_TAG_COLON]                          = "`:`",
	[TOKEN_TAG_SEMICOLON]                      = "`;`",
	[TOKEN_TAG_LESS_THAN_SIGN]                 = "`<`",
	[TOKEN_TAG_EQUAL_SIGN]                     = "`=`",
	[TOKEN_TAG_GREATER_THAN_SIGN]              = "`>`",
	[TOKEN_TAG_QUESTION_MARK]                  = "`?`",
	[TOKEN_TAG_AT_SIGN]                        = "`@`",
	[TOKEN_TAG_LEFT_SQUARE_BRACKET]            = "`[`",
	[TOKEN_TAG_BACKSLASH]                      = "`\\`",
	[TOKEN_TAG_RIGHT_SQUARE_BRACKET]           = "`]`",
	[TOKEN_TAG_CIRCUMFLEX_ACCENT]              = "`^`",
	[TOKEN_TAG_GRAVE_ACCENT]                   = "```",
	[TOKEN_TAG_LEFT_CURLY_BRACKET]             = "`{`",
	[TOKEN_TAG_VERTICAL_BAR]                   = "`|`",
	[TOKEN_TAG_RIGHT_CURLY_BRACKET]            = "`}`",
	[TOKEN_TAG_TILDE]                          = "`~`",
	[TOKEN_TAG_NAME]                           = "name",
	[TOKEN_TAG_BINARY]                         = "binary",
	[TOKEN_TAG_DIGITAL]                        = "digital",
	[TOKEN_TAG_HEXADECIMAL]                    = "hexadecimal",
	[TOKEN_TAG_DECIMAL]                        = "decimal",
	[TOKEN_TAG_TEXT]                           = "text",
	[TOKEN_TAG_EXCLAMATION_MARK_EQUAL_SIGN]    = "`!=`",
	[TOKEN_TAG_PERCENT_SIGN_EQUAL_SIGN]        = "`%=`",
	[TOKEN_TAG_AMPERSAND_EQUAL_SIGN]           = "`&=`",
	[TOKEN_TAG_AMPERSAND_2]                    = "`&&`",
	[TOKEN_TAG_ASTERISK_EQUAL_SIGN]            = "`*=`",
	[TOKEN_TAG_PLUS_SIGN_EQUAL_SIGN]           = "`+=`",
	[TOKEN_TAG_HYPHEN_MINUS_EQUAL_SIGN]        = "`-=`",
	[TOKEN_TAG_FULL_STOP_2]                    = "`..`",
	[TOKEN_TAG_SLASH_EQUAL_SIGN]               = "`/=`",
	[TOKEN_TAG_LESS_THAN_SIGN_EQUAL_SIGN]      = "`<=`",
	[TOKEN_TAG_LESS_THAN_SIGN_2]               = "`<<`",
	[TOKEN_TAG_LESS_THAN_SIGN_2_EQUAL_SIGN]    = "`<<=`",
	[TOKEN_TAG_EQUAL_SIGN_2]                   = "`==`",
	[TOKEN_TAG_GREATER_THAN_SIGN_EQUAL_SIGN]   = "`>=`",
	[TOKEN_TAG_GREATER_THAN_SIGN_2]            = "`>>`",
	[TOKEN_TAG_GREATER_THAN_SIGN_2_EQUAL_SIGN] = "`>>=`",
	[TOKEN_TAG_CIRCUMFLEX_ACCENT_EQUAL_SIGN]   = "`^=`",
	[TOKEN_TAG_VERTICAL_BAR_EQUAL_SIGN]        = "`|=`",
	[TOKEN_TAG_VERTICAL_BAR_2]                 = "`||`"
};

typedef struct
{
	Token_Tag tag;
	Range range;
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
	SEVERITY_VERBOSE,
	SEVERITY_COMMENT,
	SEVERITY_CAUTION,
	SEVERITY_FAILURE,
} Severity;

static void report_v(Severity severity, const Source *source, const Range *range, const utf8 *message, vargs vargs)
{
	const utf8 *severities[] =
	{
		[SEVERITY_VERBOSE] = "verbose",
		[SEVERITY_COMMENT] = "comment",
		[SEVERITY_CAUTION] = "caution",
		[SEVERITY_FAILURE] = "failure",
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
	report_v(SEVERITY_FAILURE, source, range, message, vargs);
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
		token->tag = TOKEN_TAG_NAME;
		do get_character(caret);
		while(check_letter(caret->character) || check_digital(caret->character) || caret->character == '-');
		// TODO(Emhyr): disallow names ending wiht `-`
	}
	else if(check_digital(caret->character))
	{
		bit (*checker)(utf32) = &check_digital;
		token->tag = TOKEN_TAG_DIGITAL;
		if(caret->character == '0')
		{
			switch(get_character(caret))
			{
			case 'b':
				token->tag = TOKEN_TAG_BINARY;
				checker = &check_binary;
				get_character(caret);
				break;
			case 'x':
				token->tag = TOKEN_TAG_HEXADECIMAL;
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
				case TOKEN_TAG_DECIMAL:
				case TOKEN_TAG_BINARY:
				case TOKEN_TAG_HEXADECIMAL:
					failure_message = "weird ass number";
					goto failed;
				default:
					token->tag = TOKEN_TAG_DECIMAL;
					get_character(caret);
					break;
				}
			}
		}
	}
	else switch(caret->character)
	{
	case '\3':
		token->tag = TOKEN_TAG_ETX;
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
		token->tag = TOKEN_TAG_TEXT;
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
		utf32 second_character = peek_character(&peeked_increment, caret);
		if(second_character == '=')
		{
			switch(caret->character)
			{
			case '!': token->tag = TOKEN_TAG_EXCLAMATION_MARK_EQUAL_SIGN;  break;
			case '%': token->tag = TOKEN_TAG_PERCENT_SIGN_EQUAL_SIGN;      break;
			case '&': token->tag = TOKEN_TAG_AMPERSAND_EQUAL_SIGN;         break;
			case '*': token->tag = TOKEN_TAG_ASTERISK_EQUAL_SIGN;          break;
			case '+': token->tag = TOKEN_TAG_PLUS_SIGN_EQUAL_SIGN;         break;
			case '-': token->tag = TOKEN_TAG_HYPHEN_MINUS_EQUAL_SIGN;      break;
			case '.': token->tag = TOKEN_TAG_FULL_STOP_2;                  break;
			case '/': token->tag = TOKEN_TAG_SLASH_EQUAL_SIGN;             break;
			case '<': token->tag = TOKEN_TAG_LESS_THAN_SIGN_EQUAL_SIGN;    break;
			case '=': token->tag = TOKEN_TAG_EQUAL_SIGN_2;                 break;
			case '>': token->tag = TOKEN_TAG_GREATER_THAN_SIGN_EQUAL_SIGN; break;
			case '^': token->tag = TOKEN_TAG_CIRCUMFLEX_ACCENT_EQUAL_SIGN; break;
			case '|': token->tag = TOKEN_TAG_VERTICAL_BAR_EQUAL_SIGN;      break;
			}
			goto twice;
		}
		else if(second_character == caret->character)
		{
			switch(second_character)
			{
			case '&': token->tag = TOKEN_TAG_AMPERSAND_2;    break;
			case '|': token->tag = TOKEN_TAG_VERTICAL_BAR_2; break;
			case '<':
			case '>':
				get_character(caret);
				if(peek_character(&peeked_increment, caret) == '=')
				{
					switch(second_character)
					{
					case '<': token->tag = TOKEN_TAG_LESS_THAN_SIGN_2_EQUAL_SIGN;    break;
					case '>': token->tag = TOKEN_TAG_GREATER_THAN_SIGN_2_EQUAL_SIGN; break;
					}
				}
				else
				{
					switch(second_character)
					{
					case '<': token->tag = TOKEN_TAG_LESS_THAN_SIGN_2;    break;
					case '>': token->tag = TOKEN_TAG_GREATER_THAN_SIGN_2; break;
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
	NODE_TAG_INVOCATION,
	NODE_TAG_NEGATIVE,
	NODE_TAG_NEGATION,
	NODE_TAG_NOT,
	NODE_TAG_ADDRESS,
	NODE_TAG_INDIRECTION,
	NODE_TAG_JUMP,
	NODE_TAG_INFERENCE,
	
	NODE_TAG_LIST,
	NODE_TAG_RANGE,
	NODE_TAG_RESOLUTION,
	NODE_TAG_ADDITION,
	NODE_TAG_SUBTRACTION,
	NODE_TAG_MULTIPLICATION,
	NODE_TAG_DIVISION,
	NODE_TAG_REMAINDER,
	NODE_TAG_AND,
	NODE_TAG_OR,
	NODE_TAG_XOR,
	NODE_TAG_LSH,
	NODE_TAG_RSH,
	NODE_TAG_CONJUNCTION,
	NODE_TAG_DISJUNCTION,
	NODE_TAG_EQUALITY,
	NODE_TAG_INEQUALITY,
	NODE_TAG_MAJORITY,
	NODE_TAG_MINORITY,
	NODE_TAG_INCLUSIVE_MAJORITY,
	NODE_TAG_INCLUSIVE_MINORITY,
	
	NODE_TAG_ASSIGNMENT,
	NODE_TAG_ADDITION_ASSIGNMENT,
	NODE_TAG_SUBTRACTION_ASSIGNMENT,
	NODE_TAG_MULTIPLICATION_ASSIGNMENT,
	NODE_TAG_DIVISION_ASSIGNMENT,
	NODE_TAG_REMAINDER_ASSIGNMENT,
	NODE_TAG_AND_ASSIGNMENT,
	NODE_TAG_OR_ASSIGNMENT,
	NODE_TAG_XOR_ASSIGNMENT,
	NODE_TAG_LSH_ASSIGNMENT,
	NODE_TAG_RSH_ASSIGNMENT,
	
	NODE_TAG_CONDITION,

	NODE_TAG_VALUE,
	NODE_TAG_LABEL,
	NODE_TAG_ROUTINE,
	NODE_TAG_SCOPE,

	NODE_TAG_INTEGER,
	NODE_TAG_REAL,
	NODE_TAG_STRING,
	NODE_TAG_REFERENCE,

	NODE_TAG_SUBEXPRESSION,
	NODE_TAG_STRUCTURE,
} Node_Tag;

static const utf8 *representations_of_node_tags[] =
{
	[NODE_TAG_INVOCATION]                = "invocation",
	[NODE_TAG_NEGATIVE]                  = "negative",
	[NODE_TAG_NEGATION]                  = "negation",
	[NODE_TAG_NOT]                       = "NOT",
	[NODE_TAG_ADDRESS]                   = "address",
	[NODE_TAG_INDIRECTION]               = "indirection",
	[NODE_TAG_JUMP]                      = "jump",
	[NODE_TAG_INFERENCE]                 = "inference",
	[NODE_TAG_LIST]                      = "list",
	[NODE_TAG_RANGE]                     = "range",
	[NODE_TAG_RESOLUTION]                = "resolution",
	[NODE_TAG_ADDITION]                  = "addition",
	[NODE_TAG_SUBTRACTION]               = "subtraction",
	[NODE_TAG_MULTIPLICATION]            = "multiplication",
	[NODE_TAG_DIVISION]                  = "division",
	[NODE_TAG_REMAINDER]                 = "remainder",
	[NODE_TAG_AND]                       = "AND",
	[NODE_TAG_OR]                        = "OR",
	[NODE_TAG_XOR]                       = "XOR",
	[NODE_TAG_LSH]                       = "LSH",
	[NODE_TAG_RSH]                       = "RSH",
	[NODE_TAG_CONJUNCTION]               = "conjunction",
	[NODE_TAG_DISJUNCTION]               = "disjunction",
	[NODE_TAG_EQUALITY]                  = "equality",
	[NODE_TAG_INEQUALITY]                = "inequality",
	[NODE_TAG_MAJORITY]                  = "majority",
	[NODE_TAG_MINORITY]                  = "minority",
	[NODE_TAG_INCLUSIVE_MAJORITY]        = "inclusive majority",
	[NODE_TAG_INCLUSIVE_MINORITY]        = "inclusive minority",
	[NODE_TAG_ASSIGNMENT]                = "assignment",
	[NODE_TAG_ADDITION_ASSIGNMENT]       = "addition assignment",
	[NODE_TAG_SUBTRACTION_ASSIGNMENT]    = "subtraction assignment",
	[NODE_TAG_MULTIPLICATION_ASSIGNMENT] = "multiplication assignment",
	[NODE_TAG_DIVISION_ASSIGNMENT]       = "division assignment",
	[NODE_TAG_REMAINDER_ASSIGNMENT]      = "remainder assignment",
	[NODE_TAG_AND_ASSIGNMENT]            = "AND assignment",
	[NODE_TAG_OR_ASSIGNMENT]             = "OR assignment",
	[NODE_TAG_XOR_ASSIGNMENT]            = "XOR assignment",
	[NODE_TAG_LSH_ASSIGNMENT]            = "LSH assignment",
	[NODE_TAG_RSH_ASSIGNMENT]            = "RSH assignment",
	[NODE_TAG_CONDITION]                 = "condition",
	[NODE_TAG_VALUE]                     = "value",
	[NODE_TAG_LABEL]                     = "label",
	[NODE_TAG_ROUTINE]                   = "routine",
	[NODE_TAG_SCOPE]                     = "scope",
	[NODE_TAG_INTEGER]                   = "integer",
	[NODE_TAG_REAL]                      = "real",
	[NODE_TAG_STRING]                    = "string",
	[NODE_TAG_REFERENCE]                 = "reference",
	[NODE_TAG_SUBEXPRESSION]             = "subexpression",
	[NODE_TAG_STRUCTURE]                 = "structure",
};

typedef uint8 Precedence;

enum : uint8
{
	PRECEDENCE_TYPE       = 100,
	PRECEDENCE_ASSIGNMENT,
};

static Precedence precedences[] =
{
	[NODE_TAG_RESOLUTION]                = 14,
	
	[NODE_TAG_INVOCATION]                = 13,
	[NODE_TAG_NEGATIVE]                  = 13,
	[NODE_TAG_NEGATION]                  = 13,
	[NODE_TAG_NOT]                       = 13,
	[NODE_TAG_ADDRESS]                   = 13,
	[NODE_TAG_INDIRECTION]               = 13,
	[NODE_TAG_JUMP]                      = 13,
	[NODE_TAG_INFERENCE]                 = 13,
	[NODE_TAG_RANGE]                     = 13,
	
	[NODE_TAG_MULTIPLICATION]            = 12,
	[NODE_TAG_DIVISION]                  = 12,
	[NODE_TAG_REMAINDER]                 = 12,
	
	[NODE_TAG_ADDITION]                  = 11,
	[NODE_TAG_SUBTRACTION]               = 11,
	
	[NODE_TAG_LSH]                       = 10,
	[NODE_TAG_RSH]                       = 10,
	
	[NODE_TAG_MAJORITY]                  = 9,
	[NODE_TAG_MINORITY]                  = 9,
	[NODE_TAG_INCLUSIVE_MAJORITY]        = 9,
	[NODE_TAG_INCLUSIVE_MINORITY]        = 9,
	
	[NODE_TAG_EQUALITY]                  = 8,
	[NODE_TAG_INEQUALITY]                = 8,
	
	[NODE_TAG_AND]                       = 7,
	
	[NODE_TAG_XOR]                       = 6,
	
	[NODE_TAG_OR]                        = 5,
	
	[NODE_TAG_CONJUNCTION]               = 4,
	
	[NODE_TAG_DISJUNCTION]               = 3,
	
	[NODE_TAG_CONDITION]                 = 2,
	[NODE_TAG_ASSIGNMENT]                = 2,
	[NODE_TAG_ADDITION_ASSIGNMENT]       = 2,
	[NODE_TAG_SUBTRACTION_ASSIGNMENT]    = 2,
	[NODE_TAG_MULTIPLICATION_ASSIGNMENT] = 2,
	[NODE_TAG_DIVISION_ASSIGNMENT]       = 2,
	[NODE_TAG_REMAINDER_ASSIGNMENT]      = 2,
	[NODE_TAG_AND_ASSIGNMENT]            = 2,
	[NODE_TAG_OR_ASSIGNMENT]             = 2,
	[NODE_TAG_XOR_ASSIGNMENT]            = 2,
	[NODE_TAG_LSH_ASSIGNMENT]            = 2,
	[NODE_TAG_RSH_ASSIGNMENT]            = 2,
	
	[NODE_TAG_LIST]                      = 1,
};

typedef struct Node Node;

typedef struct
{
	const utf8 *value;
	uint32 size;
} Identifier;

typedef struct
{
	Identifier identifier;
	Node *type;
	Node *assignment;
	bit constant : 1;
} Value;

typedef struct
{
	Identifier identifier;
	uint32 position;
} Label;

typedef struct Routine Routine;

typedef struct Scope Scope;

struct Scope
{
	// NOTE(Emhyr): the following 2 are set by the caller of `parse_scope`
	Scope *parent;
	Routine *owner;

	Value *values;
	Label *labels;
	Routine *routines;
	Node **statements;
	uint32 values_count;
	uint32 labels_count;
	uint32 routines_count;
	uint32 statements_count;
};

struct Routine
{
	Identifier identifier;
	Value *parameters;
	uint32 parameters_count;
	uint32 arguments_count;
	Scope scope;
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
	Range range;
	union
	{
		Identifier identifier;
		Integer integer;
		Real real;
		String string;
		Node *unary;
		Node *binary[2];
		Node *ternary[3];
		Value *value;
		Scope scope;
	} data[];
};

void parse_integer(Integer *integer, Token *token, Caret *caret)
{
	ASSERT(token->tag == TOKEN_TAG_BINARY || token->tag == TOKEN_TAG_DIGITAL || token->tag == TOKEN_TAG_HEXADECIMAL);

	uint8 base;
	switch(token->tag)
	{
	case TOKEN_TAG_BINARY:
		base = 2;
		break;
	case TOKEN_TAG_DIGITAL:
		base = 10;
		break;
	case TOKEN_TAG_HEXADECIMAL:
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
	ASSERT(token->tag == TOKEN_TAG_DECIMAL);
	char *ending;
	real->value = strtod(caret->source->data + token->range.beginning, &ending);
	get_token(token, caret);
}

void parse_string(String *string, Token *token, Caret *caret)
{
	ASSERT(token->tag == TOKEN_TAG_TEXT);
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
	ASSERT(token->tag == TOKEN_TAG_NAME);
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
	case TOKEN_TAG_BINARY:
	case TOKEN_TAG_DIGITAL:
	case TOKEN_TAG_HEXADECIMAL:
		left = push_into_buffer(sizeof(Node) + sizeof(Integer), alignof(Node), buffer);
		left->tag = NODE_TAG_INTEGER;
		parse_integer(&left->data->integer, token, caret);
		break;
		// TODO(Emhyr): allow scientific and hex notation
	case TOKEN_TAG_DECIMAL:
		left = push_into_buffer(sizeof(Node) + sizeof(Real), alignof(Node), buffer);
		left->tag = NODE_TAG_REAL;
		parse_real(&left->data->real, token, caret);
		break;
	case TOKEN_TAG_TEXT:
		left = push_into_buffer(sizeof(Node) + sizeof(String), alignof(Node), buffer);
		left->tag = NODE_TAG_STRING;
		parse_string(&left->data->string, token, caret);
		break;
	case TOKEN_TAG_NAME:
		left = push_into_buffer(sizeof(Node) + sizeof(Identifier), alignof(Node), buffer);
		left->tag = NODE_TAG_REFERENCE;
		parse_identifier(&left->data->identifier, token, caret);
		break;

	case TOKEN_TAG_LEFT_SQUARE_BRACKET:
		// TODO(Emhyr): parse structure
		UNIMPLEMENTED();
		
	case TOKEN_TAG_LEFT_PARENTHESIS:
		left = push_into_buffer(sizeof(Node) + sizeof(Node *), alignof(Node), buffer);
		left->tag = token->tag == TOKEN_TAG_LEFT_PARENTHESIS ? NODE_TAG_SUBEXPRESSION : NODE_TAG_STRUCTURE;
		get_token(token, caret);
		left->data->unary = parse_expression(0, token, caret, buffer);
		if(token->tag == (left->tag == NODE_TAG_SUBEXPRESSION ? TOKEN_TAG_RIGHT_PARENTHESIS : TOKEN_TAG_RIGHT_SQUARE_BRACKET)) get_token(token, caret);
		else fail(caret->source, &token->range, "unterminated scope; expected `%c`", left->tag == NODE_TAG_SUBEXPRESSION ? ')' : ']');
		break;

		Node_Tag left_tag;
	case TOKEN_TAG_HYPHEN_MINUS:      left_tag = NODE_TAG_NEGATIVE;    goto unary;
	case TOKEN_TAG_EXCLAMATION_MARK:  left_tag = NODE_TAG_NEGATION;    goto unary;
	case TOKEN_TAG_TILDE:             left_tag = NODE_TAG_NOT;         goto unary;
	case TOKEN_TAG_AT_SIGN:           left_tag = NODE_TAG_ADDRESS;     goto unary;
	case TOKEN_TAG_BACKSLASH:         left_tag = NODE_TAG_INDIRECTION; goto unary;
	case TOKEN_TAG_CIRCUMFLEX_ACCENT: left_tag = NODE_TAG_JUMP;        goto unary;
	case TOKEN_TAG_APOSTROPHE:        left_tag = NODE_TAG_INFERENCE;   goto unary;
	unary:
		left = push_into_buffer(sizeof(Node) + sizeof(Node *), alignof(Node), buffer);
		left->tag = left_tag;
		get_token(token, caret);
		left->data->unary = parse_expression(precedences[left_tag], token, caret, buffer);
		break;

	case TOKEN_TAG_COLON:
	case TOKEN_TAG_SEMICOLON:
	case TOKEN_TAG_RIGHT_PARENTHESIS:
	case TOKEN_TAG_RIGHT_SQUARE_BRACKET:
	case TOKEN_TAG_RIGHT_CURLY_BRACKET:
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
		case TOKEN_TAG_COMMA:                           right_tag = NODE_TAG_LIST;                      goto binary;
		case TOKEN_TAG_FULL_STOP_2:                     right_tag = NODE_TAG_RANGE;                     goto binary;
		case TOKEN_TAG_FULL_STOP:                       right_tag = NODE_TAG_RESOLUTION;                goto binary;
		case TOKEN_TAG_PLUS_SIGN:                       right_tag = NODE_TAG_ADDITION;                  goto binary;
		case TOKEN_TAG_HYPHEN_MINUS:                    right_tag = NODE_TAG_SUBTRACTION;               goto binary;
		case TOKEN_TAG_ASTERISK:                        right_tag = NODE_TAG_MULTIPLICATION;            goto binary;
		case TOKEN_TAG_SLASH:                           right_tag = NODE_TAG_DIVISION;                  goto binary;
		case TOKEN_TAG_PERCENT_SIGN:                    right_tag = NODE_TAG_REMAINDER;                 goto binary;
		case TOKEN_TAG_AMPERSAND:                       right_tag = NODE_TAG_AND;                       goto binary;
		case TOKEN_TAG_VERTICAL_BAR:                    right_tag = NODE_TAG_OR;                        goto binary;
		case TOKEN_TAG_CIRCUMFLEX_ACCENT:               right_tag = NODE_TAG_XOR;                       goto binary;
		case TOKEN_TAG_LESS_THAN_SIGN_2:                right_tag = NODE_TAG_LSH;                       goto binary;
		case TOKEN_TAG_GREATER_THAN_SIGN_2:             right_tag = NODE_TAG_RSH;                       goto binary;
		case TOKEN_TAG_AMPERSAND_2:                     right_tag = NODE_TAG_CONJUNCTION;               goto binary;
		case TOKEN_TAG_VERTICAL_BAR_2:                  right_tag = NODE_TAG_DISJUNCTION;               goto binary;
		case TOKEN_TAG_EQUAL_SIGN_2:                    right_tag = NODE_TAG_EQUALITY;                  goto binary;
		case TOKEN_TAG_EXCLAMATION_MARK_EQUAL_SIGN:     right_tag = NODE_TAG_INEQUALITY;                goto binary;
		case TOKEN_TAG_GREATER_THAN_SIGN:               right_tag = NODE_TAG_MAJORITY;                  goto binary;
		case TOKEN_TAG_LESS_THAN_SIGN:                  right_tag = NODE_TAG_MINORITY;                  goto binary;
		case TOKEN_TAG_GREATER_THAN_SIGN_EQUAL_SIGN:    right_tag = NODE_TAG_INCLUSIVE_MAJORITY;        goto binary;
		case TOKEN_TAG_LESS_THAN_SIGN_EQUAL_SIGN:       right_tag = NODE_TAG_INCLUSIVE_MINORITY;        goto binary;
		case TOKEN_TAG_EQUAL_SIGN:                      right_tag = NODE_TAG_ASSIGNMENT;                goto binary;
		case TOKEN_TAG_PLUS_SIGN_EQUAL_SIGN:            right_tag = NODE_TAG_ADDITION_ASSIGNMENT;       goto binary;
		case TOKEN_TAG_HYPHEN_MINUS_EQUAL_SIGN:         right_tag = NODE_TAG_SUBTRACTION_ASSIGNMENT;    goto binary;
		case TOKEN_TAG_ASTERISK_EQUAL_SIGN:             right_tag = NODE_TAG_MULTIPLICATION_ASSIGNMENT; goto binary;
		case TOKEN_TAG_SLASH_EQUAL_SIGN:                right_tag = NODE_TAG_DIVISION_ASSIGNMENT;       goto binary;
		case TOKEN_TAG_PERCENT_SIGN_EQUAL_SIGN:         right_tag = NODE_TAG_REMAINDER_ASSIGNMENT;      goto binary;
		case TOKEN_TAG_AMPERSAND_EQUAL_SIGN:            right_tag = NODE_TAG_AND_ASSIGNMENT;            goto binary;
		case TOKEN_TAG_VERTICAL_BAR_EQUAL_SIGN:         right_tag = NODE_TAG_OR_ASSIGNMENT;             goto binary;
		case TOKEN_TAG_CIRCUMFLEX_ACCENT_EQUAL_SIGN:    right_tag = NODE_TAG_XOR_ASSIGNMENT;            goto binary;
		case TOKEN_TAG_LESS_THAN_SIGN_2_EQUAL_SIGN:     right_tag = NODE_TAG_LSH_ASSIGNMENT;            goto binary;
		case TOKEN_TAG_GREATER_THAN_SIGN_2_EQUAL_SIGN:  right_tag = NODE_TAG_RSH_ASSIGNMENT;            goto binary;
		default:                                        right_tag = NODE_TAG_INVOCATION;                goto binary;
		case TOKEN_TAG_QUESTION_MARK:                   right_tag = NODE_TAG_CONDITION;                 goto binary;
		binary:
			if((other_precedence == PRECEDENCE_TYPE || other_precedence == PRECEDENCE_ASSIGNMENT)&& (right_tag >= NODE_TAG_ASSIGNMENT && right_tag <= NODE_TAG_RSH_ASSIGNMENT || right_tag == NODE_TAG_LIST)) goto finished;
			Precedence right_precedence = precedences[right_tag];
			if(right_precedence <= other_precedence) goto finished;
			if(right_tag != NODE_TAG_INVOCATION) get_token(token, caret);
			right = push_into_buffer(sizeof(Node) + (right_tag == NODE_TAG_CONDITION ? sizeof(Node *[3]) : sizeof(Node *[2])), alignof(Node), buffer);
			right->tag = right_tag;
			right->data->binary[0] = left;
			right->data->binary[1] = parse_expression(right_tag == NODE_TAG_CONDITION ? 0 : right_precedence, token, caret, buffer);
			if(right_tag == NODE_TAG_CONDITION)
			{
				if(token->tag == TOKEN_TAG_EXCLAMATION_MARK)
				{
					get_token(token, caret);
					right->data->ternary[2] = parse_expression(right_precedence, token, caret, buffer);
				}
				else right->data->ternary[2] = 0;
			}
			break;
			
		case TOKEN_TAG_COLON:
		case TOKEN_TAG_SEMICOLON:
		case TOKEN_TAG_RIGHT_PARENTHESIS:
		case TOKEN_TAG_RIGHT_SQUARE_BRACKET:
		case TOKEN_TAG_RIGHT_CURLY_BRACKET:
		case TOKEN_TAG_EXCLAMATION_MARK:
			goto finished;

		case TOKEN_TAG_ETX:
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
	ASSERT(token->tag == TOKEN_TAG_NAME);
	parse_identifier(&value->identifier, token, caret);
	if(token->tag != TOKEN_TAG_COLON) fail(caret->source, &token->range, "expected `:`");
	get_token(token, caret);
	switch(token->tag)
	{
	case TOKEN_TAG_EQUAL_SIGN:
	case TOKEN_TAG_COLON:
		break;
	default:
		value->type = parse_expression(PRECEDENCE_TYPE, token, caret, buffer);
		break;
	}
	value->constant = 0;
	switch(token->tag)
	{
	case TOKEN_TAG_COLON:
		value->constant = 1;
	case TOKEN_TAG_EQUAL_SIGN:
		get_token(token, caret);
		value->assignment = parse_expression(PRECEDENCE_ASSIGNMENT, token, caret, buffer);
		break;
	default:
		if(!value->type) fail(caret->source, &token->range, "untyped and uninitialized value");
		break;
	}
}

void parse_scope(Scope *scope, Token *token, Caret *caret)
{
	ASSERT(token->tag == TOKEN_TAG_LEFT_CURLY_BRACKET);
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
		case TOKEN_TAG_NAME:
			if(get_token(token, caret) == TOKEN_TAG_COLON)
			{
				// TODO(Emhyr): parse modules
				*token = onsetting_token;
				*caret = onsetting_caret;
				Value *value = push_into_buffer(sizeof(Value), alignof(Value), values);
				++scope->values_count;
				parse_value(value, token, caret, buffer);
				node = push_into_buffer(sizeof(Node) + sizeof(Value *), alignof(Node), buffer);
				node->tag = NODE_TAG_VALUE;
				node->data->value = value;
				break;
			}
			else
			{
				*token = onsetting_token;
				*caret = onsetting_caret;
				goto expression;
			}
			break;
	
		case TOKEN_TAG_FULL_STOP:
			if(get_token(token, caret) == TOKEN_TAG_NAME)
			{
				Identifier identifier;
				parse_identifier(&identifier, token, caret);
				if(token->tag == TOKEN_TAG_COLON)
				{
					Routine *routine = push_into_buffer(sizeof(Routine), alignof(Routine), routines);
					++scope->routines_count;
					routine->identifier = identifier;
					Buffer *parameters = 0;
					routine->parameters_count = 0;
					if(get_token(token, caret) == TOKEN_TAG_LEFT_PARENTHESIS)
					{
						parameters = allocate_buffer(MEBIBYTE(1), system_page_size);
						get_token(token, caret);
						for(;;)
						{
							Value *value = 0;
							switch(token->tag)
							{
							case TOKEN_TAG_NAME:
								value = push_into_buffer(sizeof(Value), alignof(Value), parameters);
								parse_value(value, token, caret, buffer);
								++routine->parameters_count;
								break;
							case TOKEN_TAG_COMMA:
								get_token(token, caret);
								break;
							case TOKEN_TAG_RIGHT_PARENTHESIS:
								get_token(token, caret);
								goto finished_arguments;
							default:
								fail(caret->source, &token->range, "unexpected token; expected name, `,`, or `)`");
							}
						}
					finished_arguments:
					}
					routine->arguments_count = routine->parameters_count;
					if(token->tag == TOKEN_TAG_NAME)
					{
						if(!parameters) parameters = allocate_buffer(MEBIBYTE(1), system_page_size);
						for(;;)
						{
							Value *value = 0;
							switch(token->tag)
							{
							case TOKEN_TAG_NAME:
								value = push_into_buffer(sizeof(Value), alignof(Value), parameters);
								parse_value(value, token, caret, buffer);
								++routine->parameters_count;
								break;
							case TOKEN_TAG_COMMA:
								get_token(token, caret);
								break;
							case TOKEN_TAG_SEMICOLON:
								get_token(token, caret);
							case TOKEN_TAG_LEFT_CURLY_BRACKET:
								goto finished_results;
							default:
								fail(caret->source, &token->range, "unexpected token; expected name, `,`, `;`, or `{`");
							}
						}
					finished_results:
					}
					routine->parameters = (Value *)parameters->base;
					if(token->tag == TOKEN_TAG_LEFT_CURLY_BRACKET) parse_scope(&routine->scope, token, caret);
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

		case TOKEN_TAG_LEFT_CURLY_BRACKET:
			node = push_into_buffer(sizeof(Node) + sizeof(Scope), alignof(Node), buffer);
			node->tag = NODE_TAG_SCOPE;
			parse_scope(&node->data->scope, token, caret);
			break;

		default:
		expression:
			node = parse_expression(0, token, caret, buffer);
			break;
	
		case TOKEN_TAG_SEMICOLON:
			get_token(token, caret);
			continue;
		case TOKEN_TAG_RIGHT_CURLY_BRACKET:
			get_token(token, caret);
			goto finished;
		}

		Node **statement = push_into_buffer(sizeof(Node *), alignof(Node *), statements);
		*statement = node;
		++scope->statements_count;
	}

finished:
}

typedef struct Module Module;

struct Module
{
	Value *values;
	Label *labels;
	Routine *routines;
	uint32 values_count;
	uint32 labels_count;
	uint32 routines_count;
};

void dump(Node *);

void dump_value(Value *value)
{
	print("{\"identifier\":\"%.*s\"", value->identifier.size, value->identifier.value);
	print(",\"type\":"), dump(value->type);
	print(",\"assignment\":"), dump(value->assignment);
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
	print(",\"parameters\":[");
	if(routine->parameters_count)
	{
		for(uint32 i = 0; i < routine->parameters_count - 1; ++i)
		{
			Value *parameter = routine->parameters + i;
			dump_value(parameter);
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
			Value *Value = scope->values + i;
			dump_value(Value);
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
		case NODE_TAG_SUBEXPRESSION:
		case NODE_TAG_NEGATIVE:
		case NODE_TAG_NEGATION:
		case NODE_TAG_NOT:
		case NODE_TAG_ADDRESS:
		case NODE_TAG_INDIRECTION:
		case NODE_TAG_JUMP:
		case NODE_TAG_INFERENCE:
			dump(node->data->unary);
			break;
	
		case NODE_TAG_INVOCATION:
		case NODE_TAG_LIST:
		case NODE_TAG_RESOLUTION:
		case NODE_TAG_ADDITION:
		case NODE_TAG_SUBTRACTION:
		case NODE_TAG_MULTIPLICATION:
		case NODE_TAG_DIVISION:
		case NODE_TAG_REMAINDER:
		case NODE_TAG_AND:
		case NODE_TAG_OR:
		case NODE_TAG_XOR:
		case NODE_TAG_LSH:
		case NODE_TAG_RSH:
		case NODE_TAG_CONJUNCTION:
		case NODE_TAG_DISJUNCTION:
		case NODE_TAG_EQUALITY:
		case NODE_TAG_INEQUALITY:
		case NODE_TAG_MAJORITY:
		case NODE_TAG_MINORITY:
		case NODE_TAG_INCLUSIVE_MAJORITY:
		case NODE_TAG_INCLUSIVE_MINORITY:
		case NODE_TAG_ASSIGNMENT:
		case NODE_TAG_ADDITION_ASSIGNMENT:
		case NODE_TAG_SUBTRACTION_ASSIGNMENT:
		case NODE_TAG_MULTIPLICATION_ASSIGNMENT:
		case NODE_TAG_DIVISION_ASSIGNMENT:
		case NODE_TAG_REMAINDER_ASSIGNMENT:
		case NODE_TAG_AND_ASSIGNMENT:
		case NODE_TAG_OR_ASSIGNMENT:
		case NODE_TAG_XOR_ASSIGNMENT:
		case NODE_TAG_LSH_ASSIGNMENT:
		case NODE_TAG_RSH_ASSIGNMENT:
			print("[");
			dump(node->data->binary[0]);
			print(",");
			dump(node->data->binary[1]);
			print("]");
			break;
	
		case NODE_TAG_CONDITION:
			print("[");
			dump(node->data->ternary[0]);
			print(",");
			dump(node->data->ternary[1]);
			print(",");
			dump(node->data->ternary[2]);
			print("]");
			break;

		case NODE_TAG_VALUE:
			dump_value(node->data->value);
			break;
		case NODE_TAG_SCOPE:
			dump_scope(&node->data->scope);
			break;

		case NODE_TAG_INTEGER:
			print("%lu", node->data->integer.value);
			break;
		case NODE_TAG_REAL:
			print("%f", node->data->real.value);
			break;
		case NODE_TAG_STRING:
			print("\"%.*s\"", node->data->string.size, node->data->string.value);
			break;
		case NODE_TAG_REFERENCE:
			print("\"%.*s\"", node->data->identifier.size, node->data->identifier.value);
			break;

		case NODE_TAG_STRUCTURE:
			break;

		default:
			ASSERT(!"UNIMPLEMENTED");
			UNREACHABLE();
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
		parse_scope(&Scope, &token, &caret);
		dump_scope(&Scope);
#else
		buffer *buffer = allocate_buffer(GIBIBYTE(1), system_page_size);
		print("{\"statements\":[");
		while(token.tag != TOKEN_TAG_ETX)
		{
			node *node = parse_expression(0, &token, &caret, buffer);
			dump(node);
			print(",\n");
			//report(SEVERITY_VERBOSE, &source, &node->range, "%s", representations_of_node_tags[node->tag]);
			if(token.tag == TOKEN_TAG_SEMICOLON) get_token(&token, &caret);
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

