#include "compiler.h"

#define UNIMPLEMENTED() ({ ASSERT(!"unimplemented"); UNREACHABLE(); })

#define KIBIBYTE(x) ((ulong)(x) << 10)
#define MEBIBYTE(x) (KIBIBYTE(x) << 10)
#define GIBIBYTE(x) (MEBIBYTE(x) << 10)

#define UMAXOF(type) ((type)-1)

#define MINIMUM(a, b) ((a) <= (b) ? (a) : (b))

ulong format_v(utf8 *buffer, ulong size, const utf8 *format, vargs vargs);
ulong format(utf8 *buffer, ulong size, const utf8 *format, ...);
void print_v(const utf8 *format, vargs vargs);
void print(const utf8 *format, ...);

ubyte decode_utf8(utf32 *character, const utf8 bytes[4]);

void copy_memory(void *left, const void *right, ulong size);
void move_memory(void *left, const void *right, ulong size);
void fill_memory(void *buffer, ulong size, ubyte value);
void zero_memory(void *buffer, ulong size);

shalf compare_memory(const void *left, const void *right, ulong size);

ulong get_size_of_string(const utf8 *String);

constexpr uword universal_alignment = alignof(long double);

ulong get_backward_alignment(ulong address, ulong alignment);
ulong get_forward_alignment(ulong address, ulong alignment);

struct buffer
{
	ubyte *memory;
	uword  reservation_size;
	uword  commission_rate;
	uword  commission_size;
	uword  mass;

	alignas(universal_alignment) ubyte base[];
};

struct buffer *allocate_buffer  (uword reservation_size, uword commission_rate);
void           deallocate_buffer(struct buffer *buffer);
void          *push_into_buffer (uword size, uword alignment, struct buffer *buffer);

struct source
{
	const utf8 *path;
	utf8       *data;
	ulong       data_size;
	uhalf       path_size;
};

static void load_source(struct source *source, const utf8 *path)
{
	source->path = path;
	source->path_size = get_size_of_string(source->path);
	handle file = open_file(path);
	source->data_size = get_size_of_file(file);
	source->data = allocate_memory(source->data_size);
	read_from_file(source->data, source->data_size, file);
	close_file(file);
}

struct range
{
	ulong beginning;
	ulong ending;
	ulong row;
	ulong column;
};

struct location
{
	ulong position;
	ulong row;
	ulong column;
};

struct caret
{
	const struct source *source;
	struct location      location;
	utf32                character;
	ubyte                increment;
};

// NOTE(Emhyr): didn't feel like using X macros :/. fingers hurt
enum token_tag : ubyte
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
};

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

struct token
{
	enum token_tag tag;
	struct range   range;
};

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

static utf32 peek_character(ubyte *increment, const struct caret *caret)
{
	utf32 character;
	ulong position = caret->location.position + caret->increment;
	if(position < caret->source->data_size)
	{
		utf8 bytes[4] = {};
		for(ubyte i = 0; i < sizeof(bytes) / sizeof(bytes[0]); i += 1)
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

static utf32 get_character(struct caret *caret)
{
	ubyte increment;
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

enum severity
{
	VERBOSE,
	COMMENT,
	CAUTION,
	FAILURE,
};

static void report_v(enum severity severity, const struct source *source, const struct range *range, const utf8 *message, vargs vargs)
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
		print("%.*s[%lu-%lu|%lu,%lu]: %s: ", source->path_size, source->path, range->beginning, range->ending, range->row, range->column, severities[severity]);
		print_v(message, vargs);
		print("\n");

		ulong range_size = range->ending - range->beginning;
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

static inline void report(enum severity severity, const struct source *source, const struct range *range, const utf8 *message, ...)
{
	vargs vargs;
	GET_VARGS(vargs, message);
	report_v(severity, source, range, message, vargs);
	END_VARGS(vargs);
}

_Noreturn static inline void fail(const struct source *source, const struct range *range, const utf8 *message, ...)
{
	vargs vargs;
	GET_VARGS(vargs, message);
	report_v(FAILURE, source, range, message, vargs);
	END_VARGS(vargs);
	terminate(-1);
	UNREACHABLE();
}

static enum token_tag get_token(struct token *token, struct caret *caret)
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
			case '.':
				token->tag = DECIMAL;
				get_character(caret);
				break;
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
		ubyte peeked_increment;
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
			default:
				goto single;
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

enum node_tag : ubyte
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
	REAL64,
	STRING,
	REFERENCE,

	SUBEXPRESSION,
};

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
	[REAL64]                    = "real64",
	[STRING]                    = "string",
	[REFERENCE]                 = "reference",
	[SUBEXPRESSION]             = "subexpression",
};

typedef ubyte precedence_t;

enum : precedence_t
{
	DEFAULT_PRECEDENCE     = 0,
	DECLARATION_PRECEDENCE = 100,
};

static precedence_t precedences[] =
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

struct identifier
{
	const utf8 *value;
	ulong       size;
};

struct value
{
	struct identifier identifier;
	struct node      *type;
	struct node      *assignment;
	bit               constant : 1;
	struct range      range;
};

struct label
{
	struct identifier identifier;
	ulong             position;
};

struct symbol_table
{
	struct value   *values;
	struct label   *labels;
	struct routine *routines;
	ulong           values_count;
	ulong           labels_count;
	ulong           routines_count;
};

struct scope
{
	struct scope       *parent;
	struct routine     *owner;
	struct node       **statements;
	ulong               statements_count;
	struct symbol_table symbols;
};

struct routine
{
	struct identifier identifier;
	struct value     *parameters;
	ulong             parameters_count;
	ulong             arguments_count;
	struct scope      scope;
};

struct integer
{
	ulong value;
};

struct real
{
	real64 value;
};

struct string
{
	ubyte *value;
	ulong size;
};

struct unary
{
	struct node *other;
};

struct binary
{
	struct node *left, *right;
};

struct ternary
{
	struct node *left, *right, *other;
};

struct node
{
	enum node_tag tag;
	struct range  range;
	union
	{
		struct identifier identifier;
		struct integer    integer;
		struct real       real;
		struct string     string;
		struct unary      unary;
		struct binary     binary;
		struct ternary    ternary;
		struct value     *value;
		struct scope      scope;
	} data[];
};

void parse_integer(struct integer *integer, struct token *token, struct caret *caret)
{
	ASSERT(token->tag == BINARY || token->tag == DIGITAL || token->tag == HEXADECIMAL);

	ubyte base;
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
	for(const utf8 *pointer = caret->source->data + token->range.beginning + (token->tag != DIGITAL ? 2 : 0),
	               *ending  = caret->source->data + token->range.ending;
	    pointer < ending;
	    pointer += 1)
	{
		integer->value = integer->value * base + *pointer - (*pointer >= '0' && *pointer <= '9' ? '0' : *pointer >= 'A' && *pointer <= 'F' ? 'A' : 'a');
		integer->value += !(*pointer >= '0' && *pointer <= '9') ? 10 : 0;
	}
	get_token(token, caret);
}

#include <stdlib.h> // TODO(Emhyr): ditch <stdlib.h>

void parse_real(struct real *real, struct token *token, struct caret *caret)
{
	ASSERT(token->tag == DECIMAL);
	char *ending;
	real->value = strtod(caret->source->data + token->range.beginning, &ending);
	get_token(token, caret);
}

void parse_string(struct string *string, struct token *token, struct caret *caret)
{
	ASSERT(token->tag == TEXT);
	const ubyte *input  = (ubyte *)caret->source->data + token->range.beginning + 1;
	const ubyte *ending = (ubyte *)caret->source->data + token->range.ending;
	if(input == ending) fail(caret->source, &token->range, "empty string");
	ending -= 1;
	string->value = allocate_memory(ending - input);
	ubyte *output = string->value;
	while(input < ending)
	{
		ubyte byte = *input;
		input += 1;
		if(byte == '\\')
		{
			ubyte escape = *input;
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
				ubyte buffer = byte;
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

void parse_identifier(struct identifier *identifier, struct token *token, struct caret *caret)
{
	ASSERT(token->tag == NAME);
	identifier->value = caret->source->data + token->range.beginning;
	identifier->size = token->range.ending - token->range.beginning;
	get_token(token, caret);
}

struct node *parse_expression(precedence_t other_precedence, struct token *token, struct caret *caret, struct buffer *buffer)
{
	ulong beginning = token->range.beginning;
	ulong row = token->range.row;
	ulong column = token->range.column;

	struct node *left = 0;
	switch(token->tag)
	{
	case BINARY:
	case DIGITAL:
	case HEXADECIMAL:
		left = push_into_buffer(sizeof(struct node) + sizeof(struct integer), alignof(struct node), buffer);
		left->tag = INTEGER;
		parse_integer(&left->data->integer, token, caret);
		break;
		// TODO(Emhyr): allow scientific and hex notation
	case DECIMAL:
		left = push_into_buffer(sizeof(struct node) + sizeof(struct real), alignof(struct node), buffer);
		left->tag = REAL64;
		parse_real(&left->data->real, token, caret);
		break;
	case TEXT:
		left = push_into_buffer(sizeof(struct node) + sizeof(struct string), alignof(struct node), buffer);
		left->tag = STRING;
		parse_string(&left->data->string, token, caret);
		break;
	case NAME:
		left = push_into_buffer(sizeof(struct node) + sizeof(struct identifier), alignof(struct node), buffer);
		left->tag = REFERENCE;
		parse_identifier(&left->data->identifier, token, caret);
		break;
	case LEFT_PARENTHESIS:
		left = push_into_buffer(sizeof(struct node) + sizeof(struct unary), alignof(struct node), buffer);
		left->tag = SUBEXPRESSION;
		get_token(token, caret);
		left->data->unary.other = parse_expression(0, token, caret, buffer);
		if(token->tag == RIGHT_PARENTHESIS) get_token(token, caret);
		else fail(caret->source, &token->range, "unterminated scope; expected `%c`", left->tag == SUBEXPRESSION ? ')' : ']');
		break;

		enum node_tag left_tag;
	case HYPHEN_MINUS:      left_tag = NEGATIVE;    goto unary;
	case EXCLAMATION_MARK:  left_tag = NEGATION;    goto unary;
	case TILDE:             left_tag = NOT;         goto unary;
	case AT_SIGN:           left_tag = ADDRESS;     goto unary;
	case BACKSLASH:         left_tag = INDIRECTION; goto unary;
	case CIRCUMFLEX_ACCENT: left_tag = JUMP;        goto unary;
	case APOSTROPHE:        left_tag = INFERENCE;   goto unary;
	unary:
		left = push_into_buffer(sizeof(struct node) + sizeof(struct unary), alignof(struct node), buffer);
		left->tag = left_tag;
		get_token(token, caret);
		left->data->unary.other = parse_expression(precedences[left_tag], token, caret, buffer);
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
		struct node *right;
		switch(token->tag)
		{
			enum node_tag right_tag;
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
			if(other_precedence == DECLARATION_PRECEDENCE && (right_tag >= ASSIGNMENT && right_tag <= RSH_ASSIGNMENT || right_tag == LIST)) goto finished;
			precedence_t right_precedence = precedences[right_tag];
			if(right_precedence <= other_precedence) goto finished;
			if(right_tag != INVOCATION) get_token(token, caret);
			right = push_into_buffer(sizeof(struct node) + (right_tag == CONDITION ? sizeof(struct ternary) : sizeof(struct binary)), alignof(struct node), buffer);
			right->tag = right_tag;
			right->data->binary.left = left;
			right->data->binary.right = parse_expression(right_tag == CONDITION ? 0 : right_precedence, token, caret, buffer);
			if(right_tag == CONDITION)
			{
				if(token->tag == EXCLAMATION_MARK)
				{
					get_token(token, caret);
					right->data->ternary.other = parse_expression(right_precedence, token, caret, buffer);
				}
				else right->data->ternary.other = 0;
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

void parse_value(struct value *value, struct token *token, struct caret *caret, struct buffer *buffer)
{
	ASSERT(token->tag == NAME);
	value->range.beginning = token->range.beginning;
	value->range.row       = token->range.row;
	value->range.column    = token->range.column;
	parse_identifier(&value->identifier, token, caret);
	if(token->tag != COLON) fail(caret->source, &token->range, "expected `:`");
	get_token(token, caret);
	switch(token->tag)
	{
	case EQUAL_SIGN:
	case COLON:
		break;
	default:
		value->type = parse_expression(DECLARATION_PRECEDENCE, token, caret, buffer);
		break;
	}
	value->constant = 0;
	switch(token->tag)
	{
	case COLON:
		value->constant = 1;
	case EQUAL_SIGN:
		get_token(token, caret);
		value->assignment = parse_expression(DECLARATION_PRECEDENCE, token, caret, buffer);
		break;
	default:
		if(!value->type) fail(caret->source, &token->range, "untyped and uninitialized value");
		break;
	}
	value->range.ending = token->range.ending;
}

void parse_scope(struct scope *scope, struct scope *parent, struct routine *owner, struct token *token, struct caret *caret)
{
	ASSERT(token->tag == LEFT_CURLY_BRACKET);

	scope->parent = parent;
	scope->owner = owner;
	
	// TODO(Emhyr): ditch `allocate_buffer`
	struct buffer *statements = allocate_buffer(GIBIBYTE(1), system_page_size);
	struct buffer *buffer     = allocate_buffer(GIBIBYTE(1), system_page_size);
	struct buffer *values     = allocate_buffer(GIBIBYTE(1), system_page_size);
	struct buffer *labels     = allocate_buffer(GIBIBYTE(1), system_page_size);
	struct buffer *routines   = allocate_buffer(GIBIBYTE(1), system_page_size);
	
	scope->statements               = (struct node   **)statements->base;
	scope->statements_count         = 0;
	scope->symbols.values           = (struct value   *)values->base;
	scope->symbols.labels           = (struct label   *)labels->base;
	scope->symbols.routines         = (struct routine *)routines->base;
	scope->symbols.values_count     = 0;
	scope->symbols.labels_count     = 0;
	scope->symbols.routines_count   = 0;

	get_token(token, caret);
	for(;;)
	{
		struct node *node;
		struct token onsetting_token = *token;
		struct caret onsetting_caret = *caret;
		switch(onsetting_token.tag)
		{
		case NAME:
			if(get_token(token, caret) == COLON)
			{
				// TODO(Emhyr): parse modules
				*token = onsetting_token;
				*caret = onsetting_caret;
				struct value *value = push_into_buffer(sizeof(struct value), alignof(struct value), values);
				++scope->symbols.values_count;
				parse_value(value, token, caret, buffer);
				if(value->assignment && !value->constant)
				{
					node = push_into_buffer(sizeof(struct node) + sizeof(struct value *), alignof(struct node), buffer);
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
				struct identifier identifier;
				parse_identifier(&identifier, token, caret);
				if(token->tag == COLON)
				{
					struct routine *routine = push_into_buffer(sizeof(struct routine), alignof(struct routine), routines);
					++scope->symbols.routines_count;
					routine->identifier = identifier;
					get_token(token, caret);
					struct value  *parameter;
					struct buffer *parameters = 0;
					if(token->tag == LEFT_PARENTHESIS)
					{
						parameters = allocate_buffer(MEBIBYTE(1), system_page_size);
						get_token(token, caret);
						for(;;)
						{
							switch(token->tag)
							{
							case NAME:
								parameter = push_into_buffer(sizeof(struct value), alignof(struct value), parameters);
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
								parameter = push_into_buffer(sizeof(struct value), alignof(struct value), parameters);
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
					routine->parameters = (struct value *)parameters->base;
					if(token->tag == LEFT_CURLY_BRACKET) parse_scope(&routine->scope, scope, routine, token, caret);
				}
				else
				{
					struct label *label = push_into_buffer(sizeof(struct node) + sizeof(struct label), alignof(struct label), labels);
					++scope->symbols.labels_count;
					label->identifier = identifier;
					label->position = scope->statements_count;
				}
			}
			else fail(caret->source, &token->range, "unexpected token; expected name");
			continue;

		case LEFT_CURLY_BRACKET:
			node = push_into_buffer(sizeof(struct node) + sizeof(struct scope), alignof(struct node), buffer);
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
		struct node **statement = push_into_buffer(sizeof(struct node *), alignof(struct node *), statements);
		*statement = node;
		++scope->statements_count;
	}

finished:
}

enum type_class
{
	PRIMITIVE,
	POINTER,
	ARRAY,
	TUPLE,
	NAMED,
};

struct pointer
{
	struct type *subtype;
};

struct array
{
	struct type *subtype;
	ulong        count;
};

struct tuple
{
	struct type **subtypes;
	ulong         count;
};

struct named
{
	struct value *value;
};

struct type
{
	enum type_class class;
	union
	{
		struct address *pointer;
		struct array   *array;
		struct tuple   *tuple;
		struct named   *named;
	};
};

struct
{
	struct type ubyte;
	struct type uhalf;
	struct type uword;
	struct type ulong;
	struct type sbyte;
	struct type shalf;
	struct type sword;
	struct type slong;
	struct type real32;
	struct type real64;
} primitives =
{
	{PRIMITIVE},
	{PRIMITIVE},
	{PRIMITIVE},
	{PRIMITIVE},
	{PRIMITIVE},
	{PRIMITIVE},
	{PRIMITIVE},
	{PRIMITIVE},
	{PRIMITIVE},
	{PRIMITIVE}
};

struct type_table
{
	struct type_table *parent;
	struct type_table *children;
	ulong              children_count;

	struct pointer *pointers;
	struct array   *arrays;
	struct tuple   *tuples;
	struct named   *nameds;
	ulong           pointers_count;
	ulong           arrays_count;
	ulong           tuples_count;
	ulong           nameds_count;
};

struct type_buffers
{
	struct buffer *children;
	struct buffer *pointers;
	struct buffer *arrays;
	struct buffer *tuples;
	struct buffer *nameds;
};

struct type_buffers initialize_type_table(struct type_table *table, struct type_table *parent)
{
	zero_memory(table, sizeof(struct type_table));
	table->parent = parent;
	struct type_buffers buffers =
	{
		.children  = allocate_buffer(GIBIBYTE(1), system_page_size),
		.pointers  = allocate_buffer(GIBIBYTE(1), system_page_size),
		.arrays    = allocate_buffer(GIBIBYTE(1), system_page_size),
		.tuples    = allocate_buffer(GIBIBYTE(1), system_page_size),
		.nameds = allocate_buffer(GIBIBYTE(1), system_page_size)
	};
	table->children = (struct type_table *)buffers.children->base;
	table->pointers = (struct pointer    *)buffers.pointers->base;
	table->arrays   = (struct array      *)buffers.arrays->base;
	table->tuples   = (struct tuple      *)buffers.tuples->base;
	table->nameds   = (struct named      *)buffers.nameds->base;
	return buffers;
}

void *find(const void *query, ulong query_size, const void *items, ulong items_count)
{
	void *result = 0;
	for(ulong i = 0; i < items_count; ++i)
	{
		const void *current = items + i * query_size;
		if(!compare_memory(current, query, query_size))
		{
			result = (void *)current;
			break;
		}
	}
	return result;
}

void *find_type(enum type_class class, const void *query, struct type_table *types)
{
	const void *items;
	ulong       items_count;
	ulong       item_size;
	switch(class)
	{
	case POINTER:
		items = types->pointers;
		items_count = types->pointers_count;
		item_size = sizeof(struct pointer);
		break;
	case ARRAY:
		items = types->arrays;
		items_count = types->arrays_count;
		item_size = sizeof(struct array);
		break;
	case TUPLE:
		items = types->tuples;
		items_count = types->tuples_count;
		item_size = sizeof(struct tuple);
		break;
	case NAMED:
		items = types->nameds;
		items_count = types->nameds_count;
		item_size = sizeof(struct named);
		break;
	default:
		ASSERT(0);
	}
	void *result;
	if(types)
	{
		result = find(query, item_size, items, items_count);
		if(!result) result = find_type(class, query, types->parent);
	}
	else result = 0;
	return result;
}

struct type *check_node(struct node *node, struct type_table *types, struct type_buffers *buffers, struct symbol_table *symbols, struct source *source)
{
	struct type *result = 0;
	switch(node->tag)
	{
		struct type *left_type, *right_type, *other_type;
		
	case INTEGER:
		     if(node->data->integer.value <= UMAXOF(ubyte)) result = &primitives.ubyte;
		else if(node->data->integer.value <= UMAXOF(uhalf)) result = &primitives.uhalf;
		else if(node->data->integer.value <= UMAXOF(uword)) result = &primitives.uword;
		else                                                result = &primitives.ulong;
		break;
	case REAL64:
		result = &primitives.real64;
		break;
#if 0
	case STRING:
		{
			struct array array =
			{
				.subtype = &primitives.ubyte,
				.count   = node->data->string.size
			};
			result->array = find_type(ARRAY, &array, types);
			if(!result->array)
			{
				result->array  = push_into_buffer(sizeof(struct array), alignof(struct array), buffers->arrays);
				*result->array = array;
				
			}
		}
		result->class = ARRAY;
		break;
#endif

	//case REFERENCE:
	//	break;

	case VALUE:
		ASSERT(!"TODO(Emhyr): ignore this path since it's already checked when checking a scope's declarations");
		break;

	case SUBEXPRESSION:
		if(node->data->unary.other) result = check_node(node->data->unary.other, types, buffers, symbols, source);
		else result = 0;
		break;

	case NEGATIVE:
		other_type = check_node(node->data->unary.other, types, buffers, symbols, source);
		if(other_type >= &primitives.ubyte && other_type <= &primitives.real64)
		{
			     if(other_type == &primitives.ubyte) result = &primitives.sbyte;
			else if(other_type == &primitives.uhalf) result = &primitives.shalf;
			else if(other_type == &primitives.uword) result = &primitives.sword;
			else if(other_type == &primitives.ulong) result = &primitives.slong;
			else                                     result = other_type;
		}
		else fail(source, &node->range, "a negation only applies to primitives");
		break;

	case RSH:
	case LSH:
		right_type = check_node(node->data->binary.right, types, buffers, symbols, source);
		if(right_type >= &primitives.ubyte && right_type <= &primitives.ulong)
		{
			left_type = check_node(node->data->binary.left, types, buffers, symbols, source);
			if(left_type >= &primitives.ubyte && left_type <= &primitives.ulong) result = left_type;
			else fail(source, &node->range, "the type of the left part of a bitwise shift must be an unsigned integer");
		}
		else fail(source, &node->range, "the type of the right part of a bitwise shift must be an unsigned integer");
		break;

	default:
		UNIMPLEMENTED();
	}

	return result;
}

const utf8 *stringify_type(struct type *type)
{
	const utf8 *string;
	     if(type == &primitives.ubyte)  string = "ubyte"; 
	else if(type == &primitives.uhalf)  string = "uhalf";
	else if(type == &primitives.uword)  string = "uword";
	else if(type == &primitives.ulong)  string = "ulong";
	else if(type == &primitives.sbyte)  string = "sbyte"; 
	else if(type == &primitives.shalf)  string = "shalf";
	else if(type == &primitives.sword)  string = "sword";
	else if(type == &primitives.slong)  string = "slong";
	else if(type == &primitives.real32) string = "real32";
	else if(type == &primitives.real64) string = "real64";
	else if(type == 0)                  string = "void";
	return string;
}

void check(struct symbol_table *symbols, struct symbol_table *parent_symbols, struct source *source)
{
	struct type_table   types;
	struct type_buffers buffers = initialize_type_table(&types, 0);

	for(ulong i = 0; i < symbols->values_count; ++i)
	{
		struct value *value = symbols->values + i;
		struct type  *type  = check_node(value->type, &types, &buffers, symbols, source);
		struct type  *assignment_type = 0;
		if(value->assignment)
		{
			assignment_type = check_node(value->assignment, &types, &buffers, symbols, source);
			if(type != assignment_type) fail(source, &value->range, "mismatched types: %s != %s", stringify_type(type), stringify_type(assignment_type));
		}
		print("%.*s: %s", value->identifier.size, value->identifier.value, stringify_type(type));
		if(assignment_type) print(" = %s", stringify_type(assignment_type));
		print("\n");
	}
}

typedef struct Module Module;

struct Module
{
	struct value   *values;
	struct label   *labels;
	struct routine *routines;
	ulong           values_count;
	ulong           labels_count;
	ulong           routines_count;
};

void dump(struct node *);

void dump_value(struct value *value)
{
	print("{\"identifier\":\"%.*s\"", value->identifier.size, value->identifier.value);
	if(value->type) print(",\"type\":"), dump(value->type);
	if(value->assignment) print(",\"assignment\":"), dump(value->assignment);
	print(",\"constant\":%i}", value->constant);
}

void dump_label(struct label *label)
{
	print("{\"identifier\":\"%.*s\"", label->identifier.size, label->identifier.value);
	print(",\"position\":%i}", label->position);
}

void dump_scope(struct scope *scope);

void dump_routine(struct routine *routine)
{
	print("{\"identifier\":\"%.*s\"", routine->identifier.size, routine->identifier.value);
	print(",\"arguments\":[");
	if(routine->parameters_count)
	{
		for(ulong i = 0; i < routine->parameters_count - 1; ++i)
		{
			struct value *value = routine->parameters + i;
			dump_value(value);
			print(",");
		}
		dump_value(routine->parameters + routine->parameters_count - 1);
	}
	print("],\"scope\":");
	dump_scope(&routine->scope);
	print("}");
}

void dump_scope(struct scope *scope)
{
	print("{");
	print("\"values\":[");
	if(scope->symbols.values_count)
	{
		for(ulong i = 0; i < scope->symbols.values_count - 1; ++i)
		{
			struct value *value = scope->symbols.values + i;
			dump_value(value);
			print(",");
		}
		dump_value(scope->symbols.values + scope->symbols.values_count - 1);
	}
	print("],\"labels\":[");
	if(scope->symbols.labels_count)
	{
		for(ulong i = 0; i < scope->symbols.labels_count - 1; ++i)
		{
			struct label *label = scope->symbols.labels + i;
			dump_label(label);
			print(",");
		}
		dump_label(scope->symbols.labels + scope->symbols.labels_count - 1);
	}
	print("],\"routines\":[");
	if(scope->symbols.routines_count)
	{
		for(ulong i = 0; i < scope->symbols.routines_count - 1; ++i)
		{
			struct routine *routine = scope->symbols.routines + i;
			dump_routine(routine);
			print(",");
		}
		dump_routine(scope->symbols.routines + scope->symbols.routines_count - 1);
	}
	print("],\"statements\":[");
	if(scope->statements_count)
	{
		for(ulong i = 0; i < scope->statements_count - 1; ++i)
		{
			struct node *statement = scope->statements[i];
			dump(statement);
			print(",");
		}
		dump(scope->statements[scope->statements_count - 1]);
	}
	print("]}");
}

void dump(struct node *node)
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
			dump(node->data->unary.other);
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
			dump(node->data->binary.left);
			print(",");
			dump(node->data->binary.right);
			print("]");
			break;
	
		case CONDITION:
			print("[");
			dump(node->data->ternary.left);
			print(",");
			dump(node->data->ternary.right);
			print(",");
			dump(node->data->ternary.other);
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
		case REAL64:
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
		struct source source;
		load_source(&source, argv[1]);
		struct caret caret = {&source, {0, 0, 0}, '\n', 0};
		get_character(&caret);
		struct token token;
		get_token(&token, &caret);
#if 1
		struct scope scope;
		parse_scope(&scope, 0, 0, &token, &caret);
		dump_scope(&scope);
		print("\n");
		check(&scope.symbols, 0, &source);
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

static inline ulong format_v(utf8 *buffer, ulong size, const utf8 *format, vargs vargs)
{
	return stbsp_vsnprintf(buffer, size, format, vargs);
}

static inline ulong format(utf8 *buffer, ulong size, const utf8 *format, ...)
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
	ulong size = format_v(0, 0, format, vargs_copy) + 1;
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
typedef enum : uhalf
{
	DFA_UTF8_STATE_ACCEPT = 0,
	DFA_UTF8_STATE_REJECT = 1,
} Dfa_Utf8_State;

static inline uhalf dfa_decode_utf8(Dfa_Utf8_State *state, utf32 *character, utf8 byte)
{
	static const ubyte table[] =
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

	uhalf type = table[byte];
	*character = *state != DFA_UTF8_STATE_ACCEPT ? byte & 0x3fu | *character << 6 : 0xff >> type & byte;
	*state = table[256 + *state * 16 + type];
	return *state;
}

static ubyte decode_utf8(utf32 *character, const utf8 bytes[4])
{
	Dfa_Utf8_State state = DFA_UTF8_STATE_ACCEPT;
	uhalf i = 0;
	do if(dfa_decode_utf8(&state, character, bytes[i++]) == DFA_UTF8_STATE_ACCEPT) break;
	while(i < 4);
	return i;
}

static inline void copy_memory(void *left, const void *right, ulong size)
{
	__builtin_memcpy(left, right, size);
}

static inline void move_memory(void *left, const void *right, ulong size)
{
	__builtin_memmove(left, right, size);
}

static inline void fill_memory(void *buffer, ulong size, ubyte value)
{
	__builtin_memset(buffer, value, size);
}

static inline void zero_memory(void *buffer, ulong size)
{
	fill_memory(buffer, size, 0);
}

static inline shalf compare_memory(const void *left, const void *right, ulong size)
{
	return __builtin_memcmp(left, right, size);
}

static inline ulong get_size_of_string(const utf8 *String)
{
	return __builtin_strlen(String);
}

static inline ulong get_backward_alignment(ulong address, ulong alignment)
{
	ASSERT(alignment % 2 == 0);
	return alignment ? address & (alignment - 1) : 0;
}

static inline ulong get_forward_alignment(ulong address, ulong alignment)
{
	ulong remainder = get_backward_alignment(address, alignment);
	return remainder ? alignment - remainder : 0;
}

static struct buffer *allocate_buffer(uword reservation_size, uword commission_rate)
{
	reservation_size += get_forward_alignment(reservation_size, system_page_size);
	commission_rate += get_forward_alignment(commission_rate, system_page_size);
	struct buffer *buffer = reserve_memory(reservation_size);
	commit_memory(buffer, commission_rate);
	buffer->memory           = (ubyte *)buffer;
	buffer->reservation_size = reservation_size;
	buffer->commission_rate  = commission_rate;
	buffer->commission_size  = commission_rate;
	buffer->mass             = sizeof(struct buffer);
	return buffer;
}

static inline void deallocate_buffer(struct buffer *buffer)
{
	release_memory(buffer, buffer->reservation_size);
}

static void *push_into_buffer(uword size, uword alignment, struct buffer *buffer)
{
	ASSERT(alignment % 2 == 0);
	uword forward_alignment = get_forward_alignment((ulong)buffer + buffer->mass, alignment);
	if(buffer->mass + forward_alignment + size > buffer->commission_size)
	{
		if(buffer->commission_size + buffer->commission_rate > buffer->reservation_size)
		{
			print("buffer overflows: [%p]\n\treservation_size: %u\n\tcommission_rate: %u\n\tcommission_size: %u\n\tmass: %u\n",
				buffer, buffer->reservation_size, buffer->commission_rate, buffer->commission_size, buffer->mass);
			TRAP();
		}
		commit_memory((ubyte *)buffer + buffer->commission_size, buffer->commission_rate);
		buffer->commission_size += buffer->commission_rate;
	}
	buffer->mass += forward_alignment;
	void *result = buffer->memory + buffer->mass;
	zero_memory(result, size);
	buffer->mass += size;
	return result;
}


