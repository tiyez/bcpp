



struct tokenizer {
	char	*start_data;
	char	*data;
	usize	size;
	usize	cap;
	char	*current;
	char	*prev;
};

enum token {
	Token_eof,
	Token_newline,
	Token_identifier,
	Token_preprocessing_number,
	Token_punctuator,
	Token_string,
	Token_character,
	Token_path_global,
	Token_path_relative,
	Token_link,
};

struct position {
	const char	*filename;
	int			line;
	int			column;
	int			at_the_beginning;
};

int		is_string_token (int token) {
	return (token >= Token_string && token <= Token_path_relative);
}

const char	*get_token_name (const char *token) {
	switch (token[-1]) {
		case Token_eof: return "Token_eof";
		case Token_newline: return "Token_newline";
		case Token_identifier: return "Token_identifier";
		case Token_preprocessing_number: return "Token_preprocessing_number";
		case Token_punctuator: return "Token_punctuator";
		case Token_string: return "Token_string";
		case Token_character: return "Token_character";
		case Token_path_global: return "Token_path_global";
		case Token_path_relative: return "Token_path_relative";
		case Token_link: return "Token_link";
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
	push_tokenizer_byte (tokenizer, (value & 0xFF00) >> 8 );
	push_tokenizer_byte (tokenizer, value & 0xFF);
}

void	push_tokenizer_bytes (struct tokenizer *tokenizer, void *bytes, usize length) {
	memcpy (tokenizer->data + tokenizer->size, bytes, length);
	tokenizer->size += length;
}

int		prepare_tokenizer (struct tokenizer *tokenizer, usize tofit) {
	int		success;

	if (tokenizer->size + tofit + (2 + 2 + 1 + sizeof (void *) * 2) > tokenizer->cap) {
		void	*memory;

		memory = expand_array (0, &tokenizer->cap);
		if (memory) {
			if (!tokenizer->start_data) {
				tokenizer->start_data = memory;
			}
			if (tokenizer->data) {
				*(void **) tokenizer->data = memory;
				push_tokenizer_2bytes (tokenizer, 0);
				push_tokenizer_2bytes (tokenizer, 0);
				push_tokenizer_byte (tokenizer, Token_link);
				push_tokenizer_bytes (tokenizer, &memory, sizeof memory);
			}
			tokenizer->data = memory;
			tokenizer->size = sizeof memory;
			*(void **) tokenizer->data = 0;
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
	token[-3] = (offset >> 8) & 0xFF;
}

void	set_token_length (char *token, int length) {
	token[-4] = length & 0xFF;
	token[-5] = (length >> 8) & 0xFF;
}

int		get_token_offset (const char *token) {
	int		offset;

	offset = (unsigned char) token[-2];
	offset += (unsigned char) token[-3] << 8;
	return (offset);
}

int		get_token_length (const char *token) {
	int		length;

	length = (unsigned char) token[-4];
	length += (unsigned char) token[-5] << 8;
	return (length);
}

int		push_token (struct tokenizer *tokenizer, int offset, int token, const char *data, usize length) {
	int		success;

	if ((success = prepare_tokenizer (tokenizer, length + 6))) {
		push_tokenizer_2bytes (tokenizer, length);
		push_tokenizer_2bytes (tokenizer, offset);
		push_tokenizer_byte (tokenizer, token);
		tokenizer->prev = tokenizer->current;
		tokenizer->current = tokenizer->data + tokenizer->size;
		push_tokenizer_bytes (tokenizer, (void *) data, length);
		push_tokenizer_byte (tokenizer, 0);
	}
	return (success);
}

int		push_newline_token (struct tokenizer *tokenizer, int offset) {
	int		success;

	if (tokenizer->current && tokenizer->current[-1] == Token_newline && tokenizer->current[0] < 0x7f) {
		tokenizer->current[0] += 1;
		success = 1;
	} else {
		if ((success = prepare_tokenizer (tokenizer, 7))) {
			push_tokenizer_2bytes (tokenizer, 1);
			push_tokenizer_2bytes (tokenizer, offset);
			push_tokenizer_byte (tokenizer, Token_newline);
			tokenizer->prev = tokenizer->current;
			tokenizer->current = tokenizer->data + tokenizer->size;
			push_tokenizer_byte (tokenizer, 1);
			push_tokenizer_byte (tokenizer, 0);
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

int		push_string_token (struct tokenizer *tokenizer, int offset, const char *string, usize length, int push_at_existing) {
	int		success;

	if (push_at_existing && tokenizer->current && tokenizer->current[-1] == Token_string) {
		tokenizer->size -= 1;
		if ((success = prepare_tokenizer (tokenizer, length + 1))) {
			push_tokenizer_bytes (tokenizer, (void *) string, length);
			push_tokenizer_byte (tokenizer, 0);
			Debug ("old length %zu", length);
			length += get_token_length (tokenizer->current);
			Debug ("new length %zu", length);
			set_token_length (tokenizer->current, length);
			Debug ("actual length %d", get_token_length (tokenizer->current));
			Assert (get_token_length (tokenizer->current) == (int) length);
		}
	} else if ((success = prepare_tokenizer (tokenizer, length + 6))) {
		push_tokenizer_2bytes (tokenizer, length);
		push_tokenizer_2bytes (tokenizer, offset);
		push_tokenizer_byte (tokenizer, Token_string);
		tokenizer->prev = tokenizer->current;
		tokenizer->current = tokenizer->data + tokenizer->size;
		push_tokenizer_bytes (tokenizer, (void *) string, length);
		push_tokenizer_byte (tokenizer, 0);
		Assert (get_token_length (tokenizer->current) >= 0);
	}
	return (success);
}

const char	*next_const_token (const char *tokens, struct position *pos) {
	const char	*old = tokens;

	if (pos) {
		if (tokens[-1] == Token_newline) {
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
	tokens += 1; /* skip null-term */
	tokens += 2; /* skip length */
	tokens += 2; /* skip offset */
	tokens += 1; /* skip token kind */
	if (tokens[-1] == Token_link) {
		void	*memory = *(void **) tokens;

		tokens = memory;
		tokens += sizeof memory; /* skip next data pointer */
		tokens += 2; /* skip length */
		tokens += 2; /* skip offset */
		tokens += 1; /* skip token kind */
	}
	if (pos && tokens[-1] != Token_newline) {
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
	return (tokenizer->start_data ? tokenizer->start_data + sizeof (void *) + 5 : 0);
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

	if ((success = tokenizer->cap - tokenizer->size >= 6 || prepare_tokenizer (tokenizer, 6))) {
		push_tokenizer_2bytes (tokenizer, 0);
		push_tokenizer_2bytes (tokenizer, offset);
		push_tokenizer_byte (tokenizer, Token_eof);
		tokenizer->prev = tokenizer->current;
		tokenizer->current = tokenizer->data + tokenizer->size;
		push_tokenizer_byte (tokenizer, 0);
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
		push_token (tokenizer, state->offset, Token_identifier, start, content - start);
		state->offset = 0;
		state->its_include = 0;
		if (state->check_include && (0 == strncmp (start, "include", content - start) || 0 == strncmp (start, "import", content - start))) {
			state->its_include = 1;
		}
		state->check_include = 0;
	} else if (isdigit (*content) || (*content == '.' && isdigit (content[1]))) {
		const char	*start = content;

		content += (*content == '.');
		do {
			content += 1 + ((*content == 'e' || *content == 'E' || *content == 'p' || *content == 'P') && (content[1] == '+' || content[1] == '-'));
		} while (isalnum (*content) || *content == '_' || *content == '.');
		push_token (tokenizer, state->offset, Token_preprocessing_number, start, content - start);
		state->offset = 0;
		state->column += content - start;
		state->check_include = 0; state->its_include = 0;
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
				tokenizer->current[-1] = Token_character;
			} else if (end_symbol == '>') {
				tokenizer->current[-1] = Token_path_global;
			} else if (state->its_include) {
				tokenizer->current[-1] = Token_path_relative;
			}
			content += 1;
		}
		state->column += (content - start) + 1;
		state->offset = 0;
		state->check_include = 0; state->its_include = 0;
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
		state->check_include = 0; state->its_include = 0;
		if (length == 1 && *content == '#' && ((tokenizer->current && tokenizer->current[-1] == Token_newline) || !tokenizer->current)) {
			state->check_include = 1;
		}
		if (length == 2 && (*content == '<' || *content == '%' || *content == ':')) {
			if (0 == strncmp (content, "<%", 2)) {
				push_token (tokenizer, state->offset, Token_punctuator, "{", 1);
			} else if (0 == strncmp (content, "%>", 2)) {
				push_token (tokenizer, state->offset, Token_punctuator, "}", 1);
			} else if (0 == strncmp (content, "<:", 2)) {
				push_token (tokenizer, state->offset, Token_punctuator, "[", 1);
			} else if (0 == strncmp (content, ":>", 2)) {
				push_token (tokenizer, state->offset, Token_punctuator, "]", 1);
			} else if (0 == strncmp (content, "%:", 2)) {
				push_token (tokenizer, state->offset, Token_punctuator, "#", 1);
			} else {
				push_token (tokenizer, state->offset, Token_punctuator, content, length);
			}
		} else if (length == 4 && 0 == strncmp (content, "%:%:", 4)) {
			push_token (tokenizer, state->offset, Token_punctuator, "##", 2);
		} else {
			push_token (tokenizer, state->offset, Token_punctuator, content, length);
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
	if (tokenizer->prev) {
		if (tokenizer->current[-1] == Token_newline) {
			tokenizer->size -= 2;
		} else {
			tokenizer->size -= get_token_length (tokenizer->current) + 1;
		}
		tokenizer->size -= 5;
		tokenizer->current = tokenizer->prev;
		tokenizer->prev = 0;
		success = 1;
	} else if (tokenizer->current) {
		tokenizer->size = sizeof (void *);
		tokenizer->current = 0;
		success = 1;
	} else {
		success = 1;
	}
	return (success);
}

void	free_tokens (const char *tokens) {
	void	**memory = (void **) (tokens - 5 - sizeof (void *));

	while (*memory) {
		void	*next = *memory;

		free (memory);
		memory = (void **) next;
	}
	free (memory);
}

void	free_tokenizer (struct tokenizer *tokenizer) {
	if (tokenizer->start_data) {
		free_tokens (get_first_token (tokenizer));
	}
	memset (tokenizer, 0, sizeof *tokenizer);
}

void	reset_tokenizer (struct tokenizer *tokenizer) {
	if (tokenizer->data) {
		tokenizer->size = sizeof (void *);
	}
}

char	*tokenize_with (struct tokenizer *tokenizer, const char *content, int *nl_array, int ensure_nl_at_end, const char *filename);

char	*tokenize (const char *content, int *nl_array, int ensure_nl_at_end, const char *filename) {
	struct tokenizer	tokenizer = {0};

	return (tokenize_with (&tokenizer, content, nl_array, ensure_nl_at_end, filename));
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

	while (*content && success) {
		success = make_token (tokenizer, &state, &content);
	}
	if (success) {
		if (ensure_nl_at_end && tokenizer->current && tokenizer->current[-1] != Token_newline) {
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
	return (get_next_from_tokenizer (tokenizer, begin));
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
		case Token_string: result = '\"'; break ;
		case Token_character: result = '\''; break ;
		case Token_path_global: result = '<'; break ;
		case Token_path_relative: result = '\"'; break ;
	}
	return (result);
}

int		get_close_string (int tkn) {
	int		result = 0;

	switch (tkn) {
		case Token_string: result = '\"'; break ;
		case Token_character: result = '\''; break ;
		case Token_path_global: result = '>'; break ;
		case Token_path_relative: result = '\"'; break ;
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

	if (tokenizer->current && tokenizer->current[-1] && tokenizer->current[-1] != Token_newline && token[-1] && token[-1] != Token_newline) {
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

			if (tokenizer->prev) {
				tokenizer->size -= get_token_length (tokenizer->current) + 1 + 5;
				tokenizer->current = tokenizer->prev;
				tokenizer->prev = 0;
				success = make_token (tokenizer, &state, (const char **) &ptr);
				if (success) {
					if ((success = (*ptr == 0))) {
						set_token_offset (tokenizer->current, offset);
					} else {
						Error_Message (pos, "resulting token is invalid");
						success = 0;
					}
				}
			} else {
				Error_Message (pos, "prev pointer is null");
				success = 0;
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
		if (tokens[-1] == Token_newline) {
			size_t	index = 0, initial_line = pos->line;

			if (tokens[0] > 16) {
				const char	*next;
				int		old_line = pos->line;

				pos->line += tokens[0];
				next = next_const_token (tokens, 0);
				while (next[-1] == Token_newline) {
					pos->line += next[0];
					tokens = next;
					next = next_const_token (next, 0);
				}
				fprintf (file, "\n%s", line_prefix);
				if (with_lines) {
					fprintf (file, "%*d|", 4, pos->line);
				}
				fprintf (file, "#line %d ", pos->line);
				print_string_token (pos->filename, file);
				fprintf (file, "\n");
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
		} else if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "#") && ((prev && prev[-1] == Token_newline) || !prev)) {
			const char	*next;

			next = next_const_token (tokens, 0);
			if (next[-1]) {
				if (next[-1] && next[-1] == Token_identifier && 0 == strcmp (next, "line")) {
					next = next_const_token (next, 0);
					if (next[-1] && next[-1] == Token_preprocessing_number) {
						pos->line = atoi (next) - 1;
						next = next_const_token (next, 0);
						if (next[-1] && next[-1] == Token_string) {
							pos->filename = next;
						}
					}
				}
			}
			fprintf (file, "%*.s%s", get_token_offset (tokens), "", tokens);
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
	if (prev && prev[-1] != Token_newline) {
		fprintf (file, "\n");
	}
}

void	print_tokens (const char *tokens, int with_lines, const char *line_prefix, FILE *file) {
	print_tokens_until (tokens, with_lines, line_prefix, Token_eof, file);
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
	if (token[-1] == Token_newline && tokenizer->current && tokenizer->current[-1] == Token_newline) {
		if (token[0] + tokenizer->current[0] > 0x7f) {
			int		remaining = (token[0] + tokenizer->current[0]) - 0x7f;

			tokenizer->current[0] = 0x7f;
			if ((success = prepare_tokenizer (tokenizer, 7))) {
				push_tokenizer_2bytes (tokenizer, 1);
				push_tokenizer_2bytes (tokenizer, 0);
				push_tokenizer_byte (tokenizer, token[-1]);
				tokenizer->prev = tokenizer->current;
				tokenizer->current = tokenizer->data + tokenizer->size;
				push_tokenizer_byte (tokenizer, remaining);
				push_tokenizer_byte (tokenizer, 0);
			}
		} else {
			tokenizer->current[0] += token[0];
			success = 1;
		}
	} else if ((success = prepare_tokenizer (tokenizer, length + 6))) {
		push_tokenizer_byte (tokenizer, token[-5]);
		push_tokenizer_byte (tokenizer, token[-4]);
		push_tokenizer_byte (tokenizer, token[-3]);
		push_tokenizer_byte (tokenizer, token[-2]);
		push_tokenizer_byte (tokenizer, token[-1]);
		tokenizer->prev = tokenizer->current;
		tokenizer->current = tokenizer->data + tokenizer->size;
		push_tokenizer_bytes (tokenizer, (void *) token, length);
		push_tokenizer_byte (tokenizer, 0);
	}
	return (success);
}

