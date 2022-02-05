


/*
	token format:

	2 byte: token length (0 - 65535)
	1 byte: token offset (0 - 255)
	1 byte: token type (enum token)
	n byte: token value        <- token pointer
	1 byte: null terminator
	2 byte: token length (0 - 65535, same as first byte)

*/
#define Token(name) Token_##name
#define Token_Header_Size 4
#define Token_Footer_Size 3
#define Calc_Token_Size(value_length) (Token_Header_Size + (value_length) + Token_Footer_Size)

/* 
	token page format

	token link: 2 pointers: 1. pointer to next page, 2. pointer to last token from prev page
	content tokens
	token link: 3 pointers: 1. pointer to next page, 2. pointer to this page. 3. size of this page

*/
#define Token_Page_Header_Size Calc_Token_Size (sizeof (void *) * 2)
#define Token_Page_Footer_Size Calc_Token_Size (sizeof (void *) * 4)

struct tokenizer {
	char	*start_data;
	char	*data;
	usize	size;
	usize	cap;
	char	*current;
};

enum token {
	Token (eof),
	Token (newline),
	Token (identifier),
	Token (preprocessing_number),
	Token (punctuator),
	Token (string),
	Token (character),
	Token (path_global),
	Token (path_relative),
	Token (link),
};

struct position {
	const char	*filename;
	int			line;
	int			column;
	int			at_the_beginning;
};

int		is_string_token (int token) {
	return (token >= Token (string) && token <= Token (path_relative));
}

const char	*get_token_name (int token) {
	switch (token) {
		case Token (eof): return "Token (eof)";
		case Token (newline): return "Token (newline)";
		case Token (identifier): return "Token (identifier)";
		case Token (preprocessing_number): return "Token (preprocessing_number)";
		case Token (punctuator): return "Token (punctuator)";
		case Token (string): return "Token (string)";
		case Token (character): return "Token (character)";
		case Token (path_global): return "Token (path_global)";
		case Token (path_relative): return "Token (path_relative)";
		case Token (link): return "Token (link)";
	}
	return "<invalid token>";
}

int		is_token (const char *token, int tkn, const char *string) {
	return (token[-1] == tkn && 0 == strcmp (token, string));
}

void	push_tokenizer_byte (struct tokenizer *tokenizer, int byte) {
	tokenizer->data[tokenizer->size] = byte;
	tokenizer->size += 1;
}

void	push_tokenizer_2bytes (struct tokenizer *tokenizer, int value) {
	push_tokenizer_byte (tokenizer, (value >> 8) & 0xFF );
	push_tokenizer_byte (tokenizer, value & 0xFF);
}

void	push_tokenizer_bytes (struct tokenizer *tokenizer, const void *bytes, usize length) {
	memcpy (tokenizer->data + tokenizer->size, bytes, length);
	tokenizer->size += length;
}

void	push_token_header (struct tokenizer *tokenizer, int length, int offset, int type) {
	push_tokenizer_2bytes (tokenizer, length);
	push_tokenizer_byte (tokenizer, offset);
	push_tokenizer_byte (tokenizer, type);
}

void	push_token_footer (struct tokenizer *tokenizer, int length) {
	push_tokenizer_byte (tokenizer, 0);
	push_tokenizer_2bytes (tokenizer, length);
}

void	*push_token_bytes (struct tokenizer *tokenizer, int offset, int type, const void *ptr, usize length) {
	void	*token;

	push_token_header (tokenizer, length, offset, type);
	token = tokenizer->data + tokenizer->size;
	push_tokenizer_bytes (tokenizer, ptr, length);
	push_token_footer (tokenizer, length);
	return (token);
}

int		prepare_tokenizer (struct tokenizer *tokenizer, usize tofit) {
	int		success;

	/* todo: can fail when tofit > Memory_Page */
	if (tokenizer->size + tofit + Token_Page_Footer_Size > tokenizer->cap) {
		void	*memory;
		usize	old_cap = tokenizer->cap;

		Debug ("cap: %zu; tofit: %zu; tokenizer->size: %zu; footer size: %zu; available: %zd", tokenizer->cap, tofit, tokenizer->size, Token_Page_Footer_Size, tokenizer->cap - tokenizer->size);
		Assert (tokenizer->cap == 0 || tokenizer->size + Token_Page_Footer_Size <= tokenizer->cap);
		memory = expand_array (0, &tokenizer->cap);
		if (memory) {
			void	*last_token = 0, *pointers[2];

			if (!tokenizer->start_data) {
				tokenizer->start_data = memory;
			}
			if (tokenizer->data) {
				void	*pointers[4] = { memory, tokenizer->data, (void *) tokenizer->size, (void *) old_cap };

				*(void **) (tokenizer->data + Token_Header_Size) = memory;
				last_token = push_token_bytes (tokenizer, 0, Token (link), pointers, sizeof pointers);
			}
			pointers[0] = 0;
			pointers[1] = last_token;
			tokenizer->data = memory;
			tokenizer->size = 0;
			tokenizer->current = push_token_bytes (tokenizer, 0, Token (link), pointers, sizeof pointers);
			Assert (tokenizer->size == Token_Page_Header_Size);
			success = 1;
		} else {
			Error ("NO MEMORY!!!!!!!!!!!!!!!!");
			success = 0;
		}
	} else {
		success = 1;
	}
	return (success);
}

void	set_token_offset (char *token, int offset) {
	token[-2] = offset & 0xFF;
	// token[-3] = (offset >> 8) & 0xFF;
}

void	set_token_length (char *token, int length) {
	token[-3] = length & 0xFF;
	token[-4] = (length >> 8) & 0xFF;
	(token + length + 1)[0] = (length >> 8) & 0xFF;
	(token + length + 1)[1] = length & 0xFF;
}

int		get_token_offset (const char *token) {
	int		offset;

	offset = (unsigned char) token[-2];
	// offset += (unsigned char) token[-3] << 8;
	return (offset);
}

int		get_token_length (const char *token) {
	int		length, second_length;

	length = (unsigned char) token[-3];
	length += (unsigned char) token[-4] << 8;
	second_length = (unsigned char) (token + length + 1)[0] << 8;
	second_length += (unsigned char) (token + length + 1)[1];
	Assert (length == second_length);
	return (length);
}

int		push_token (struct tokenizer *tokenizer, int offset, int token, const char *data, usize length) {
	int		success;

	if ((success = prepare_tokenizer (tokenizer, Calc_Token_Size (length)))) {
		tokenizer->current = push_token_bytes (tokenizer, offset, token, data, length);
	}
	return (success);
}

int		push_newline_token (struct tokenizer *tokenizer, int offset) {
	int		success;

	if (tokenizer->current && tokenizer->current[-1] == Token (newline) && (unsigned char) tokenizer->current[0] < 0x7f) {
		*(unsigned char *) tokenizer->current += 1;
		success = 1;
	} else {
		if ((success = prepare_tokenizer (tokenizer, Calc_Token_Size (1)))) {
			int		value = 1;

			tokenizer->current = push_token_bytes (tokenizer, offset, Token (newline), &value, 1);
		}
	}
	return (success);
}

int		push_compiled_newline_token (struct tokenizer *tokenizer, int count, int offset) {
	int		success = 1;

	/* TODO: unwrap loop */
	while (count > 0 && success) {
		success = push_newline_token (tokenizer, offset);
		offset = 0;
		count -= 1;
	}
	return (success);
}

int		revert_token (struct tokenizer *tokenizer);

int		push_string_token (struct tokenizer *tokenizer, int offset, const char *string, usize length, int push_at_existing) {
	int		success;

	if (push_at_existing && tokenizer->current && tokenizer->current[-1] == Token (string)) {
		int		old_length;
		char	*memory;

		old_length = get_token_length (tokenizer->current);
		offset = get_token_offset (tokenizer->current);
		memory = malloc (old_length + length);
		memcpy (memory, tokenizer->current, old_length);
		revert_token (tokenizer);
		if ((success = prepare_tokenizer (tokenizer, Calc_Token_Size (length + old_length)))) {
			push_token_header (tokenizer, length + old_length, offset, Token (string));
			tokenizer->current = tokenizer->data + tokenizer->size;
			push_tokenizer_bytes (tokenizer, memory, old_length);
			push_tokenizer_bytes (tokenizer, string, length);
			push_token_footer (tokenizer, length + old_length);
			Assert (get_token_length (tokenizer->current) >= 0);
		}
		free (memory);
	} else if ((success = prepare_tokenizer (tokenizer, Calc_Token_Size (length)))) {
		tokenizer->current = push_token_bytes (tokenizer, offset, Token (string), string, length);
		Assert (get_token_length (tokenizer->current) >= 0);
	}
	return (success);
}

const char	*next_const_token (const char *tokens, struct position *pos) {
	const char	*old = tokens;

	if (pos) {
		if (tokens[-1] == Token (newline)) {
			pos->line += tokens[0];
			pos->column = 1;
			pos->at_the_beginning = 1;
		} else {
			pos->column += get_token_length (tokens);
			pos->at_the_beginning = 0;
		}
	}
	int length = get_token_length (tokens);
	tokens += length;
	Assert (*tokens == 0);
	tokens += Token_Footer_Size;
	tokens += Token_Header_Size;
	if (tokens[-1] == Token (link)) {
		void	*memory = *(void **) tokens;

		tokens = memory;
		Assert (tokens);
		tokens += Token_Page_Header_Size;
		tokens += Token_Header_Size;
	}
	if (pos && tokens[-1] != Token (newline)) {
		pos->column += get_token_offset (tokens);
	}
	return (tokens);
}

char	*next_token (char *tokens, struct position *pos) {
	return ((char *) next_const_token (tokens, pos));
}

static unsigned	escape_symbol (const char **psource) {
	const char	*source = *psource;
	unsigned	result = *source;

	if (*source == '\\') {
		source += 1;
		switch (*source) {
			case 'a': result = '\a'; break ;
			case 'b': result = '\b'; break ;
			case 'f': result = '\f'; break ;
			case 'n': result = '\n'; break ;
			case 'r': result = '\r'; break ;
			case 't': result = '\t'; break ;
			case 'v': result = '\v'; break ;
			case '\\': result = '\\'; break ;
			case '\'': result = '\''; break ;
			case '\"': result = '\"'; break ;
			case '\?': result = '\?'; break ;
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
				unsigned value = *source - '0';
				if (source[1] >= '0' && source[1] <= '7') {
					source += 1;
					value <<= 3;
					value += *source - '0';
					if (source[1] >= '0' && source[1] <= '7') {
						source += 1;
						value <<= 3;
						value += *source - '0';
					}
				}
				result = (char) value;
			} break ;
			case 'x': {
				unsigned value = 0;
				while (isxdigit (source[1])) {
					source += 1;
					value *= 16;
					if (*source >= '0' && *source <= '9') {
						value += '0' - *source;
					} else if (*source >= 'a' && *source <= 'f') {
						value += 10 + ('a' - *source);
					} else if (*source >= 'A' && *source <= 'F') {
						value += 10 + ('A' - *source);
					}
				}
				result = value;
			} break ;
			case 'u': case 'U': {
				Error ("Unicode escape sequences are not implemented");
			} break ;
		}
		*psource = source;
	} else {
		result = *source;
	}
	return (result);
}

static void	unescape_symbol (int ch, char **pout) {
	char		*out = *pout;

	switch (ch) {
		case '\a': *out++ = '\\'; *out++ = 'a'; break ;
		case '\b': *out++ = '\\'; *out++ = 'b'; break ;
		case '\f': *out++ = '\\'; *out++ = 'f'; break ;
		case '\n': *out++ = '\\'; *out++ = 'n'; break ;
		case '\r': *out++ = '\\'; *out++ = 'r'; break ;
		case '\t': *out++ = '\\'; *out++ = 't'; break ;
		case '\v': *out++ = '\\'; *out++ = 'v'; break ;
		case '\\': *out++ = '\\'; *out++ = '\\'; break ;
		case '\'': *out++ = '\\'; *out++ = '\''; break ;
		case '\"': *out++ = '\\'; *out++ = '\"'; break ;
		case '\?': *out++ = '\\'; *out++ = '\?'; break ;
		default: {
			if (isprint (ch)) {
				*out++ = ch;
			} else {
				*out++ = '\\';
				if (!ch) {
					*out++ = '0';
				} else {
					register int oct;

					oct = (ch >> 6) & 0x3;
					*out++ = '0' + oct;
					out -= !oct;
					oct = (ch >> 3) & 0x7;
					*out++ = '0' + oct;
					out -= !oct;
					*out++ = '0' + ((ch >> 0) & 0x7);
				}
			}
		}
	}
	*pout = out;
}

char	*get_first_token (struct tokenizer *tokenizer) {
	return (tokenizer->start_data ? tokenizer->start_data + Token_Page_Header_Size + Token_Header_Size : 0);
}

char	*get_next_from_tokenizer (struct tokenizer *tokenizer, char *token) {
	if (token) {
		if (tokenizer->current == token) {
			token = 0;
		} else {
			token = next_token (token, 0);
		}
	} else {
		token = get_first_token (tokenizer);
	}
	return (token);
}

int		end_tokenizer (struct tokenizer *tokenizer, int offset) {
	int		success;

	if ((success = prepare_tokenizer (tokenizer, Calc_Token_Size (0)))) {
		tokenizer->current = push_token_bytes (tokenizer, offset, Token (eof), 0, 0);
	}
	return (success);
}

struct token_state {
	int			line;
	int			column;
	int			offset;
	int			tabsize;
	int			check_include;
	int			its_include;
	int			its_implement;
	int			*nl_array;
	const char	*filename;
};

int		make_token (struct tokenizer *tokenizer, struct token_state *state, const char **pcontent) {
	int			success = 1;
	const char	*content = *pcontent;

	while (*content && success && (isspace (*content) || *content <= 0x20)) {
		usize	spaces;

		if (*content == '\t') {
			if ((state->column - 1) % state->tabsize > 0) {
				spaces = state->tabsize - ((state->column - 1) % state->tabsize);
			} else {
				spaces = state->tabsize;
			}
		} else {
			spaces = 1;
		}
		state->offset += spaces;
		if (*content == '\n') {
			success = push_newline_token (tokenizer, state->offset);
			while (*state->nl_array && *state->nl_array == state->line && success) {
				success = push_newline_token (tokenizer, 0);
				state->nl_array += 1;
				state->line += 1;
			}
			state->line += 1;
			state->column = 0;
			spaces = 1;
			state->offset = 0;
			state->check_include = 0;
		}
		state->column += spaces;
		content += 1;
	}
	if (!*content || !success) {
	} else if (isalpha (*content) || *content == '_') {
		const char	*start = content;

		do {
			content += 1;
		} while (isalnum (*content) || *content == '_');
		state->column += content - start;
		push_token (tokenizer, state->offset, Token (identifier), start, content - start);
		state->offset = 0;
		if (state->its_include && state->its_implement) {
			state->its_implement = 0;
		} else if (state->check_include) {
			state->its_include = 0;
			state->its_implement = 0;
			if (0 == strncmp (start, "include", content - start) || 0 == strncmp (start, "import", content - start)) {
				state->its_include = 1;
			} else if (0 == strncmp (start, "implement", content - start)) {
				state->its_include = 1;
				state->its_implement = 1;
			}
		}
		state->check_include = 0;
	} else if (isdigit (*content) || (*content == '.' && isdigit (content[1]))) {
		const char	*start = content;

		content += (*content == '.');
		do {
			content += 1 + ((*content == 'e' || *content == 'E' || *content == 'p' || *content == 'P') && (content[1] == '+' || content[1] == '-'));
		} while (isalnum (*content) || *content == '_' || *content == '.');
		push_token (tokenizer, state->offset, Token (preprocessing_number), start, content - start);
		state->offset = 0;
		state->column += content - start;
		state->check_include = 0;
		state->its_include = 0;
		state->its_implement = 0;
	} else if (*content == '"' || *content == '\'' || (state->its_include && *content == '<')) {
		char	buffer_memory[256], *buffer = buffer_memory;
		char	end_symbol = (*content == '<' ? '>' : *content);
		int		pushed = 0;
		const char	*start;

		content += 1;
		start = content;
		while (*content && *content != end_symbol && success) {
			if (buffer - buffer_memory >= (isize) sizeof buffer_memory) {
				if ((success = push_string_token (tokenizer, state->offset, buffer_memory, sizeof buffer_memory, pushed))) {
					pushed = 1;
					buffer = buffer_memory;
				} else {
					break ;
				}
			}
			int	is_forbidden_newline = (*content == '\n');
			if (*content == '\\' && !state->its_include) {
				is_forbidden_newline = is_forbidden_newline || (content[1] == '\n');
				*buffer++ = escape_symbol (&content);
			} else {
				*buffer++ = *content;
			}
			if (is_forbidden_newline) {
				/* TODO: test it */
				Debug ("end_symbol: %c", end_symbol);
				Error_Message_p (state->filename, state->line, state->column, "new line character in the string literal is forbidden");
				success = 0;
				break ;
			}
			content += 1;
		}
		if (success) {
			success = push_string_token (tokenizer, state->offset, buffer_memory, buffer - buffer_memory, pushed);
			if (end_symbol == '\'') {
				tokenizer->current[-1] = Token (character);
			} else if (end_symbol == '>') {
				tokenizer->current[-1] = Token (path_global);
			} else if (state->its_include) {
				tokenizer->current[-1] = Token (path_relative);
			}
			content += 1;
		}
		state->column += (content - start) + 1;
		state->offset = 0;
		state->check_include = 0;
		state->its_include = 0;
		state->its_implement = 0;
	} else {
		static const char	*const strings[] = {
			"++", "+=", "--", "-=", "->", "...", "!=", "*=", "&&", "&=", "/=", "%=", "<=", "<<=", "<<", ">=", ">>=", ">>", "^=", "|=", "||", "==", "##",
			"<%", "%>", "<:", ":>", "%:%:", "%:",
			0
		};
		const char	*const *string = strings;
		usize		length = 1;

		while (*string) {
			usize	string_length = strlen (*string);

			if (0 == strncmp (*string, content, string_length)) {
				length = string_length;
				break ;
			}
			string += 1;
		}
		state->check_include = 0;
		state->its_include = 0;
		state->its_implement = 0;
		if (length == 1 && *content == '#' && ((tokenizer->current && tokenizer->current[-1] == Token (newline)) || !tokenizer->current)) {
			state->check_include = 1;
		}
		if (length == 2 && (*content == '<' || *content == '%' || *content == ':')) {
			if (0 == strncmp (content, "<%", 2)) {
				push_token (tokenizer, state->offset, Token (punctuator), "{", 1);
			} else if (0 == strncmp (content, "%>", 2)) {
				push_token (tokenizer, state->offset, Token (punctuator), "}", 1);
			} else if (0 == strncmp (content, "<:", 2)) {
				push_token (tokenizer, state->offset, Token (punctuator), "[", 1);
			} else if (0 == strncmp (content, ":>", 2)) {
				push_token (tokenizer, state->offset, Token (punctuator), "]", 1);
			} else if (0 == strncmp (content, "%:", 2)) {
				push_token (tokenizer, state->offset, Token (punctuator), "#", 1);
			} else {
				push_token (tokenizer, state->offset, Token (punctuator), content, length);
			}
		} else if (length == 4 && 0 == strncmp (content, "%:%:", 4)) {
			push_token (tokenizer, state->offset, Token (punctuator), "##", 2);
		} else {
			push_token (tokenizer, state->offset, Token (punctuator), content, length);
		}
		state->column += length;
		state->offset = 0;
		content += length;
	}
	*pcontent = content;
	return (success);
}

int		revert_token (struct tokenizer *tokenizer) {
	int		success;

	/* TODO: fix inter-paged revert case */
	if (tokenizer->current) {
		const char	*token = tokenizer->current;
		int			length, drop_length;

		drop_length = get_token_length (token);
		token -= Token_Header_Size;
		token -= 2;
		length = token[0] << 8;
		length += token[1];
		token -= 1;
		Assert (*token == 0);
		token -= length;
		if (token[-1] == Token (link)) {
			const char	*last_token;

			Assert (*(void **) token == 0);
			last_token = *((void **) token + 1);
			if (last_token) {
				Assert (last_token[-1] == Token (link));
				release_array (*(void **) last_token);
				*(void **) last_token = 0;
				length = get_token_length (last_token);
				Assert (length == sizeof (void *) * 4);
				Debug ("Return to previous page");
				tokenizer->data = *((void **) last_token + 1);
				tokenizer->size = (usize) *((void **) last_token + 2);
				tokenizer->cap = (usize) *((void **) last_token + 3);
				token = last_token;
				token -= Token_Header_Size;
				token -= 2;
				length = token[0] << 8;
				length += token[1];
				token -= 1;
				Assert (*token == 0);
				token -= length;
				tokenizer->current = (char *) token;
				if (token[-1] == Token (link)) {
					tokenizer->current = (char *) last_token;
					success = revert_token (tokenizer);
				} else {
					success = 1;
				}
			} else {
				release_array (tokenizer->data);
				memset (tokenizer, 0, sizeof *tokenizer);
				success = 1;
			}
		} else {
			tokenizer->current = (char *) token;
			tokenizer->size -= Calc_Token_Size (drop_length);
			success = 1;
		}
	} else {
		success = 1;
	}
	return (success);
}

void	free_tokens (const char *tokens) {
	void	**memory = (void **) (tokens - Token_Page_Header_Size);

	while (*memory) {
		void	*next = *memory;

		memory = (void **) ((char *) memory - Token_Header_Size);
		free (memory);
		memory = next ? (void **) ((char *) next + Token_Header_Size) : 0;
	}
	memory = (void **) ((char *) memory - Token_Header_Size);
	free (memory);
}

void	free_tokenizer (struct tokenizer *tokenizer) {
	if (tokenizer->start_data) {
		free_tokens (get_first_token (tokenizer));
	}
	memset (tokenizer, 0, sizeof *tokenizer);
}

void	reset_tokenizer (struct tokenizer *tokenizer) {
	/* TODO: reset at the begin */
	if (tokenizer->data) {
		tokenizer->size = Token_Page_Header_Size;
	}
}

char	*tokenize_with (struct tokenizer *tokenizer, const char *content, int *nl_array, int ensure_nl_at_end, const char *filename);

char	*tokenize (const char *content, int *nl_array, int ensure_nl_at_end, const char *filename) {
	struct tokenizer	tokenizer = {0};

	return (tokenize_with (&tokenizer, content, nl_array, ensure_nl_at_end, filename));
}

char	*tokenize_to (struct tokenizer *tokenizer, const char *content, const char *filename) {
	int					success = 1;
	char				*begin = tokenizer->current;
	struct token_state	state = {
		.tabsize = 1,
		.line = 1,
		.column = 1,
		.nl_array = (int []) { 0 },
		.filename = filename,
	};

	while (success && *content) {
		success = make_token (tokenizer, &state, &content);
	}
	return (success ? get_next_from_tokenizer (tokenizer, begin) : 0);
}

char	*tokenize_with (struct tokenizer *tokenizer, const char *content, int *nl_array, int ensure_nl_at_end, const char *filename) {
	int					success = 1;
	int					was_allocated = !!tokenizer->data;
	char				*begin = tokenizer->current;
	struct token_state	state = {
		.tabsize = 1,
		.line = 1,
		.column = 1,
		.nl_array = nl_array,
		.filename = filename,
	};

	while (success && *content) {
		success = make_token (tokenizer, &state, &content);
	}
	if (success) {
		if (ensure_nl_at_end && tokenizer->current && tokenizer->current[-1] != Token (newline)) {
			success = push_newline_token (tokenizer, state.offset);
			state.offset = 0;
		}
		if (success) {
			success = end_tokenizer (tokenizer, state.offset);
		}
	}
	if (!success) {
		Error ("tokenization failure");
		if (!was_allocated) {
			free_tokenizer (tokenizer);
		}
	}
	return (success ? get_next_from_tokenizer (tokenizer, begin) : 0);
}

int		unescape_string (const char **ptoken, char *out, usize cap, usize *size) {
	usize	index = 0;
	const char	*token = *ptoken;
	char	*out_start = out;
	int		success = 1;
	int		length = get_token_length (token);

	while (success && length > 0 && (success = out - out_start < (isize) cap - 4)) {
		unescape_symbol ((unsigned char) *token, &out);
		token += 1;
		length -= 1;
	}
	if (success) {
		if (out - out_start < (isize) cap - 1) {
			*size = out - out_start;
			*out++ = 0;
		} else {
			success = 0;
		}
	}
	*ptoken = token;
	return (success);
}

int		get_open_string (int tkn) {
	int		result = 0;

	switch (tkn) {
		case Token (string): result = '\"'; break ;
		case Token (character): result = '\''; break ;
		case Token (path_global): result = '<'; break ;
		case Token (path_relative): result = '\"'; break ;
	}
	return (result);
}

int		get_close_string (int tkn) {
	int		result = 0;

	switch (tkn) {
		case Token (string): result = '\"'; break ;
		case Token (character): result = '\''; break ;
		case Token (path_global): result = '>'; break ;
		case Token (path_relative): result = '\"'; break ;
	}
	return (result);
}

int		unescape_string_token (const char *token, char *out, usize cap, usize *size) {
	int		result;
	int		tkn = token[-1];

	if (cap > 3) {
		*out++ = get_open_string (tkn);
		result = unescape_string (&token, out, cap - 3, size);
		if (result) {
			out[(*size)++] = get_close_string (tkn);
			out[*size] = 0;
		}
	} else {
		result = 0;
	}
	return (result);
}

int		concatenate_token (struct tokenizer *tokenizer, const char *token, const struct position *pos) {
	int		success;

	if (tokenizer->current && tokenizer->current[-1] && tokenizer->current[-1] != Token (newline) && token[-1] && token[-1] != Token (newline)) {
		char	*content;
		usize	cap = 0;
		int		offset;

		offset = get_token_offset (tokenizer->current);
		content = expand_array (0, &cap);
		if (content) {
			usize	size = 0;

			if (is_string_token (tokenizer->current[-1])) {
				success = unescape_string_token (tokenizer->current, content, cap, &size);
			} else {
				size = stpcpy (content, tokenizer->current) - content;
				if (size < cap) {
					if (is_string_token (token[-1])) {
						usize	new_size = 0;

						success = unescape_string_token (token, content + size, cap - size, &new_size);
						size += new_size;
					} else {
						size = stpcpy (content + size, token) - content;
						if (size < cap) {
							content[size] = 0;
							success = 1;
						} else {
							System_Error_Message (pos, "not enough memory");
							success = 0;
						}
					}
				} else {
					System_Error_Message (pos, "not enough memory");
					success = 0;
				}
			}
		} else {
			success = 0;
		}
		if (success) {
			struct token_state	state = {
				.line = 1,
				.column = 1,
				.nl_array = &(int) {0},
				.filename = pos->filename,
			};
			const char	*ptr = content;

			success = revert_token (tokenizer);
			if (success) {
				success = make_token (tokenizer, &state, (const char **) &ptr);
				if (success) {
					if ((success = (*ptr == 0))) {
						set_token_offset (tokenizer->current, offset);
					} else {
						Error_Message (pos, "resulting token is invalid");
						success = 0;
					}
				}
			}
		}
		free (content);
	} else {
		Error_Message (pos, "invalid tokens");
		success = 0;
	}
	return (success);
}

void	print_string_token (const char *token, FILE *file) {
	char	buffer[256 + 1];
	usize	bufsize = 0;
	int		tkn = token[-1];

	fprintf (file, "%c", get_open_string (tkn));
	while (!unescape_string (&token, buffer, Array_Count (buffer) - 1, &bufsize)) {
		buffer[bufsize] = 0;
		fprintf (file, "%s", buffer);
	}
	buffer[bufsize] = 0;
	fprintf (file, "%s%c", buffer, get_close_string (tkn));
}

void	print_tokens_until (const char *tokens, int with_lines, const char *line_prefix, int end_token, FILE *file) {
	struct position	cpos = {
		.filename = "",
		.line = 1,
		.column = 1,
	}, *pos = &cpos;
	const char	*prev = 0;

	fprintf (file, "%s", line_prefix);
	if (with_lines) {
		fprintf (file, "%*d|", 4, pos->line);
	}
	if (!tokens[-1] || tokens[-1] == end_token) {
		fprintf (file, "%*.s\n", get_token_offset (tokens), "");
	} else while (tokens[-1] && tokens[-1] != end_token) {
		if (tokens[-1] == Token (newline)) {
			size_t	index = 0, initial_line = pos->line;

			if (tokens[0] > 16) {
				const char	*next;
				int		old_line = pos->line;

				pos->line += tokens[0];
				next = next_const_token (tokens, 0);
				while (next[-1] == Token (newline)) {
					pos->line += next[0];
					tokens = next;
					next = next_const_token (next, 0);
				}
				if (!g_no_line_directives) {
					fprintf (file, "\n%s", line_prefix);
					if (with_lines) {
						fprintf (file, "%*d|", 4, pos->line);
					}
					fprintf (file, "#line %d ", pos->line);
					print_string_token (pos->filename, file);
					fprintf (file, "\n");
				}
			} else {
				while (index < (size_t) tokens[0]) {
					pos->line += 1;
					fprintf (file, "\n%s", line_prefix);
					if (with_lines) {
						fprintf (file, "%*d|", 4, pos->line);
					}
					index += 1;
				}
			}
			if (!next_const_token (tokens, 0)[-1]) {
				fprintf (file, "%*.s\n", get_token_offset (tokens), "");
			}
		} else if (tokens[-1] == Token (punctuator) && 0 == strcmp (tokens, "#") && ((prev && prev[-1] == Token (newline)) || !prev)) {
			const char	*next;
			int			should_print = 1;

			next = next_const_token (tokens, 0);
			if (next[-1]) {
				if (next[-1] && next[-1] == Token (identifier) && 0 == strcmp (next, "line")) {
					next = next_const_token (next, 0);
					if (next[-1] && next[-1] == Token (preprocessing_number)) {
						pos->line = atoi (next) - 1;
						next = next_const_token (next, 0);
						if (next[-1] && next[-1] == Token (string)) {
							pos->filename = next;
							if (g_no_line_directives) {
								tokens = next_const_token (next, 0);
								should_print = 0;
							}
						}
					}
				}
			}
			if (should_print) {
				fprintf (file, "%*.s%s", get_token_offset (tokens), "", tokens);
			}
		} else if (is_string_token (tokens[-1])) {
			fprintf (file, "%*.s", get_token_offset (tokens), "");
			print_string_token (tokens, file);
		} else {
			fprintf (file, "%*.s%s", get_token_offset (tokens), "", tokens);
		}
		prev = tokens;
		tokens = next_const_token (tokens, 0);
	}
	if (!tokens[-1] && get_token_offset (tokens) > 0) {
		fprintf (file, "%*.s", get_token_offset (tokens), "");
	}
	if (prev && prev[-1] != Token (newline)) {
		fprintf (file, "\n");
	}
}

void	print_tokens (const char *tokens, int with_lines, const char *line_prefix, FILE *file) {
	print_tokens_until (tokens, with_lines, line_prefix, Token (eof), file);
}

void	print_tokens_to_string (const char *tokens, char **pstring) {

}

#ifndef Without_Tests
void	test_tokenize_stage (void) {
	usize	size;
	const char	*filename = "test.c";
	char	*content = read_entire_file (filename, &size);
	char	*tokens;
	int		*newline_array;

	preprocess_text (content, content + size, &newline_array);
	if (content) {
		tokens = tokenize (content, newline_array, 1, filename);
		if (tokens) {
			print_tokens (tokens, 1, "", stdout);
			free_tokens (tokens);
		} else {
			Error ("no tokens");
		}
		free (content);
	} else {
		Error ("no content");
	}

}
#endif

int		copy_token (struct tokenizer *tokenizer, const char *token) {
	usize	length;
	int		success;

	length = get_token_length (token);
	if (token[-1] == Token (newline) && tokenizer->current && tokenizer->current[-1] == Token (newline)) {
		if (token[0] + tokenizer->current[0] > 0x7f) {
			int		remaining = (token[0] + tokenizer->current[0]) - 0x7f;

			tokenizer->current[0] = 0x7f;
			if ((success = prepare_tokenizer (tokenizer, Calc_Token_Size (1)))) {
				tokenizer->current = push_token_bytes (tokenizer, 0, token[-1], &remaining, 1);
			}
		} else {
			tokenizer->current[0] += token[0];
			success = 1;
		}
	} else if ((success = prepare_tokenizer (tokenizer, Calc_Token_Size (length)))) {
		tokenizer->current = push_token_bytes (tokenizer, get_token_offset (token), token[-1], token, get_token_length (token));
	}
	return (success);
}

