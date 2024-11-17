#include "compiler.h"

#define UNIMPLEMENTED() ({ ASSERT(!"unimplemented"); UNREACHABLE(); })

#define KIBIBYTE(x) ((ulong)(x) << 10)
#define MEBIBYTE(x) (KIBIBYTE(x) << 10)
#define GIBIBYTE(x) (MEBIBYTE(x) << 10)

#define UMAXOF(type) ((type)-1)
#define SMAXOF(type) ((type)((1ull << (sizeof(type) * 8 - 1)) - 1))

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

static inline ulong align_forwards(ulong address, ulong alignment)
{
	return address + get_forward_alignment(address, alignment);
}

struct buffer_data
{
	ubyte *ending;
	ubyte  beginning[];
};

struct buffer
{
	ulong reservation_size;
	ulong commission_rate;
	struct buffer_data *data;
	ulong commission_size;
};

void *push(uword size, uword alignment, struct buffer *buffer);

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

#define NORETURN _Noreturn void

static inline NORETURN fail(const struct source *source, const struct range *range, const utf8 *message, ...)
{
	vargs vargs;
	GET_VARGS(vargs, message);
	report_v(FAILURE, source, range, message, vargs);
	END_VARGS(vargs);
	terminate(-1);
	UNREACHABLE();
}

// TOKENIZATION ////////////////////////////////////////////////////////////////

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
	//FULL_STOP_2,
	ASTERISK_2,
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
	ARROW,
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
	//[FULL_STOP_2]                    = "`..`",
	[ASTERISK_2]                     = "`**`",
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
	[VERTICAL_BAR_2]                 = "`||`",
	[ARROW]                          = "`->`"
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
			//case '.': token->tag = FULL_STOP_2;    break;
			case '*': token->tag = ASTERISK_2;     break;
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
		else if(caret->character == '-' && second_character == '>')
		{
			token->tag = ARROW;
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

// PARSING /////////////////////////////////////////////////////////////////////

enum node_tag : ubyte
{
	INVOCATION,  // expression expression
	LAMBDA,      // expression `->` expression ? this must be of an address

	NEGATIVE,    // `-` expression
	NEGATION,    // `!` expression
	NOT,         // `~` expression
	ADDRESS,     // `@` expression
	INDIRECTION, // `\` expression
	JUMP,        // `^` expression
	INFERENCE,   // `'` expression
	DESIGNATION, // `.` expression ? this must be within a scoped expression
	
	LIST,               // expression `,` expression
	RANGE,              // expression `..` expression
	RESOLUTION,         // expression `.` expression
	ADDITION,           // expression `+` expression
	SUBTRACTION,        // expression `-` expression
	MULTIPLICATION,     // expression `*` expression
	DIVISION,           // expression `/` expression
	REMAINDER,          // expression `%` expression
	AND,                // expression `&` expression
	OR,                 // expression `|` expression
	XOR,                // expression `^` expression
	LSH,                // expression `<<` expression
	RSH,                // expression `>>` expression
	CONJUNCTION,        // expression `&&` expression
	DISJUNCTION,        // expression `||` expression
	EQUALITY,           // expression `==` expression
	INEQUALITY,         // expression `!=` expression
	MAJORITY,           // expression `>` expression
	MINORITY,           // expression `<` expression
	INCLUSIVE_MAJORITY, // expression `>=` expression
	INCLUSIVE_MINORITY, // expression `<=` expression
	
	ASSIGNMENT,                // expression `=` expression
	ADDITION_ASSIGNMENT,       // expression `+=` expression
	SUBTRACTION_ASSIGNMENT,    // expression `-=` expression
	MULTIPLICATION_ASSIGNMENT, // expression `*=` expression
	DIVISION_ASSIGNMENT,       // expression `/=` expression
	REMAINDER_ASSIGNMENT,      // expression `%=` expression
	AND_ASSIGNMENT,            // expression `&=` expression
	OR_ASSIGNMENT,             // expression `|=` expression
	XOR_ASSIGNMENT,            // expression `^=` expression
	LSH_ASSIGNMENT,            // expression `<<=` expression
	RSH_ASSIGNMENT,            // expression `>>=` expression

	FIELD, // expression `:` expression  ? this must be within a scope expression
	
	CONDITION, // expression `?` expression `!` expression

	SUBEXPRESSION, // `(` expression `)`
	ENUMERATION,   // `[` expression `]`

	VALUE,   // identifier `:` ([expression] `=` expression | expression [`=` expression])
	LABEL,   // `.` identifier
	ROUTINE, // `.` identifier `:` expression [scope]
	SCOPE,   // `{` {statement | `;`} `}`

	INTEGER,
	REAL32,
	REAL64,
	STRING,
	REFERENCE,
};

static const utf8 *representations_of_node_tags[] =
{
	[INVOCATION]                = "invocation",
	[LAMBDA]                    = "lambda",
	[NEGATIVE]                  = "negative",
	[NEGATION]                  = "negation",
	[NOT]                       = "NOT",
	[ADDRESS]                   = "address",
	[INDIRECTION]               = "indirection",
	[JUMP]                      = "jump",
	[INFERENCE]                 = "inference",
	[DESIGNATION]               = "designation",
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
	[FIELD]                     = "field",
	[VALUE]                     = "value",
	[LABEL]                     = "label",
	[ROUTINE]                   = "routine",
	[SCOPE]                     = "scope",
	[INTEGER]                   = "integer",
	[REAL32]                    = "real32",
	[REAL64]                    = "real64",
	[STRING]                    = "string",
	[REFERENCE]                 = "reference",
	[SUBEXPRESSION]             = "subexpression",
	[ENUMERATION]               = "enumeration",
};

typedef ubyte precedence_t;

enum : precedence_t
{
	DEFAULT_PRECEDENCE     = 0,
	DECLARATION_PRECEDENCE = 100,
};

static precedence_t precedences[] =
{
	[RESOLUTION]                = 16,

	[INVOCATION]                = 15,
	[LAMBDA]                    = 15,
	
	[NEGATIVE]                  = 14,
	[NEGATION]                  = 14,
	[NOT]                       = 14,
	[ADDRESS]                   = 14,
	[INDIRECTION]               = 14,
	[JUMP]                      = 14,
	[INFERENCE]                 = 14,
	[DESIGNATION]               = 14,
	
	[MULTIPLICATION]            = 13,
	[DIVISION]                  = 13,
	[REMAINDER]                 = 13,
	
	[ADDITION]                  = 12,
	[SUBTRACTION]               = 12,
	
	[LSH]                       = 11,
	[RSH]                       = 11,
	
	[MAJORITY]                  = 10,
	[MINORITY]                  = 10,
	[INCLUSIVE_MAJORITY]        = 10,
	[INCLUSIVE_MINORITY]        = 10,
	
	[EQUALITY]                  = 9,
	[INEQUALITY]                = 9,
	
	[AND]                       = 8,
	
	[XOR]                       = 7,
	
	[OR]                        = 6,
	
	[CONJUNCTION]               = 5,
	
	[DISJUNCTION]               = 4,
	
	[RANGE]                     = 3,
	[CONDITION]                 = 3,
	[ASSIGNMENT]                = 3,
	[ADDITION_ASSIGNMENT]       = 3,
	[SUBTRACTION_ASSIGNMENT]    = 3,
	[MULTIPLICATION_ASSIGNMENT] = 3,
	[DIVISION_ASSIGNMENT]       = 3,
	[REMAINDER_ASSIGNMENT]      = 3,
	[AND_ASSIGNMENT]            = 3,
	[OR_ASSIGNMENT]             = 3,
	[XOR_ASSIGNMENT]            = 3,
	[LSH_ASSIGNMENT]            = 3,
	[RSH_ASSIGNMENT]            = 3,

	[FIELD]                     = 2,
	
	[LIST]                      = 1,
};

struct identifier
{
	const utf8 *value;
	ulong       size;
};

struct value
{
	struct range      range;
	struct identifier identifier;
	struct node      *type;
	struct node      *initialization;
	bit               is_constant : 1;
};

struct label
{
	struct identifier identifier;
	ulong             position;
};

struct scope
{
	struct scope   *parent;
	struct routine *owner;

	struct range range;

	struct node **statements;
	ulong         statements_count;

	struct value   *values;
	struct label   *labels;
	struct routine *routines;
	ulong           values_count;
	ulong           labels_count;
	ulong           routines_count;
};

struct routine
{
	struct identifier identifier;
	struct node      *parameters;
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
	ulong  size;
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

		struct value *value;
		struct scope  scope;
	} data[];
};

void parse_integer(struct integer *result, struct token *token, struct caret *caret)
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

	result->value = 0;
	for(const utf8 *pointer = caret->source->data + token->range.beginning + (token->tag != DIGITAL ? 2 : 0),
	               *ending  = caret->source->data + token->range.ending;
	    pointer < ending;
	    pointer += 1)
	{
		result->value = result->value * base + *pointer - (*pointer >= '0' && *pointer <= '9' ? '0' : *pointer >= 'A' && *pointer <= 'F' ? 'A' : 'a');
		result->value += !(*pointer >= '0' && *pointer <= '9') ? 10 : 0;
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
	ulong row       = token->range.row;
	ulong column    = token->range.column;

	struct node *left = 0;
	switch(token->tag)
	{
	case BINARY:
	case DIGITAL:
	case HEXADECIMAL:
		left = push(sizeof(struct node) + sizeof(struct integer), alignof(struct node), buffer);
		left->tag = INTEGER;
		parse_integer(&left->data->integer, token, caret);
		break;

		// TODO(Emhyr): scientific notation for real64, decimal for real32
	case DECIMAL:
		left = push(sizeof(struct node) + sizeof(struct real), alignof(struct node), buffer);
		left->tag = REAL64;
		parse_real(&left->data->real, token, caret);
		break;
	case TEXT:
		left = push(sizeof(struct node) + sizeof(struct string), alignof(struct node), buffer);
		left->tag = STRING;
		parse_string(&left->data->string, token, caret);
		break;
	case NAME:
		left = push(sizeof(struct node) + sizeof(struct identifier), alignof(struct node), buffer);
		left->tag = REFERENCE;
		parse_identifier(&left->data->identifier, token, caret);
		break;
	case LEFT_PARENTHESIS:
	case LEFT_SQUARE_BRACKET:
		left = push(sizeof(struct node) + sizeof(struct unary), alignof(struct node), buffer);
		left->tag = token->tag == LEFT_PARENTHESIS ? SUBEXPRESSION : ENUMERATION;
		get_token(token, caret);
		left->data->unary.other = parse_expression(0, token, caret, buffer);
		if(token->tag == (left->tag == SUBEXPRESSION ? RIGHT_PARENTHESIS : RIGHT_SQUARE_BRACKET)) get_token(token, caret);
		else fail(caret->source, &token->range, "expected `%c`", left->tag == SUBEXPRESSION ? ')' : ']');
		break;

		enum node_tag left_tag;
	case HYPHEN_MINUS:      left_tag = NEGATIVE;    goto unary;
	case EXCLAMATION_MARK:  left_tag = NEGATION;    goto unary;
	case TILDE:             left_tag = NOT;         goto unary;
	case AT_SIGN:           left_tag = ADDRESS;     goto unary;
	case BACKSLASH:         left_tag = INDIRECTION; goto unary;
	case CIRCUMFLEX_ACCENT: left_tag = JUMP;        goto unary;
	case APOSTROPHE:        left_tag = INFERENCE;   goto unary;
	case FULL_STOP:         left_tag = DESIGNATION; goto unary;
	unary:
		left = push(sizeof(struct node) + sizeof(struct unary), alignof(struct node), buffer);
		left->tag = left_tag;
		get_token(token, caret);
		left->data->unary.other = parse_expression(precedences[left_tag], token, caret, buffer);
		break;

	case SEMICOLON:
	case RIGHT_PARENTHESIS:
	case RIGHT_SQUARE_BRACKET:
	case LEFT_CURLY_BRACKET:
	case RIGHT_CURLY_BRACKET:
		goto finished;

	default:
		fail(caret->source, &token->range, "unexpected token");
	}

	left->range.beginning = beginning;
	left->range.ending    = token->range.ending;
	left->range.row       = row;
	left->range.column    = column;

	for(;;)
	{
		struct node *right;
		switch(token->tag)
		{
			enum node_tag right_tag;
		case COMMA:                           right_tag = LIST;                      goto binary;
		//case FULL_STOP_2:                     right_tag = RANGE;                     goto binary;
		case ASTERISK_2:                      right_tag = RANGE;                     goto binary;
		case FULL_STOP:                       right_tag = RESOLUTION;                goto binary;
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
		case COLON:                           right_tag = FIELD;                     goto binary;
		case ARROW:                           right_tag = LAMBDA;                    goto binary;
		default:                              right_tag = INVOCATION;                goto binary;
		case QUESTION_MARK:                   right_tag = CONDITION;                 goto binary;
		binary:
			if(other_precedence == DECLARATION_PRECEDENCE)
			{
				if(right_tag >= ASSIGNMENT && right_tag <= RSH_ASSIGNMENT || right_tag == LIST || right_tag == FIELD) goto finished;
				else other_precedence = 0;
			}
			precedence_t right_precedence = precedences[right_tag];
			if(right_precedence <= other_precedence) goto finished;
			if(right_tag != INVOCATION) get_token(token, caret);
			bit is_ternary = right_tag == CONDITION;
			right = push(sizeof(struct node) + (is_ternary ? sizeof(struct ternary) : sizeof(struct binary)), alignof(struct node), buffer);
			right->tag = right_tag;
			right->data->binary.left = left;
			right->data->binary.right = parse_expression(is_ternary ? 0 : right_precedence, token, caret, buffer);
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
			
		case SEMICOLON:
		case RIGHT_PARENTHESIS:
		case RIGHT_SQUARE_BRACKET:
		case LEFT_CURLY_BRACKET:
		case RIGHT_CURLY_BRACKET:
		case EXCLAMATION_MARK:
			goto finished;

		case ETX:
			fail(caret->source, &token->range, "unfinished expression");
		}

		right->range.beginning = beginning;
		right->range.ending    = token->range.ending;
		right->range.row       = row;
		right->range.column    = column;

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
	if(token->tag == COLON)
	{
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
		value->is_constant = 0;
		switch(token->tag)
		{
		case COLON:
			value->is_constant = 1;
		case EQUAL_SIGN:
			get_token(token, caret);
			value->initialization = parse_expression(DECLARATION_PRECEDENCE, token, caret, buffer);
			break;
		default:
			if(value->type) break;
			else fail(caret->source, &token->range, "untyped and uninitialized value");
		}
		value->range.ending = token->range.ending;
	}
	else fail(caret->source, &token->range, "expected `:`");
}

void parse_scope(struct scope *scope, struct scope *parent, struct routine *owner, struct token *token, struct caret *caret)
{
	ASSERT(token->tag == LEFT_CURLY_BRACKET);

	scope->parent = parent;
	scope->owner = owner;
	
	scope->range.beginning = token->range.beginning;
	scope->range.row       = token->range.row;
	scope->range.column    = token->range.column;

	// TODO(Emhyr): ditch `allocate_buffer`
	struct buffer statements = {0, 0, 0};
	struct buffer buffer     = {0, 0, 0};
	struct buffer values     = {0, 0, 0};
	struct buffer labels     = {0, 0, 0};
	struct buffer routines   = {0, 0, 0};
	
	scope->statements_count = 0;
	scope->values_count     = 0;
	scope->labels_count     = 0;
	scope->routines_count   = 0;

	get_token(token, caret);
	for(;;)
	{
		struct node *node;
		// TODO(Emhyr): supersede `onsetting_*` - it's super gross
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
				struct value *value = push(sizeof(struct value), alignof(struct value), &values);
				scope->values_count += 1;
				parse_value(value, token, caret, &buffer);
				if(value->initialization && !value->is_constant)
				{
					node = push(sizeof(struct node) + sizeof(struct value *), alignof(struct node), &buffer);
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
					struct routine *routine = push(sizeof(struct routine), alignof(struct routine), &routines);
					scope->routines_count += 1;
					routine->identifier = identifier;
					get_token(token, caret);
					routine->parameters = parse_expression(DECLARATION_PRECEDENCE, token, caret, &buffer);
					if(token->tag == LEFT_CURLY_BRACKET) parse_scope(&routine->scope, scope, routine, token, caret);
				}
				else
				{
					struct label *label = push(sizeof(struct node) + sizeof(struct label), alignof(struct label), &labels);
					scope->labels_count += 1;
					label->identifier = identifier;
					label->position = scope->statements_count;
				}
			}
			else fail(caret->source, &token->range, "expected name");
			continue;

		case LEFT_CURLY_BRACKET:
			node = push(sizeof(struct node) + sizeof(struct scope), alignof(struct node), &buffer);
			node->tag = SCOPE;
			parse_scope(&node->data->scope, scope, 0, token, caret);
			break;

		default:
		expression:
			node = parse_expression(0, token, caret, &buffer);
			break;
	
		case SEMICOLON:
			get_token(token, caret);
			continue;

		case RIGHT_CURLY_BRACKET:
			get_token(token, caret);
			goto finished;
		}

	statement:
		struct node **statement = push(sizeof(struct node *), alignof(struct node *), &statements);
		*statement = node;
		scope->statements_count += 1;
	}

finished:
	scope->range.ending = token->range.ending;
	scope->statements = (struct node   **)statements.data->beginning;
	scope->values     = (struct value   *)values.data->beginning;
	scope->labels     = (struct label   *)labels.data->beginning;
	scope->routines   = (struct routine *)routines.data->beginning;
}

// TODO(Emhyr): parse modules
struct module
{
	struct module  *dependencies;
	struct value   *values;
	struct routine *routines;
	ulong           values_count;
	ulong           routines_count;
};

// DUMPING /////////////////////////////////////////////////////////////////////

void dump(struct node *);

void dump_value(struct value *value)
{
	print("{\"identifier\":\"%.*s\"", value->identifier.size, value->identifier.value);
	if(value->type) print(",\"type\":"), dump(value->type);
	if(value->initialization) print(",\"initialization\":"), dump(value->initialization);
	print(",\"is_constant\":%i}", value->is_constant);
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
	print(",\"arguments\":");
	dump(routine->parameters);
	print(",\"scope\":");
	dump_scope(&routine->scope);
	print("}");
}

void dump_scope(struct scope *scope)
{
	print("{");
	print("\"values\":[");
	if(scope->values_count)
	{
		for(ulong i = 0; i < scope->values_count - 1; ++i)
		{
			struct value *value = scope->values + i;
			dump_value(value);
			print(",");
		}
		dump_value(scope->values + scope->values_count - 1);
	}
	print("],\"labels\":[");
	if(scope->labels_count)
	{
		for(ulong i = 0; i < scope->labels_count - 1; ++i)
		{
			struct label *label = scope->labels + i;
			dump_label(label);
			print(",");
		}
		dump_label(scope->labels + scope->labels_count - 1);
	}
	print("],\"routines\":[");
	if(scope->routines_count)
	{
		for(ulong i = 0; i < scope->routines_count - 1; ++i)
		{
			struct routine *routine = scope->routines + i;
			dump_routine(routine);
			print(",");
		}
		dump_routine(scope->routines + scope->routines_count - 1);
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
		case ENUMERATION:
		case NEGATIVE:
		case NEGATION:
		case NOT:
		case ADDRESS:
		case INDIRECTION:
		case JUMP:
		case INFERENCE:
		case DESIGNATION:
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
		case FIELD:
		case LAMBDA:
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
			print("%llu", node->data->integer.value);
			break;
		case REAL32:
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

#if 0

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

static void *push(uword size, uword alignment, struct buffer *buffer)
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
#endif

static void *push(uword size, uword alignment, struct buffer *buffer)
{
	ASSERT(alignment % 2 == 0);
	if(!buffer->data)
	{
		if(!buffer->reservation_size) buffer->reservation_size = GIBIBYTE(1);
		if(!buffer->commission_rate)  buffer->commission_rate  = system_page_size;
		buffer->data = reserve_memory(buffer->reservation_size);
		commit_memory(buffer->data, buffer->commission_rate);
		buffer->commission_size = buffer->commission_rate;
		buffer->data->ending = buffer->data->beginning;
	}
	ulong forward_alignment = get_forward_alignment((ulong)buffer->data->beginning, alignment);
	ulong mass = buffer->data->ending - buffer->data->beginning;
	if(mass + forward_alignment + size > buffer->commission_size)
	{
		if(buffer->commission_size + buffer->commission_rate > buffer->reservation_size)
		{
			ulong new_reservation_size = align_forwards(buffer->reservation_size + buffer->reservation_size / 2, system_page_size);
			struct buffer_data *new_data = reserve_memory(new_reservation_size);
			commit_memory(new_data, buffer->commission_size);
			new_data->ending = new_data->beginning + mass;
			copy_memory(new_data->beginning, buffer->data->beginning, mass);
			buffer->reservation_size = new_reservation_size;
			buffer->data = new_data;
		}
		commit_memory((ubyte *)buffer->data + buffer->commission_size, buffer->commission_rate);
		buffer->commission_size += buffer->commission_rate;
	}
	buffer->data->ending += forward_alignment;
	void *result = buffer->data->ending;
	buffer->data->ending += size;
	zero_memory(result, size);
	return result;
}
