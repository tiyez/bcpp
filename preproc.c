

struct macro_stack {
	usize		size;
	const char	*macros[128 + 1];
};

int		push_macro_stack (struct macro_stack *stack, const char *macro) {
	int		success;

	if (stack->size < Array_Count (stack->macros) - 1) {
		stack->macros[stack->size] = macro;
		stack->size += 1;
		stack->macros[stack->size] = 0;
		success = 1;
	} else {
		Error ("macro stack overflow. maximum depth of macro expansion is reached.");
		success = 0;
	}
	return (success);
}

void	pop_macro_stack (struct macro_stack *stack) {
	if (stack->size > 0) {
		stack->size -= 1;
		stack->macros[stack->size] = 0;
	}
}

int		is_macro_pushed_to_stack (struct macro_stack *stack, const char *macro) {
	const char	**macros = stack->macros;
	int			success;

	if (stack->size > 0) {
		while (*macros && *macros != macro) {
			macros += 1;
		}
		success = (*macros == macro);
	} else {
		success = 0;
	}
	return (success);
}

struct macro_desc {
	const char	*ident;
	const char	*args;
	const char	*body;
	usize		args_count;
	int			is_variadic;
	int			is_multiline;
	int			is_undef;
};

struct bcpp {
	const char			*include_paths;
	const char			**include_paths_sorted;
	struct tokenizer	do_not_include_those;
	char				*predefined;
	usize				predefined_size;
	struct macro_desc	*macros;
	struct macro_desc	*macros_data;
	usize				macros_size;
	usize				macros_cap;
	struct tokenizer	macro_tokenizer;
	struct macro_stack	macro_stack;
};

#include "expr.c"

int		evaluate_directives (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *tokens, const char *filename);

int		add_content_to_translation_unit (struct bcpp *bcpp, struct tokenizer *tokenizer, char *content, usize size, const char *filename, int allocated_content) {
	char	*tokens;
	int		*newline_array;
	int		success;

	preprocess_text (content, content + size, &newline_array);
	if (content) {
		tokens = tokenize (content, newline_array, filename);
		if (allocated_content) {
			free (content);
		}
		content = 0;
		free (newline_array);
		newline_array = 0;
		if (tokens) {
			success = evaluate_directives (bcpp, tokenizer, tokens, filename);
			free_tokens (tokens);
			tokens = 0;
		} else {
			Error ("no tokens");
			success = 0;
		}
	} else {
		Error ("no content");
		success = 0;
	}
	return (success);
}

int		add_file_to_translation_unit (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *filename) {
	usize	size;
	char	*content;
	int		success;

	content = read_entire_file (filename, &size);
	if (content) {
		success = add_content_to_translation_unit (bcpp, tokenizer, content, size, filename, 1);
	} else {
		success = 0;
	}
	return (success);
}

void	print_macro_list (struct macro_desc *macros);

char	*make_translation_unit (struct bcpp *bcpp, const char *filename) {
	int		success;
	struct tokenizer	ctokenizer = {0}, *tokenizer = &ctokenizer;

	success = add_content_to_translation_unit (bcpp, tokenizer, bcpp->predefined, bcpp->predefined_size, "<predefined>", 0);
	if (success) {
		if (!add_file_to_translation_unit (bcpp, tokenizer, filename)) {
			Error ("cannot add file to the translation unit of '%s'", filename);
			free_tokenizer (tokenizer);
			success = 0;
		} else if (!end_tokenizer (tokenizer)) {
			Error ("cannot end tokenizer");
			free_tokenizer (tokenizer);
			success = 0;
		} else {
			success = 1;
		}
	} else {
		free_tokenizer (tokenizer);
	}
	return (get_first_token (tokenizer));
}

int		make_line_directive (struct tokenizer *tokenizer, const char *path, int line) {
	int		success;
	char	numbuf[32];
	int		numbuf_length;
	usize	path_length = strlen (path);

	numbuf_length = snprintf (numbuf, sizeof numbuf, "%d", line);
	success = push_token (tokenizer, 0, Token_punctuator, "#", 1);
	success = success && push_token (tokenizer, 0, Token_identifier, "line", 4);
	success = success && push_token (tokenizer, 1, Token_preprocessing_number, numbuf, numbuf_length);
	success = success && push_string_token (tokenizer, 1, path, path_length, 0);
	success = success && push_newline_token (tokenizer);
	return (success);
}

int		is_file_already_included (struct bcpp *bcpp, const char *path) {
	int			result = 0;
	const char	*tokens;

	tokens = get_first_token (&bcpp->do_not_include_those);
	while (!result && tokens[-1]) {
		result = (0 == strcmp (tokens, path));
	}
	return (result);
}

int		include_file_global (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *filename, struct position *pos) {
	int			success;
	const char	**includes;
	char		path[512];

	if (bcpp->include_paths) {
		int		found = 0;

		includes = bcpp->include_paths_sorted;
		while (*includes) {
			usize	length = strlen (*includes);

			strcpy (path, *includes);
			if (path[length - 1] != '/') {
				path[length] = '/';
				strcpy (path + length + 1, filename);
			} else {
				strcpy (path + length, filename);
			}
			if (check_file_access (path, Access_Mode_read)) {
				found = 1;
				if (!is_file_already_included (bcpp, path)) {
					success = add_file_to_translation_unit (bcpp, tokenizer, path);
				} else {
					success = 1;
				}
				break ;
			}
			includes += 1;
		}
		if (!found) {
			Error_Message (pos, "file not found");
			success = 0;
		}
	} else {
		Error_Message (pos, "file not found");
		success = 0;
	}
	return (success);
}

int		include_file_relative (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *filename, const char *relative_from, struct position *pos) {
	int		success;
	char	*ptr;
	char	path[512];

	if ((ptr = strrchr (relative_from, '/'))) {
		memcpy (path, relative_from, ptr - relative_from + 1);
		strcpy (path + (ptr - relative_from + 1), filename);
	} else {
		strcpy (path, filename);
	}
	if (check_file_access (path, Access_Mode_read)) {
		if (!is_file_already_included (bcpp, path)) {
			success = add_file_to_translation_unit (bcpp, tokenizer, path);
		} else {
			success = 1;
		}
	} else {
		success = include_file_global (bcpp, tokenizer, filename, pos);
	}
	return (success);
}

int		is_function_like_macro (const char *macro) {
	macro = next_const_token (macro, 0);
	return (macro[-1] == Token_punctuator && 0 == strcmp (macro, "(") && 0 == get_token_offset (macro));
}

int		is_function_like_macro_call (const char *macro) {
	do {
		macro = next_const_token (macro, 0);
	} while (macro[-1] && macro[-1] == Token_newline);
	return (macro[-1] == Token_punctuator && 0 == strcmp (macro, "("));
}

#define Iterate_Macro(macro) ((macro)->ident || (((macro) = (macro) + 1)->ident && ((macro) = (struct macro_desc *) (macro)->ident + 1)->ident))

struct macro_desc	*find_macro (struct macro_desc *macros, const char *identifier) {
	struct macro_desc	*macro;

	if (macros) {
		macro = 0;
		while (Iterate_Macro (macros)) {
			if (!macros->is_undef && 0 == strcmp (macros->ident, identifier)) {
				macro = macros;
				break ;
			}
			macros += 1;
		}
	} else {
		macro = 0;
	}
	return (macro);
}

void	print_macro_list (struct macro_desc *macros) {
	Debug ("macro list:");
	if (macros) while (Iterate_Macro (macros)) {
		print_tokens_until (macros->ident, 0, "|", Token_eof, stderr);
		macros += 1;
	}
}

struct macro_args {
	usize		size;
	const char	*tokens[32][3]; /* 0 - macro, 1 - macro call expanded, 2 - macro call */
};

#define Macro_Arg 0
#define Macro_Expanded_Arg 1
#define Macro_Call_Arg 2

int		push_macro_arg (struct macro_args *args, const char *arg) {
	int		success;

	if (args->size < Array_Count (args->tokens)) {
		args->tokens[args->size][0] = arg;
		args->tokens[args->size][1] = 0;
		args->tokens[args->size][2] = 0;
		args->size += 1;
		success = 1;
	} else {
		Error ("macro args overflow. maximum number of macro arguments is exceeded. %zu", args->size);
		success = 0;
	}
	return (success);
}

int		set_macro_arg (struct macro_args *args, const char *arg, usize index, int type) {
	int		success;

	if (index < args->size) {
		args->tokens[index][type] = arg;
		success = 1;
	} else {
		Error ("invalid macro argument index %zu < %zu", index, args->size);
		success = 0;
	}
	return (success);
}

int		find_macro_arg (struct macro_args *args, const char *identifier, usize *index, struct position *pos) {
	int		success;
	usize	arg_index = 0;

	if (0 == strcmp (identifier, "__VA_ARGS__")) {
		if (args->size > 0 && 0 == strcmp (args->tokens[args->size - 1][0], "...")) {
			arg_index = args->size - 1;
			success = 1;
		} else {
			Error_Message (pos, "'__VA_ARGS__' token in non-variadic macro body");
			success = 0;
		}
	} else {
		while (arg_index < args->size && 0 != strcmp (identifier, args->tokens[arg_index][0])) {
			arg_index += 1;
		}
		success = 1;
	}
	*index = arg_index;
	return (success);
}

int		init_macro_args (struct macro_args *args, struct macro_desc *desc) {
	int		success;

	if (desc->args) {
		const char	*arg = next_const_token (desc->args, 0);

		success = 1;
		while (success && arg[-1]) {
			if (arg[-1] == Token_identifier || (desc->is_variadic && arg[-1] == Token_punctuator && 0 == strcmp (arg, "..."))) {
				success = push_macro_arg (args, arg);
				arg = next_const_token (arg, 0);
				if (success && arg[-1] == Token_punctuator && (0 == strcmp (arg, ",") || 0 == strcmp (arg, ")"))) {
					if (arg[0] == ')') {
						break ;
					}
				} else {
					success = 0;
				}
			} else {
				success = 0;
			}
			arg = next_const_token (arg, 0);
		}
	} else {
		success = 1;
	}
	return (success);
}

int		evaluate_token (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **ptokens, struct position *pos, int check_macro_stack);

int		evaluate_macro_call (struct bcpp *bcpp, struct tokenizer *tokenizer, struct macro_desc *desc, const char **ptokens, struct macro_args *args, struct position *pos) {
	const char	*tokens = *ptokens, *original = tokens;
	int			success;

	if (desc->args && desc->args_count + desc->is_variadic > 0) {
		const char	*ptr;

		do {
			ptr = next_const_token (tokens, pos);
		} while (ptr[-1] && ptr[-1] == Token_newline);
		if (ptr[-1] == Token_punctuator && 0 == strcmp (ptr, "(")) {
			usize		group_level = 0, arg_index = 0;
			int			is_part_of_va;
			char		*begin;

			success = 1;
			is_part_of_va = (desc->is_variadic && arg_index + 1 == args->size);
			ptr = next_const_token (ptr, pos);
			begin = tokenizer->current;
			while (success && ptr[-1]) {
				if (group_level <= 0 && ptr[-1] == Token_punctuator && (0 == strcmp (ptr, ")") || (!is_part_of_va && 0 == strcmp (ptr, ",")))) {
					success = end_tokenizer (tokenizer);
					if (success) {
						begin = get_next_from_tokenizer (tokenizer, begin);
						success = set_macro_arg (args, begin, arg_index, Macro_Call_Arg);
						begin = tokenizer->current;
						arg_index += 1;
						is_part_of_va = (desc->is_variadic && arg_index + 1 == args->size);
						if (ptr[0] == ',') {
							ptr = next_const_token (ptr, pos);
							continue ;
						}
					}
					if (!success || ptr[0] == ')') {
						break ;
					}
				}
				if (ptr[-1] == Token_newline) {
					ptr = next_const_token (ptr, pos);
				} else {
					if (ptr[-1] == Token_punctuator) {
						group_level += (0 == strcmp (ptr, "("));
						group_level -= (0 == strcmp (ptr, ")"));
					}
					success = copy_token (tokenizer, ptr);
					ptr = next_const_token (ptr, pos);
				}
			}
			if (success) {
				if (ptr[-1]) {
					usize	index;

					*ptokens = next_const_token (ptr, pos);
					index = 0;
					while (success && index < args->size) {
						const char	*arg;
						char		*begin;

						begin = tokenizer->current;
						arg = args->tokens[index][Macro_Call_Arg];
						while (success && arg[-1]) {
							struct position	pos = { .filename = "<macro>", .line = 1, .column = 1, };

							success = evaluate_token (bcpp, tokenizer, &arg, &pos, 1);
						}
						success = success && end_tokenizer (tokenizer);
						if (success) {
							begin = get_next_from_tokenizer (tokenizer, begin);
							success = set_macro_arg (args, begin, index, Macro_Expanded_Arg);
						}
						index += 1;
					}
				} else {
					Error_Message (pos, "invalid function-like macro call");
					print_tokens_until (original, 0, "orig|", Token_newline, stderr);
					success = 0;
				}
			}
		} else {
			*ptokens = next_const_token (*ptokens, pos);
			success = 1;
		}
	} else {
		*ptokens = next_const_token (*ptokens, pos);
		success = 1;
	}
	return (success);
}

int		define_macro (struct bcpp *bcpp, const char **ptokens, struct position *pos) {
	int			success;
	const char	*tokens = *ptokens;

	if (bcpp->macros_size + 3 > bcpp->macros_cap / sizeof *bcpp->macros_data) {
		void	*memory;

		memory = expand_array (0, &bcpp->macros_cap);
		if (memory) {
			if (!bcpp->macros) {
				bcpp->macros = (struct macro_desc *) memory + 1;
			}
			if (bcpp->macros_data) {
				Debug ("NEW PAGE %zu", bcpp->macros_cap);
				bcpp->macros_data[0].ident = memory;
				memset (bcpp->macros_data + bcpp->macros_size, 0, sizeof *bcpp->macros_data * 2);
				bcpp->macros_data[bcpp->macros_size + 1].ident = memory;
			}
			bcpp->macros_data = memory;
			memset (bcpp->macros_data, 0, sizeof *bcpp->macros_data * 3);
			bcpp->macros_size = 1;
			success = 1;
		} else {
			success = 0;
		}
	} else {
		success = 1;
	}
	if (success) {
		struct macro_desc	*desc;
		struct tokenizer	*tokenizer;

		desc = bcpp->macros_data + bcpp->macros_size;
		bcpp->macros_size += 1;
		memset (desc, 0, sizeof *desc * 3);
		tokenizer = &bcpp->macro_tokenizer;
		if (tokens[-1] == Token_identifier) {
			success = copy_token (tokenizer, tokens);
			desc->ident = tokenizer->current;
			if (is_function_like_macro (tokens)) {
				tokens = next_const_token (tokens, pos);
				success = copy_token (tokenizer, tokens);
				desc->args = tokenizer->current;
				tokens = next_const_token (tokens, pos);
				success = 1;
				while (success && tokens[-1] && tokens[-1] != Token_newline && !(tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")"))) {
					if (tokens[-1] == Token_identifier) {
						if ((success = copy_token (tokenizer, tokens))) {
							desc->args_count += 1;
							tokens = next_const_token (tokens, pos);
							if (tokens[-1] == Token_punctuator && (0 == strcmp (tokens, ",") || 0 == strcmp (tokens, ")"))) {
								success = copy_token (tokenizer, tokens);
								if (tokens[0] == ',') {
									tokens = next_const_token (tokens, pos);
								}
							} else {
								Error_Message (pos, "invalid macro parameter separator");
								success = 0;
							}
						}
					} else if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "...")) {
						if ((success = copy_token (tokenizer, tokens))) {
							desc->is_variadic = 1;
							tokens = next_const_token (tokens, pos);
							success = copy_token (tokenizer, tokens);
							if (success && !(tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")"))) {
								Error_Message (pos, "'...' token must be last one in macro parameter list");
								success = 0;
							}
						}
					} else {
						Error_Message (pos, "invalid macro parameter name");
						success = 0;
					}
				}
				if (success && tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")")) {
				} else {
					Error_Message (pos, "invalid macro parameter list");
					success = 0;
				}
				if (success) {
					char	*begin = tokenizer->current;

					tokens = next_const_token (tokens, pos);
					if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "##")) {
						Debug ("MULTILINE!!!");
						desc->is_multiline = 1;
						success = copy_token (tokenizer, tokens);
						if (success) {
							tokens = next_const_token (tokens, pos);
							if (tokens[-1] == Token_newline) {
								success = copy_token (tokenizer, tokens);
								begin = tokenizer->current;
								tokens = next_const_token (tokens, pos);
								while (success && tokens[-1]) {
									if (pos->at_the_beginning && is_token (tokens, Token_punctuator, "#")) {
										if (is_token (next_const_token (tokens, 0), Token_identifier, "end")) {
											while (tokens[-1] && tokens[-1] != Token_newline) {
												tokens = next_const_token (tokens, pos);
											}
											break ;
										} else {
											Error_Message (pos, "directives in the multiline macro body scope are forbidden");
											success = 0;
										}
									} else {
										success = copy_token (tokenizer, tokens);
										tokens = next_const_token (tokens, pos);
									}
								}
							} else {
								Error_Message (pos, "'##' token cannot appear at the beginning of macro body");
								success = 0;
							}
						}
					} else {
						while (success && tokens[-1] != Token_newline) {
							success = copy_token (tokenizer, tokens);
							tokens = next_const_token (tokens, pos);
						}
					}
					success = end_tokenizer (tokenizer) && success;
					desc->body = get_next_from_tokenizer (tokenizer, begin);
					print_tokens_until (desc->ident, 1, "Macro Added", Token_eof, stderr);
					*ptokens = tokens;
				}
			} else {
				char	*begin = tokenizer->current;

				tokens = next_const_token (tokens, pos);
				while (success && tokens[-1] && tokens[-1] != Token_newline) {
					success = copy_token (tokenizer, tokens);
					tokens = next_const_token (tokens, pos);
				}
				success = end_tokenizer (tokenizer) && success;
				desc->body = get_next_from_tokenizer (tokenizer, begin);
				print_tokens_until (desc->ident, 1, "Macro Added", Token_eof, stderr);
				*ptokens = tokens;
			}
		} else {
			Error_Message (pos, "invalid macro identifier");
			success = 0;
		}
	} else {
		Error ("something wrong");
		success = 0;
	}
	return (success);
}

int		are_macros_equivalent (const char *left, const char *right) {
	int		result;
	int		is_function_like;

	print_tokens_until (left, 1, "DEBUG|", Token_eof, stderr);
	if (0 == strcmp (left, right)) {
		if ((is_function_like = is_function_like_macro (left)) == is_function_like_macro (right)) {
			if (is_function_like) {
				result = 1;
				while (result && !(left[-1] == Token_punctuator && 0 == strcmp (left, ")"))) {
					if (!(left[-1] == right[-1] && 0 == strcmp (left, right))) {
						result = 0;
					}
					left = next_const_token (left, 0);
					right = next_const_token (right, 0);
				}
				if (result && !(left[-1] == right[-1] && 0 == strcmp (left, right))) {
					result = 0;
				}
			} else {
				result = 1;
			}
			if (result) {
				result = (left[-1] == right[-1]);
				if (result && left[-1] && left[-1] != Token_newline) {
					result = (0 == strcmp (left, right));
					left = next_const_token (left, 0);
					right = next_const_token (right, 0);
					while (result && left[-1] == right[-1] && left[-1] != Token_newline && left[-1]) {
						result = get_token_offset (left) == get_token_offset (right) && left[-1] == right[-1] && 0 == strcmp (left, right);
						if (!result) {
							Debug ("body mismatch");
						}
						left = next_const_token (left, 0);
						right = next_const_token (right, 0);
					}
					result = (result && (left[-1] == Token_newline || !left[-1]) == (right[-1] == Token_newline || !right[-1]));
					if (!result) {
						Debug ("finale mismatch %d %d %s", left[-1], right[-1], left);
					}
				}
			}
		} else {
			Debug ("function-like mismatch");
			result = 0;
		}
	} else {
		Debug ("identifier mismatch");
		result = 0;
	}
	return (result);
}

int		concatenate_macro_token (struct tokenizer *tokenizer, const char *token, struct macro_args *args, struct position *pos) {
	int			success;
	usize		arg_index = 0;

	if (!args || find_macro_arg (args, token, &arg_index, pos)) {
		if (args && arg_index < args->size) {
			const char	*arg = args->tokens[arg_index][Macro_Call_Arg];

			if (arg[-1]) {
				success = concatenate_token (tokenizer, arg, pos);
				if (success) {
					arg = next_const_token (arg, 0);
					while (success && arg[-1]) {
						success = copy_token (tokenizer, arg);
						arg = next_const_token (arg, 0);
					}
				}
			} else {
				success = 1;
			}
		} else {
			success = concatenate_token (tokenizer, token, pos);
		}
	} else {
		success = 0;
	}
	return (success);
}

struct unescape_pusher {
	struct tokenizer	*tokenizer;
	int					offset;
	int					push_at_existing;
	usize				size;
	char				buffer[256];
};

int		push_unescaped_char (struct unescape_pusher *pusher, int ch) {
	int		success;
	char	unesc[32], *punesc = unesc;

	unescape_symbol (ch, &punesc);
	if (pusher->size + (punesc - unesc) > Array_Count (pusher->buffer)) {
		success = push_string_token (pusher->tokenizer, pusher->offset, pusher->buffer, pusher->size, pusher->push_at_existing);
		pusher->push_at_existing = 1;
		pusher->size = 0;
	} else {
		success = 1;
	}
	if (success) {
		memcpy (pusher->buffer + pusher->size, unesc, punesc - unesc);
		pusher->size += punesc - unesc;
	}
	return (success);
}

int		push_char (struct unescape_pusher *pusher, int ch) {
	int		success;

	if (pusher->size >= Array_Count (pusher->buffer)) {
		success = push_string_token (pusher->tokenizer, pusher->offset, pusher->buffer, pusher->size, pusher->push_at_existing);
		pusher->push_at_existing = 1;
		pusher->size = 0;
	} else {
		success = 1;
	}
	if (success) {
		pusher->buffer[pusher->size] = ch;
		pusher->size += 1;
	}
	return (success);
}

int		stringify_tokens (struct tokenizer *tokenizer, const char *tokens, int count, int offset, const struct position *pos) {
	int		success = 1;
	int		until_end = (count == 0);
	struct unescape_pusher	pusher = {
		.tokenizer = tokenizer,
		.offset = offset,
	};

	while (success && (until_end || count > 0) && tokens[-1] && tokens[-1] != Token_newline) {
		const char	*string;

		if (pusher.size > 0 || pusher.push_at_existing) {
			if (get_token_offset (tokens) > 0) {
				success = push_char (&pusher, ' ');
			}
		}
		if (success) {
			string = tokens;
			if (is_string_token (tokens[-1])) {
				success = push_char (&pusher, get_open_string (tokens[-1]));
				while (success && *string) {
					success = push_unescaped_char (&pusher, *string);
					string += 1;
				}
				success = success && push_char (&pusher, get_close_string (tokens[-1]));
			} else {
				while (success && *string) {
					success = push_char (&pusher, *string);
					string += 1;
				}
			}
		}
		tokens = next_const_token (tokens, 0);
		count -= 1;
	}
	if (success && (pusher.size > 0 || !pusher.push_at_existing)) {
		success = push_string_token (tokenizer, offset, pusher.buffer, pusher.size, pusher.push_at_existing);
	}
	return (success);
}

int		evaluate_concatenation_and_stringization (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **pbody, struct macro_args *args, struct position *pos) {
	int			success;
	const char	*body = *pbody, *next = 0;
	char		*begin = tokenizer->current;
	int			is_multiline = 0;

	if (body[-1] == Token_punctuator && 0 == strcmp (body, "##")) {
		/* Multi-line macro */
		is_multiline = 1;
		body = next_const_token (body, pos);
		success = 1;
	} else {
		success = 1;
	}
	while (success && body[-1] && (is_multiline || body[-1] != Token_newline)) {
		next = next_const_token (body, 0);
		if (next[-1] == Token_punctuator && 0 == strcmp (next, "##")) {
			if (is_multiline && body[-1] == Token_newline) {
				Error_Message (pos, "'##' cannot appear at the beginning of line");
				success = 0;
				break ;
			}
			next = next_const_token (next, 0);
			if (next[-1] && next[-1] != Token_newline) {
				int		is_left_empty = 0;

				if (body[-1] == Token_identifier && args) {
					usize	arg_index;

					if (find_macro_arg (args, body, &arg_index, pos)) {
						if (arg_index < args->size) {
							const char	*arg = args->tokens[arg_index][Macro_Call_Arg];
							int			group_level = 0;

							is_left_empty = !arg[-1];
							while (success && arg[-1]) {
								success = copy_token (tokenizer, arg);
								arg = next_const_token (arg, 0);
							}
						} else {
							success = copy_token (tokenizer, body);
						}
					} else {
						success = 0;
					}
				} else {
					success = copy_token (tokenizer, body);
				}
				if (success) {
					if (is_left_empty) {
						success = push_token (tokenizer, get_token_offset (body), Token_identifier, "", 0);
					}
					if (success) {
						success = concatenate_macro_token (tokenizer, next, args, pos);
						next = next_const_token (next, 0);
					}
				}
			} else {
				Error_Message (pos, "'##' cannot appear at end of macro expansion");
				success = 0;
			}
		} else if (is_multiline && body[-1] == Token_punctuator && 0 == strcmp (body, "#")) {
		} else if (body[-1] == Token_punctuator && 0 == strcmp (body, "#")) {
			body = next_const_token (body, 0);
			if (body[-1] && body[-1] != Token_newline) {
				if (args && body[-1] == Token_identifier) {
					usize	arg_index = 0;

					if (find_macro_arg (args, body, &arg_index, pos)) {
						if (arg_index < args->size) {
							const char	*arg = args->tokens[arg_index][2];

							success = stringify_tokens (tokenizer, arg, 0, get_token_offset (body), pos);
						} else {
							success = stringify_tokens (tokenizer, body, 1, get_token_offset (body), pos);
						}
					} else {
						success = 0;
					}
				} else {
					success = stringify_tokens (tokenizer, body, 1, get_token_offset (body), pos);
				}
				next = next_const_token (next, 0);
			} else {
				Error_Message (pos, "invalid operand for stringify operator");
				success = 0;
			}
		} else {
			success = copy_token (tokenizer, body);
		}
		body = next;
	}
	success = success && end_tokenizer (tokenizer);
	if (success) {
		begin = get_next_from_tokenizer (tokenizer, begin);
		*pbody = begin;
	}
	return (success);
}

int		evaluate_macro (struct bcpp *bcpp, struct tokenizer *tokenizer, struct macro_desc *desc, const char **ptokens, struct position *pos) {
	int					success;
	struct macro_args	cargs = {0}, *const args = &cargs;
	struct tokenizer	cmacro_tokenizer = {0}, *const macro_tokenizer = &cmacro_tokenizer;
	int					offset = get_token_offset (*ptokens);

	init_macro_args (args, desc);
	Debug ("eval macro call for %s", desc->ident);
	success = evaluate_macro_call (bcpp, macro_tokenizer, desc, ptokens, args, pos);
	Debug ("end eval macro call for %s", desc->ident);
	if (success) {
		if (desc->args) {
			const char	*macro_body;
			char		*begin;

			print_tokens_until (desc->ident, 0, "orig|", Token_eof, stderr);
			macro_body = desc->body;
			success = evaluate_concatenation_and_stringization (bcpp, macro_tokenizer, &macro_body, args, pos);
			begin = macro_tokenizer->current;
			print_tokens_until (macro_body, 0, "--|", Token_eof, stderr);
			while (success && macro_body[-1]) {
				if (macro_body[-1] == Token_identifier) {
					usize		arg_index;

					if (find_macro_arg (args, macro_body, &arg_index, pos)) {
						if (arg_index < args->size) {
							const char	*arg = args->tokens[arg_index][Macro_Expanded_Arg];
							char		*begin = macro_tokenizer->current;

							while (success && arg[-1]) {
								success = copy_token (macro_tokenizer, arg);
								arg = next_const_token (arg, 0);
							}
							if (success) {
								begin = get_next_from_tokenizer (macro_tokenizer, begin);
								if (begin) {
									set_token_offset (begin, get_token_offset (macro_body));
								}
							}
						} else {
							success = copy_token (macro_tokenizer, macro_body);
						}
					} else {
						success = 0;
					}
					macro_body = next_const_token (macro_body, 0);
				} else {
					success = copy_token (macro_tokenizer, macro_body);
					macro_body = next_const_token (macro_body, 0);
				}
			}
			success = success && end_tokenizer (macro_tokenizer);
			if (success) {
				const char	*start;
				char	prefix[256];

				start = get_next_from_tokenizer (macro_tokenizer, begin);
				snprintf (prefix, sizeof prefix, "macro '%s'|", desc->ident);
				print_tokens (start, 1, prefix, stderr);

				if (start[-1]) {
					begin = tokenizer->current;
					success = evaluate_token (bcpp, tokenizer, &start, pos, 1);
					if (success) {
						begin = get_next_from_tokenizer (tokenizer, begin);
						if (begin) {
							set_token_offset (begin, offset);
							while (success && start[-1]) {
								success = evaluate_token (bcpp, tokenizer, &start, pos, 1);
							}
						}
					}
				}
			}
		} else {
			const char	*macro_body = desc->body;
			const char	*start;

			success = evaluate_concatenation_and_stringization (bcpp, macro_tokenizer, &macro_body, 0, pos);
			start = macro_body;
			while (success && macro_body[-1]) {
				if (macro_body == start) {
					char		*begin;

					begin = tokenizer->current;
					success = evaluate_token (bcpp, tokenizer, &macro_body, pos, 1);
					if (success) {
						begin = get_next_from_tokenizer (tokenizer, begin);
						if (begin) {
							set_token_offset (begin, offset);
						}
					}
				} else {
					success = evaluate_token (bcpp, tokenizer, &macro_body, pos, 1);
				}
			}
		}
	}
	free_tokenizer (macro_tokenizer);
	return (success);
}

int		evaluate_token (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **ptokens, struct position *pos, int check_macro_stack) {
	int			success;
	const char	*tokens = *ptokens;

	if (tokens[-1] == Token_identifier) {
		struct macro_desc	*macro;

		macro = find_macro (bcpp->macros, tokens);
		if (macro && (!macro->args || (macro->args && is_function_like_macro_call (tokens)))) {
			if (check_macro_stack && !is_macro_pushed_to_stack (&bcpp->macro_stack, macro->ident)) {
				int		old_line = pos->line;

				if ((success = push_macro_stack (&bcpp->macro_stack, macro->ident))) {
					success = evaluate_macro (bcpp, tokenizer, macro, ptokens, pos);
					pop_macro_stack (&bcpp->macro_stack);
				}
				if (old_line < pos->line) {
					push_compiled_newline_token (tokenizer, pos->line - old_line);
				}
			} else if (!check_macro_stack) {
				int		old_line = pos->line;

				success = evaluate_macro (bcpp, tokenizer, macro, ptokens, pos);
				if (old_line < pos->line) {
					push_compiled_newline_token (tokenizer, pos->line - old_line);
				}
			} else {
				success = copy_token (tokenizer, tokens);
				*ptokens = next_const_token (tokens, pos);
			}
		} else {
			success = copy_token (tokenizer, tokens);
			*ptokens = next_const_token (tokens, pos);
		}
	} else {
		success = copy_token (tokenizer, tokens);
		*ptokens = next_const_token (tokens, pos);
	}
	return (success);
}

int		expand_and_evaluate_expression (struct bcpp *bcpp, const char *tokens, isize *ret, const struct position *apos) {
	int			success;
	struct position	cpos = *apos, *pos = &cpos;
	struct tokenizer	ctokenizer = {0}, *tokenizer = &ctokenizer;

	while (tokens[-1] && tokens[-1] != Token_newline) {
		if (tokens[-1] == Token_identifier && 0 == strcmp (tokens, "defined")) {
			int		is_grouped;

			tokens = next_const_token (tokens, pos);
			if ((is_grouped = (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "(")))) {
				tokens = next_const_token (tokens, pos);
			}
			if (tokens[-1] == Token_identifier) {
				struct macro_desc	*macro;

				macro = find_macro (bcpp->macros, tokens);
				if (macro) {
					success = push_token (tokenizer, get_token_offset (tokens), Token_preprocessing_number, "1", 1);
				} else {
					success = push_token (tokenizer, get_token_offset (tokens), Token_preprocessing_number, "0", 1);
				}
				if (success) {
					tokens = next_const_token (tokens, pos);
					if (is_grouped) {
						if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")")) {
							tokens = next_const_token (tokens, pos);
						} else {
							Error_Message (pos, "')' token expected after 'defined' operand");
							success = 0;
						}
					}
				}
			} else {
				Error_Message (pos, "invalid operand for 'defined' operator [%s;%s]", get_token_name (tokens), tokens);
				success = 0;
			}
		} else if (tokens[-1] == Token_identifier) {
			struct macro_desc	*macro;

			macro = find_macro (bcpp->macros, tokens);
			if (macro && (!macro->args || (macro->args && is_function_like_macro_call (tokens)))) {
				if (!is_macro_pushed_to_stack (&bcpp->macro_stack, macro->ident)) {
					int		old_line = pos->line;

					if ((success = push_macro_stack (&bcpp->macro_stack, macro->ident))) {
						success = evaluate_macro (bcpp, tokenizer, macro, &tokens, pos);
						pop_macro_stack (&bcpp->macro_stack);
					}
					if (old_line < pos->line) {
						Error_Message (pos, "macro call must be in one line");
						success = 0;
					}
				} else {
					success = copy_token (tokenizer, tokens);
					tokens = next_const_token (tokens, pos);
				}
			} else {
				success = copy_token (tokenizer, tokens);
				tokens = next_const_token (tokens, pos);
			}
		} else {
			success = copy_token (tokenizer, tokens);
			tokens = next_const_token (tokens, pos);
		}
	}
	success = success && end_tokenizer (tokenizer);
	if (success) {
		char	*begin;

		begin = tokenizer->current;
		tokens = get_first_token (tokenizer);
		while (success && tokens[-1]) {
			if (tokens[-1] == Token_identifier) {
				const char	*next;
				struct macro_desc	*macro;

				macro = find_macro (bcpp->macros, tokens);
				if (macro) {
					int		is_function_like;

					next = next_const_token (tokens, 0);
					is_function_like = !!macro->args;
					if (is_function_like) {
						if ((next[-1] == Token_punctuator && 0 == strcmp (next, "("))) {
							int		group_level = 0;

							success = push_token (tokenizer, get_token_offset (tokens), Token_preprocessing_number, "1", 1);
							while (tokens[-1] && !(group_level == 1 && tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")"))) {
								if (tokens[-1] == Token_punctuator) {
									group_level += (0 == strcmp (tokens, "("));
									group_level -= (0 == strcmp (tokens, ")"));
								}
								tokens = next_const_token (tokens, 0);
							}
						} else {
							success = push_token (tokenizer, get_token_offset (tokens), Token_preprocessing_number, "0", 1);
						}
					} else {
						success = push_token (tokenizer, get_token_offset (tokens), Token_preprocessing_number, "1", 1);
					}
				} else {
					success = push_token (tokenizer, get_token_offset (tokens), Token_preprocessing_number, "0", 1);
				}
			} else {
				success = copy_token (tokenizer, tokens);
			}
			tokens = next_const_token (tokens, 0);
		}
		success = success && end_tokenizer (tokenizer);
		if (success) {
			begin = get_next_from_tokenizer (tokenizer, begin);
			print_tokens (begin, 1, "if expr|", stderr);
			success = evaluate_expression (begin, ret);
			if (!success) {
				print_tokens (get_first_token (tokenizer), 1, "if expr expanded|", stderr);
			}
		}
	}
	free_tokenizer (tokenizer);
	return (success);
}

int		evaluate_directives (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *tokens, const char *filename) {
	const char			*next = next_const_token (tokens, 0);
	int					success = 1;
	struct position		cpos = {
		.filename = filename,
		.line = 1,
		.column = 1,
	}, *pos = &cpos;
	int		is_active = 1;
	int		ifs_level = -1;
	struct {
		int		is_already_selected;
		int		prev_active;
	} ifs[128];

	make_line_directive (tokenizer, pos->filename, pos->line);
	while (tokens[-1] && success) {
		const char	*next = next_const_token (tokens, 0);

		if ((tokens[-1] == Token_newline && next[-1] == Token_punctuator && 0 == strcmp (next, "#")) ||
			(pos->line == 1 && tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "#"))) {
			if (tokens[-1] == Token_newline) {
				copy_token (tokenizer, tokens);
				tokens = next_const_token (tokens, pos);
				tokens = next_const_token (tokens, pos);
			} else {
				next = tokens;
				tokens = next_const_token (tokens, pos);
			}
			if (tokens[-1] == Token_identifier) {
				if (0 == strcmp (tokens, "include")) {
					if (is_active) {
						tokens = next_const_token (tokens, pos);
						/* TODO(Viktor): include with macro argument */
						if (tokens[-1] == Token_path_relative) {
							if (!include_file_relative (bcpp, tokenizer, tokens, filename, pos)) {
								Error_Message (pos, "while trying to include \"%s\" file", tokens);
								success = 0;
							}
						} else if (tokens[-1] == Token_path_global) {
							if (!include_file_global (bcpp, tokenizer, tokens, pos)) {
								Error_Message (pos, "while trying to include <%s> file", tokens);
								success = 0;
							}
						} else {
							Error_Message (pos, "invalid argument for 'include' directive, expected \"...\" or <...> strings");
							success = 0;
						}
						make_line_directive (tokenizer, pos->filename, pos->line);
					}
				} else if (0 == strcmp (tokens, "define")) {
					if (is_active) {
						tokens = next_const_token (tokens, pos);
						if (tokens[-1] == Token_identifier) {
							struct macro_desc	*macro;

							macro = find_macro (bcpp->macros, tokens);
							if (macro) {
								if (!are_macros_equivalent (macro->ident, tokens)) {
									Error_Message (pos, "'%s' macro redefinition", macro->ident);
									success = 0;
								}
							} else {
								int		old_line = pos->line;

								success = define_macro (bcpp, &tokens, pos);
								if (success && old_line < pos->line) {
									 success = push_compiled_newline_token (tokenizer, pos->line - old_line);
								}
							}
						} else {
							Error_Message (pos, "invalid argument for 'define' directive, expected identifier");
							success = 0;
						}
					}
				} else if (0 == strcmp (tokens, "undef")) {
					if (is_active) {
						tokens = next_const_token (tokens, pos);
						if (tokens[-1] == Token_identifier) {
							struct macro_desc	*macro;

							macro = find_macro (bcpp->macros, tokens);
							if (macro) {
								macro->is_undef = 1;
							}
						} else {
							Error_Message (pos, "invalid argument for 'undef' directive, expected identifier");
							success = 0;
						}
					}
				} else if (0 == strcmp (tokens, "if")) {
					if (ifs_level + 1 < (int) Array_Count (ifs)) {
						ifs_level += 1;
						memset (ifs + ifs_level, 0, sizeof ifs[0]);
						ifs[ifs_level].prev_active = is_active;
						if (is_active) {
							isize	ret = 0;

							tokens = next_const_token (tokens, pos);
							success = expand_and_evaluate_expression (bcpp, tokens, &ret, pos);
							is_active = !!ret;
							if (is_active) {
								ifs[ifs_level].is_already_selected = 1;
							}
						}
					} else {
						System_Error_Message (pos, "nested ifs limit is reached");
						success = 0;
					}
				} else if (0 == strcmp (tokens, "elif")) {
					if (is_active || (ifs_level >= 0 && ifs[ifs_level].prev_active)) {
						if (ifs_level >= 0) {
							if (!ifs[ifs_level].is_already_selected) {
								isize	ret = 0;

								tokens = next_const_token (tokens, pos);
								success = expand_and_evaluate_expression (bcpp, tokens, &ret, pos);
								is_active = !!ret;
								if (is_active) {
									ifs[ifs_level].is_already_selected = 1;
								}
							} else {
								is_active = 0;
							}
						} else {
							Error_Message (pos, "'elif' directive without 'if', 'ifdef' or 'ifndef'");
							success = 0;
						}
					}
				} else if (0 == strcmp (tokens, "ifdef")) {
					if (ifs_level + 1 < (int) Array_Count (ifs)) {
						ifs_level += 1;
						memset (ifs + ifs_level, 0, sizeof ifs[0]);
						ifs[ifs_level].prev_active = is_active;
						if (is_active) {
							tokens = next_const_token (tokens, pos);
							if (tokens[-1] == Token_identifier) {
								is_active = !!find_macro (bcpp->macros, tokens);
								if (is_active) {
									ifs[ifs_level].is_already_selected = 1;
								}
							} else {
								Error_Message (pos, "invalid macro name in 'ifdef' directive");
								success = 0;
							}
						}
					} else {
						System_Error_Message (pos, "nested ifs limit is reached");
						success = 0;
					}
				} else if (0 == strcmp (tokens, "ifndef")) {
					if (ifs_level + 1 < (int) Array_Count (ifs)) {
						ifs_level += 1;
						memset (ifs + ifs_level, 0, sizeof ifs[0]);
						ifs[ifs_level].prev_active = is_active;
						if (is_active) {
							tokens = next_const_token (tokens, pos);
							if (tokens[-1] == Token_identifier) {
								is_active = !find_macro (bcpp->macros, tokens);
								if (is_active) {
									ifs[ifs_level].is_already_selected = 1;
								}
							} else {
								Error_Message (pos, "invalid macro name in 'ifndef' directive");
								success = 0;
							}
						}
					} else {
						System_Error_Message (pos, "nested ifs limit is reached");
						success = 0;
					}
				} else if (0 == strcmp (tokens, "else")) {
					if (is_active || (ifs_level >= 0 && ifs[ifs_level].prev_active)) {
						if (ifs_level >= 0) {
							if (!ifs[ifs_level].is_already_selected) {
								is_active = !is_active;
								if (is_active) {
									ifs[ifs_level].is_already_selected = 1;
								}
							} else {
								is_active = 0;
							}
						} else {
							Error_Message (pos, "'else' directive without 'if', 'ifdef' or 'ifndef'");
							success = 0;
						}
					}
				} else if (0 == strcmp (tokens, "endif")) {
					if (ifs_level >= 0) {
						is_active = ifs[ifs_level].prev_active;
						ifs_level -= 1;
					} else {
						Error_Message (pos, "'endif' directive without 'if', 'ifdef' or 'ifndef'");
						success = 0;
					}
				} else if (0 == strcmp (tokens, "warning")) {
					if (is_active) {
						char	prefix[256];

						snprintf (prefix, sizeof prefix, "%s:%d:%d: warning:", pos->filename, pos->line, pos->column);
						print_tokens_until (next_const_token (tokens, 0), 0, prefix, Token_newline, stderr);
					}
				} else if (0 == strcmp (tokens, "error")) {
					if (is_active) {
						char	prefix[256];

						snprintf (prefix, sizeof prefix, "%s:%d:%d: error:", pos->filename, pos->line, pos->column);
						print_tokens_until (next_const_token (tokens, 0), 0, prefix, Token_newline, stderr);
						success = 0;
					}
				} else if (0 == strcmp (tokens, "pragma")) {
					if (is_active) {
						tokens = next_const_token (tokens, pos);
						if (tokens[-1] == Token_identifier && 0 == strcmp (tokens, "once")) {
							success = revert_token (&bcpp->do_not_include_those);
							success = success && push_token (&bcpp->do_not_include_those, 0, Token_string, pos->filename, strlen (pos->filename));
							success = success && end_tokenizer (&bcpp->do_not_include_those);
						} else {
							success = push_token (tokenizer, 0, Token_punctuator, "#", 1);
							success = success && push_token (tokenizer, 0, Token_punctuator, "pragma", 6);
							while (success && tokens[-1] && tokens[-1] != Token_newline) {
								success = copy_token (tokenizer, tokens);
								tokens = next_const_token (tokens, pos);
							}
						}
					}
				} else {
					Error_Message (pos, "invalid preprocessing directive '%s'", tokens);
					success = 0;
				}
				if (success) {
					while (tokens[-1] && tokens[-1] != Token_newline) {
						tokens = next_const_token (tokens, pos);
					}
					next = tokens;
				}
			} else {
				Error_Message (pos, "invalid preprocessing directive");
				success = 0;
			}
			tokens = next;
		} else {
			if (is_active) {
				success = evaluate_token (bcpp, tokenizer, &tokens, pos, 1);
			} else {
				if (tokens[-1] == Token_newline) {
					copy_token (tokenizer, tokens);
				}
				tokens = next_const_token (tokens, pos);
			}
		}
	}
	return (success);
}

int		parse_bcpp_include_paths (struct bcpp *bcpp, const char *source) {
	int		success = 1;
	usize	include_paths_size = 0;
	usize	include_paths_cap = 0;
	usize	include_count = 0;

	while (*source && success) {
		const char	*ptr;

		while (isspace (*source)) {
			source += 1;
		}
		ptr = source;
		while (*ptr && *ptr != '\n') {
			ptr += 1;
		}
		if (include_paths_size + (ptr - source + 1) + 1 > include_paths_cap) {
			void	*memory;

			memory = expand_array ((void *) bcpp->include_paths, &include_paths_cap);
			if (memory) {
				bcpp->include_paths = memory;
			} else {
				success = 0;
			}
		}
		if (success) {
			if (ptr - source > 0) {
				char	*start;

				start = (char *) bcpp->include_paths + include_paths_size;
				memcpy (start, source, ptr - source);
				include_paths_size += ptr - source;
				((char *) bcpp->include_paths)[include_paths_size] = 0;
				include_paths_size += 1;
				include_count += 1;
				if (strstr (start, "(framework directory)")) {
					include_paths_size -= 1;
					include_paths_size -= ptr - source;
					include_count -= 1;
				}
				source = ptr;
			} else {
				source = ptr;
			}
		}
	}
	if (!success) {
		free ((void *) bcpp->include_paths);
		bcpp->include_paths = 0;
		include_paths_size = 0;
		include_paths_cap = 0;
	}
	if (success && bcpp->include_paths) {
		const char	*includes;

		((char *) bcpp->include_paths)[include_paths_size] = 0;
		include_paths_size += 1;
		bcpp->include_paths_sorted = malloc ((include_count + 1) * sizeof *bcpp->include_paths_sorted);
		bcpp->include_paths_sorted[include_count] = 0;
		includes = bcpp->include_paths;
		while (*includes && include_count > 0) {
			include_count -= 1;
			bcpp->include_paths_sorted[include_count] = includes;
			includes += strlen (includes) + 1;
		}
	}
	return (success);
}

int		init_bcpp (struct bcpp *bcpp, int args_count, char *args[], char *env[]) {
	int			success;
	const char	*whereis_location;

	memset (bcpp, 0, sizeof *bcpp);
	if (check_file_access ("/bin/whereis", Access_Mode_execute)) {
		whereis_location = "/bin/whereis";
		success = 1;
	} else if (check_file_access ("/usr/bin/whereis", Access_Mode_execute)) {
		whereis_location = "/usr/bin/whereis";
		success = 1;
	} else {
		Error ("no whereis found");
		success = 0;
	}
	if (success) {
		char	*output;

		output = read_output_of_program (whereis_location, 1, 2, (char *[]) { (char *) whereis_location, "cc", 0 }, env);
		if (output) {
			char	*path = output;

			while (*path && *path != '\n') {
				path += 1;
			}
			*path = 0;
			path = output;
			output = read_output_of_program (path, 2, 5, (char *[]) { path, "-v", "-xc", "-E", "-", 0 }, env);
			if (output) {
				char	*ptr;

				#define Start_Line "#include <...> search starts here:"
				#define End_Line "End of search list."
				if ((ptr = strstr (output, Start_Line))) {
					char	*start_line = ptr + sizeof Start_Line;

					if ((ptr = strstr (start_line, End_Line))) {
						char	*end_line = ptr;

						*end_line = 0;
						success = parse_bcpp_include_paths (bcpp, start_line);
					} else {
						Error ("no end of include paths in `cc -v -xc -` output");
						success = 0;
					}
				} else {
					Error ("no include paths in `cc -v -xc -` output");
					success = 0;
				}
				#undef Start_Line
				#undef End_Line
				free (output);
				output = read_output_of_program (path, 1, 4, (char *[]) { path, "-dM", "-E", "-", 0 }, env);
				if (output) {
					bcpp->predefined = output;
					bcpp->predefined_size = strlen (bcpp->predefined);
				} else {
					success = 0;
				}
				end_tokenizer (&bcpp->do_not_include_those);
				free (path);
			} else {
				free (path);
				success = 0;
			}
		} else {
			Error ("whereis execution failed");
			success = 0;
		}
	}
	return (success);
}


// #"hello"

// "\
"

void	test_bcpp(struct bcpp *bcpp, const char *filename) {
	char	*preprocessed;

	preprocessed = make_translation_unit (bcpp, filename);
	if (preprocessed) {
		// print_tokens (preprocessed, 1, "output|");
		print_tokens (preprocessed, 0, "", stdout);
		// print_macro_list (bcpp->macros);
		free_tokens (preprocessed);
		print_macro_list (bcpp->macros);
	} else {
		// Error ("no preprocessed");
	}
}

