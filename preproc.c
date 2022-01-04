

static int	g_index = -1;
static int	g_indicies[64] = {0};

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

enum {
	Macro_Flag_variadic = 0x1,
	Macro_Flag_multiline = 0x2,
	Macro_Flag_undef = 0x4,
	Macro_Flag_builtin = 0x8,
	Macro_Flag_disable_directives = 0x10,
	Macro_Flag_no_recursion_check = 0x20,

	Macro_Arg_Flag_no_expand = 0x1,
};

struct macro_desc {
	const char	*ident;
	const char	*args;
	const char	*body;
	usize		args_count;
	int			flags;
	struct position	pos;
};

enum {
	Content_Start,
	Content_End,
	Content_Deps,
	Content_Newlines,
	Content_Count,
};

struct filecache {
	struct tokenizer	filenames;
	const char			*(*contents)[Content_Count];
	usize				contents_count;
	usize				contents_cap;
	char				*deps;
	usize				deps_size;
	usize				deps_cap;
};

struct bcpp {
	const char			**include_paths_sorted;
	struct tokenizer	include_paths;
	struct tokenizer	do_not_include_those;
	struct tokenizer	userincludes;
	struct filecache	filecache;
	char				*predefined;
	usize				predefined_size;
	struct macro_desc	*macros;
	struct macro_desc	*macros_data;
	usize				macros_size;
	usize				macros_cap;
	struct tokenizer	macro_tokenizer;
	struct macro_stack	macro_stack;
	int					paste_column;
	int					was_multiline;
	usize				frameworks_count;
	const char			*frameworks[128];
};

#include "expr.c"

const char	*get_next_file_dep (struct filecache *cache, const char *dep);

int		add_file_dep (struct filecache *cache, int index, const char *filename, usize size) {
	int		success;

	if ((cache->deps_size + sizeof (void *) + size + 1) >= cache->deps_cap) {
		void	*memory;

		memory = expand_array (cache->deps, &cache->deps_cap);
		if (memory) {
			cache->deps = memory;
			Assert ((cache->deps_size + sizeof (void *) + size + 1) < cache->deps_cap);
			success = 1;
		} else {
			Error ("cannot allocate memory for file dependency tracker");
			success = 0;
		}
	} else {
		success = 1;
	}
	if (success) {
		usize	old = (usize) cache->contents[index][Content_Deps];

		*(usize *) (cache->deps + cache->deps_size) = old;
		cache->deps_size += sizeof (usize);
		cache->contents[index][Content_Deps] = (void *) cache->deps_size;
		memcpy (cache->deps + cache->deps_size, filename, size);
		cache->deps[cache->deps_size + size] = 0;
		cache->deps_size += size + 1;
	}
	return (success);
}

const char	*get_file_dep (struct filecache *cache, int index) {
	const char	*result;

	if (cache->contents[index][Content_Deps]) {
		result = cache->deps + (usize) cache->contents[index][Content_Deps];
	} else {
		result = 0;
	}
	return (result);
}

const char	*get_next_file_dep (struct filecache *cache, const char *dep) {
	const char	*result;
	usize		next;

	next = *(usize *) (dep - sizeof (usize));
	if (next) {
		result = cache->deps + next;
	} else {
		result = 0;
	}
	return (result);
}

int		add_file_to_cache (struct filecache *cache, const char **pfilename, char *content, usize size, int *pindex) {
	int		success;

	if ((cache->contents_count + 1) * sizeof *cache->contents >= cache->contents_cap) {
		void	*memory;

		memory = expand_array (cache->contents, &cache->contents_cap);
		if (memory) {
			cache->contents = memory;
			success = 1;
		} else {
			Error ("cannot allocate memory for filecache");
			success = 0;
		}
	} else {
		success = 1;
	}
	if (success) {
		int		*nl_array;

		size = preprocess_text (content, content + size, &nl_array);
		cache->contents[cache->contents_count][Content_Start] = content;
		cache->contents[cache->contents_count][Content_End] = content + size;
		cache->contents[cache->contents_count][Content_Deps] = 0;
		cache->contents[cache->contents_count][Content_Newlines] = (const char *) nl_array;
		cache->contents_count += 1;
		cache->contents[cache->contents_count][Content_Start] = 0;
		cache->contents[cache->contents_count][Content_End] = 0;
		cache->contents[cache->contents_count][Content_Deps] = 0;
		cache->contents[cache->contents_count][Content_Newlines] = 0;
		if (cache->filenames.current) {
			success = revert_token (&cache->filenames);
		}
		success = success && push_string_token (&cache->filenames, cache->contents_count - 1, *pfilename, strlen (*pfilename), 0);
		*pfilename = cache->filenames.current;
		success = success && end_tokenizer (&cache->filenames, 0);
		if (success) {
			*pindex = cache->contents_count - 1;
		}
	}
	return (success);
}

int		get_file_cache_index (struct filecache *cache, const char **pfilename) {
	const char	*token;
	int			index;

	token = get_first_token (&cache->filenames);
	if (token) while (token[-1] && 0 != strcmp (token, *pfilename)) {
		token = next_const_token (token, 0);
	}
	if (token && token[-1]) {
		index = get_token_offset (token);
		*pfilename = token;
	} else {
		index = -1;
	}
	return (index);
}

int		evaluate_directives (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *tokens, struct position *pos, int is_top_level);

int		push_line_directive (struct tokenizer *tokenizer, const char *path, int line);

int		add_content_to_translation_unit (struct bcpp *bcpp, struct tokenizer *tokenizer, char *content, usize size, int *newline_array, const char *filename) {
	char	*tokens;
	int		success;

	tokens = tokenize (content, newline_array, 1, filename);
	if (tokens) {
		success = push_line_directive (tokenizer, filename, !g_no_line_directives);
		if (success) {
			struct position		cpos = {
				.filename = filename,
				.line = 1,
				.column = 1,
				.at_the_beginning = 1,
			}, *pos = &cpos;

			success = evaluate_directives (bcpp, tokenizer, tokens, pos, 1);
			free_tokens (tokens);
			tokens = 0;
		}
	} else {
		Error ("no tokens");
		success = 0;
	}
	return (success);
}

char	*load_file_content (struct filecache *cache, const char **pfilename, usize *psize, int **nl_array) {
	char	*content;
	int		cache_index;

	cache_index = get_file_cache_index (cache, pfilename);
	if (cache_index >= 0) {
		content = (char *) cache->contents[cache_index][Content_Start];
		if (psize) {
			*psize = cache->contents[cache_index][Content_End] - content;
		}
		if (nl_array) {
			*nl_array = (int *) cache->contents[cache_index][Content_Newlines];
		}
	} else {
		usize	size;

		content = read_entire_file (*pfilename, &size);
		if (content) {
			if (add_file_to_cache (cache, pfilename, content, size, &cache_index)) {
				if (psize) {
					*psize = cache->contents[cache_index][Content_End] - content;
				}
				if (nl_array) {
					*nl_array = (int *) cache->contents[cache_index][Content_Newlines];
				}
			} else {
				free (content);
				content = 0;
			}
		}
	}
	return (content);
}

int		add_file_to_translation_unit (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *filename) {
	usize	size;
	char	*content;
	int		success;
	int		*nl_array;

	content = load_file_content (&bcpp->filecache, &filename, &size, &nl_array);
	if (content) {
		success = add_content_to_translation_unit (bcpp, tokenizer, content, size, nl_array, filename);
	} else {
		success = 0;
	}
	return (success);
}

void	print_macro_list (struct macro_desc *macros);

#define Builtin_Macros \
"#define _Foreach(body, ...) body\n"

static int g_is_builtin_macro = 0;

char	*make_translation_unit (struct bcpp *bcpp, const char *filename) {
	int		success;
	struct tokenizer	ctokenizer = {0}, *tokenizer = &ctokenizer;
	char	builtin_macros[] = Builtin_Macros;
	usize	builtin_macros_size = sizeof (Builtin_Macros);

	if (bcpp->predefined) {
		success = add_content_to_translation_unit (bcpp, tokenizer, bcpp->predefined, bcpp->predefined_size, (int []) {0}, "<predefined>");
	} else {
		Debug ("no predefined macros");
		success = 1;
	}
	if (success) {
		g_is_builtin_macro = 1;
		success = add_content_to_translation_unit (bcpp, tokenizer, builtin_macros, builtin_macros_size, (int []) {0}, "<builtin>");
		g_is_builtin_macro = 0;
		if (success) {
			if (!add_file_to_translation_unit (bcpp, tokenizer, filename)) {
				Error ("cannot add file to the translation unit of '%s'", filename);
				free_tokenizer (tokenizer);
				success = 0;
			} else if (!end_tokenizer (tokenizer, 0)) {
				Error ("cannot end tokenizer");
				free_tokenizer (tokenizer);
				success = 0;
			} else {
				success = 1;
			}
		} else {
			free_tokenizer (tokenizer);
		}
	} else {
		free_tokenizer (tokenizer);
	}
	return (get_first_token (tokenizer));
}

int		push_line_directive (struct tokenizer *tokenizer, const char *path, int line) {
	int		success;
	char	numbuf[32];
	int		numbuf_length;
	usize	path_length = strlen (path);

	numbuf_length = snprintf (numbuf, sizeof numbuf, "%d", line);
	if (tokenizer->current && tokenizer->current[-1] != Token_newline) {
		success = push_newline_token (tokenizer, 0);
	} else {
		success = 1;
	}
	success = success && push_token (tokenizer, 0, Token_punctuator, "#", 1);
	success = success && push_token (tokenizer, 0, Token_identifier, "line", 4);
	success = success && push_token (tokenizer, 1, Token_preprocessing_number, numbuf, numbuf_length);
	success = success && push_string_token (tokenizer, 1, path, path_length, 0);
	success = success && push_newline_token (tokenizer, 0);
	return (success);
}

int		is_file_already_included (struct bcpp *bcpp, const char *path) {
	int			result = 0;
	const char	*tokens;

	tokens = get_first_token (&bcpp->do_not_include_those);
	while (!result && tokens[-1]) {
		result = (0 == strcmp (tokens, path));
		tokens = next_const_token (tokens, 0);
	}
	return (result);
}

#define Is_Framework_Includepath_Flag 0x1

int		is_framework_includepath (const char *includepath) {
	return (get_token_offset (includepath) & Is_Framework_Includepath_Flag);
}

int		include_file_global (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *filename, struct position *pos, int remember_file) {
	int			success;
	const char	**includes;
	char		framework[512], path[512];

	framework[0] = 0;
	if (bcpp->include_paths.current) {
		int		found = 0;
		char	*revert_framework = 0;
		int		should_revert_framework = 0;

		includes = bcpp->include_paths_sorted;
		while (*includes) {
			usize	length = get_token_length (*includes);

			strcpy (path, *includes);
			if (path[length - 1] != '/') {
				path[length] = '/';
				length += 1;
			}
			if (is_framework_includepath (*includes)) {
				const char	*ptr, *sptr;

				success = 1;
				ptr = strchr (filename, '/');
				if (ptr) {
					length = stpncpy (path + length, filename, ptr - filename) - path;
					length = stpcpy (path + length, ".framework") - path;
					sptr = path + length;
					path[length] = 0;
					if (check_file_access (path, Access_Mode_read)) {
						length = stpcpy (path + length, "/Headers/") - path;
						length = stpcpy (path + length, ptr + 1) - path;
						path[length] = 0;
						found = check_file_access (path, Access_Mode_read);
						if (found) {
							usize	length = 0;

							length = stpncpy (framework + length, path, sptr - path) - framework;
							length = stpcpy (framework + length, "/Frameworks/") - framework;
							Debug ("Trying to find Frameworks file: %s", framework);
							framework[length] = 0;
							if (check_file_access (framework, Access_Mode_read)) {
								if (bcpp->frameworks_count < Array_Count (bcpp->frameworks)) {
									bcpp->frameworks[bcpp->frameworks_count] = framework;
									Debug ("Framework folder added %zu: %s", bcpp->frameworks_count, bcpp->frameworks[bcpp->frameworks_count]);
									bcpp->frameworks_count += 1;
								} else {
									Error_Message (pos, "frameworks array overflow");
									success = 0;
								}
							} else {
								framework[0] = 0;
							}
						}
					}
				}
			} else {
				length = stpcpy (path + length, filename) - path;
				path[length] = 0;
				found = check_file_access (path, Access_Mode_read);
				success = 1;
			}
			if (success && found) {
				break ;
			}
			includes += 1;
		}
		Debug ("found: %d; frameworks_count: %zu", found, bcpp->frameworks_count);
		if (success && !found && bcpp->frameworks_count > 0) {
			usize	length;
			const char	*ptr, *sptr;
			usize	index = 0;

			ptr = strchr (filename, '/');
			if (ptr) {
				while (index < bcpp->frameworks_count) {
					length = stpcpy (path, bcpp->frameworks[index]) - path;
					length = stpncpy (path + length, filename, ptr - filename) - path;
					length = stpcpy (path + length, ".framework") - path;
					sptr = path + length;
					if (check_file_access (path, Access_Mode_read)) {
						length = stpcpy (path + length, "/Headers/") - path;
						length = stpcpy (path + length, ptr + 1) - path;
						found = check_file_access (path, Access_Mode_read);
						if (found) {
							usize	length = 0;

							length = stpncpy (framework + length, path, sptr - path) - framework;
							length = stpcpy (framework + length, "/Frameworks/") - framework;
							if (check_file_access (framework, Access_Mode_read)) {
								if (bcpp->frameworks_count < Array_Count (bcpp->frameworks)) {
									bcpp->frameworks[bcpp->frameworks_count] = framework;
									bcpp->frameworks_count += 1;
								} else {
									Error_Message (pos, "frameworks array overflow");
									success = 0;
								}
							} else {
								framework[0] = 0;
							}
						}
					}
					index += 1;
				}
			}
		}
		if (success && !found) {
			const char	*include;

			include = get_first_token (&bcpp->userincludes);
			if (include) while (success && include[-1]) {
				usize	length = get_token_length (include);

				strcpy (path, include);
				if (path[length - 1] != '/') {
					path[length] = '/';
					length += 1;
				}
				length = stpcpy (path + length, filename) - path;
				path[length] = 0;
				found = check_file_access (path, Access_Mode_read);
				success = 1;
				if (success && found) {
					break ;
				}
				include = next_const_token (include, 0);
			}
		}
		if (found) {
			if (!is_file_already_included (bcpp, path)) {
				if (remember_file) {
					success = revert_token (&bcpp->do_not_include_those);
					success = success && push_string_token (&bcpp->do_not_include_those, get_token_offset (*includes), path, strlen (path), 0);
					success = success && end_tokenizer (&bcpp->do_not_include_those, 0);
				} else {
					success = 1;
				}
				if (success) {
					success = add_file_to_translation_unit (bcpp, tokenizer, path);
					if (success) {
						int		cache_index;
						const char	*filename = pos->filename;

						cache_index = get_file_cache_index (&bcpp->filecache, &filename);
						if (cache_index >= 0) {
							success = add_file_dep (&bcpp->filecache, cache_index, path, strlen (path));
						}
					}
				}
			} else {
				success = 1;
			}
			if (success && framework[0]) {
				bcpp->frameworks_count -= 1;
			}
		} else {
			Error_Message (pos, "file not found");
			success = 0;
		}
	} else {
		Error_Message (pos, "file not found");
		success = 0;
	}
	return (success);
}

int		include_file_relative (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *filename, const char *relative_from, struct position *pos, int remember_file) {
	int		success;
	char	*ptr;
	char	path[512];

	if ((ptr = strrchr (relative_from, '/'))) {
		memcpy (path, relative_from, ptr - relative_from + 1);
		ptr = path + (ptr - relative_from);
		while (0 == strncmp (filename, "../", 3) && ptr > path) {
			char	*nptr;

			*ptr = 0;
			nptr = strrchr (path, '/');
			if (nptr) {
				nptr[1] = 0;
				ptr = nptr;
			} else {
				ptr = path;
			}
			filename += 3;
		}
		if (ptr > path) {
			strcpy (ptr + 1, filename);
		} else {
			strcpy (path, filename);
		}
	} else {
		strcpy (path, filename);
	}
	if (check_file_access (path, Access_Mode_read)) {
		if (!is_file_already_included (bcpp, path)) {
			if (remember_file) {
				success = revert_token (&bcpp->do_not_include_those);
				success = success && push_string_token (&bcpp->do_not_include_those, 0, path, strlen (path), 0);
				success = success && end_tokenizer (&bcpp->do_not_include_those, 0);
			} else {
				success = 1;
			}
			if (success) {
				success = add_file_to_translation_unit (bcpp, tokenizer, path);
				if (success) {
					int		cache_index;
					const char	*filename = relative_from;

					cache_index = get_file_cache_index (&bcpp->filecache, &filename);
					if (cache_index >= 0) {
						success = add_file_dep (&bcpp->filecache, cache_index, path, strlen (path));
					}
				}
			}
		} else {
			success = 1;
		}
	} else {
		success = include_file_global (bcpp, tokenizer, filename, pos, remember_file);
	}
	return (success);
}

int		is_function_like_macro (const char *macro) {
	return (macro[-1] == Token_punctuator && 0 == strcmp (macro, "(") && 0 == get_token_offset (macro));
}

int		is_function_like_macro_call (const char *macro) {
	while (macro[-1] && macro[-1] == Token_newline) {
		macro = next_const_token (macro, 0);
	}
	return (macro[-1] == Token_punctuator && 0 == strcmp (macro, "("));
}

#define Iterate_Macro(macro) ((macro)->ident || (((macro) = (macro) + 1)->ident && ((macro) = (struct macro_desc *) (macro)->ident + 1)->ident))

struct macro_desc	*find_macro (struct macro_desc *macros, const char *identifier) {
	struct macro_desc	*macro;

	if (macros) {
		macro = 0;
		while (Iterate_Macro (macros)) {
			if (!(macros->flags & Macro_Flag_undef) && 0 == strcmp (macros->ident, identifier)) {
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
		print_tokens_until (macros->ident, 1, "", Token_eof, stderr);
		macros += 1;
	}
}

struct macro_args {
	int			is_variadic;
	usize		size;
	const char	*tokens[32][3]; /* 0 - macro, 1 - macro call expanded, 2 - macro call */
	int			flags[32];
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
		args->flags[args->size] = 0;
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

int		find_macro_arg (struct macro_args *args, const char *identifier, usize *index, const struct position *pos) {
	int		success;
	usize	arg_index = 0;

	if (0 == strcmp (identifier, "__VA_ARGS__")) {
		if (args->size > 0 && 0 == strcmp (args->tokens[args->size - 1][Macro_Arg], "...")) {
			arg_index = args->size - 1;
			success = 1;
		} else {
			Error_Message (pos, "'__VA_ARGS__' is forbidden in non-variadic or named variadic macro bodies");
			arg_index = args->size;
			success = 0;
		}
	} else {
		while (arg_index < args->size && 0 != strcmp (identifier, args->tokens[arg_index][Macro_Arg])) {
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
		int			arg_flags = 0;

		args->is_variadic = !!(desc->flags & Macro_Flag_variadic);
		success = 1;
		if (arg[-1] == Token_punctuator && 0 == strcmp (arg, ")")) {
		} else while (success && arg[-1]) {
			if (arg[-1] == Token_identifier || ((desc->flags & Macro_Flag_variadic) && arg[-1] == Token_punctuator && 0 == strcmp (arg, "..."))) {
				Debug ("push arg '%s'", arg);
				success = push_macro_arg (args, arg);
				if (arg_flags) {
					Debug ("FLAG!!!!!!!!!!!!!!!!!!!!!!!!!!");
				}
				args->flags[args->size - 1] = arg_flags;
				arg_flags = 0;
				arg = next_const_token (arg, 0);
				if (success && (desc->flags & Macro_Flag_variadic) && arg[-1] == Token_punctuator && (0 == strcmp (arg, "..."))) {
					if (args->tokens[args->size - 1][Macro_Arg][-1] != Token_punctuator) {
						arg = next_const_token (arg, 0);
					} else {
						Error_Message (&desc->pos, "invalid argument separator");
						success = 0;
					}
				}
				if (success) {
					if (arg[-1] == Token_punctuator && (0 == strcmp (arg, ",") || 0 == strcmp (arg, ")"))) {
						if (arg[0] == ')') {
							break ;
						}
					}
				} else {
					Error_Message (&desc->pos, "invalid argument separator");
					success = 0;
				}
			} else if (arg[-1] == Token (punctuator) && 0 == strcmp (arg, "!")) {
				arg_flags |= Macro_Arg_Flag_no_expand;
			} else {
				Error_Message (&desc->pos, "invalid argument token");
				Assert (0);
				success = 0;
			}
			arg = next_const_token (arg, 0);
		}
	} else {
		success = 1;
	}
	return (success);
}

int		evaluate_token (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **ptokens, struct position *pos, int check_macro_stack, int is_top_level);

int		evaluate_macro_call (struct bcpp *bcpp, struct tokenizer *tokenizer, struct macro_desc *desc, struct macro_args *args, const char **ptokens, struct position *pos) {
	const char	*tokens = *ptokens, *original = tokens;
	int			success;
	const char	*ptr;
	usize		group_level = 0, arg_index = 0;
	int			is_part_of_va;
	char		*begin;

	Assert (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "("));
	success = 1;
	is_part_of_va = ((desc->flags & Macro_Flag_variadic) && arg_index + 1 == args->size);
	tokens = next_const_token (tokens, pos);
	begin = tokenizer->current;
	while (success && tokens[-1]) {
		if (group_level <= 0 && tokens[-1] == Token_punctuator && (0 == strcmp (tokens, ")") || (!is_part_of_va && 0 == strcmp (tokens, ",")))) {
			success = end_tokenizer (tokenizer, 0);
			if (success) {
				begin = get_next_from_tokenizer (tokenizer, begin);
				if (arg_index < args->size) {
					success = set_macro_arg (args, begin, arg_index, Macro_Call_Arg);
					arg_index += 1;
				} else if (!(args->size == 0 && arg_index == 0 && next_const_token (original, 0) == tokens)) {
					Error_Message (pos, "invalid number of arguments");
					success = 0;
				}
				begin = tokenizer->current;
				is_part_of_va = ((desc->flags & Macro_Flag_variadic) && arg_index + 1 == args->size);
				if (tokens[0] == ',') {
					tokens = next_const_token (tokens, pos);
					continue ;
				}
			}
			if (!success || tokens[0] == ')') {
				break ;
			}
		}
		if (tokens[-1] == Token_newline) {
			tokens = next_const_token (tokens, pos);
		} else {
			if (tokens[-1] == Token_punctuator) {
				group_level += (0 == strcmp (tokens, "("));
				group_level -= (0 == strcmp (tokens, ")"));
			}
			success = copy_token (tokenizer, tokens);
			tokens = next_const_token (tokens, pos);
		}
	}
	if (success) {
		if (tokens[-1]) {
			usize	index;

			*ptokens = next_const_token (tokens, pos);
			index = 0;
			while (success && index < args->size) {
				const char	*arg;
				char		*begin;

				begin = tokenizer->current;
				arg = args->tokens[index][Macro_Call_Arg];
				if (arg == 0) {
					if ((desc->flags & Macro_Flag_variadic) && index == args->size - 1) {
						success = end_tokenizer (tokenizer, 0);
						args->tokens[index][Macro_Call_Arg] = tokenizer->current;
						args->tokens[index][Macro_Expanded_Arg] = tokenizer->current;
						break ;
					} else {
						Error_Message (pos, "invalid number of arguments. Expected %zu, got %zu", args->size, arg_index);
						success = 0;
					}
				} else if (args->flags[index] & Macro_Arg_Flag_no_expand) {
					success = set_macro_arg (args, args->tokens[index][Macro_Call_Arg], index, Macro_Expanded_Arg);
				} else {
					struct position	pos = { .filename = "<macro>", .line = 1, .column = 1, };

					while (success && arg[-1]) {
						success = evaluate_token (bcpp, tokenizer, &arg, &pos, 0, 0);
					}
					success = success && end_tokenizer (tokenizer, 0);
					if (success) {
						begin = get_next_from_tokenizer (tokenizer, begin);
						success = set_macro_arg (args, begin, index, Macro_Expanded_Arg);
					}
				}
				index += 1;
			}
		} else {
			Error_Message (pos, "invalid function-like macro call");
			Debug_Code (print_tokens_until (original, 0, "orig|", Token_newline, stderr));
			success = 0;
		}
	}
	return (success);
}

int		define_macro_body (struct macro_desc *desc, struct tokenizer *tokenizer, const char **ptokens, struct position *pos, int is_ws_macro_call) {
	int			success;
	const char	*tokens = *ptokens;

	if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "(") && (is_ws_macro_call || 0 == get_token_offset (tokens))) {
		success = copy_token (tokenizer, tokens);
		desc->args = tokenizer->current;
		tokens = next_const_token (tokens, pos);
		success = 1;
		if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")")) {
			desc->args_count = 0;
			success = copy_token (tokenizer, tokens);
		} else while (success && tokens[-1] && tokens[-1] != Token_newline) {
			if (tokens[-1] == Token_identifier) {
				if ((success = copy_token (tokenizer, tokens))) {
					desc->args_count += 1;
					tokens = next_const_token (tokens, pos);
					if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "...")) {
						desc->flags |= Macro_Flag_variadic;
						success = copy_token (tokenizer, tokens);
						tokens = next_const_token (tokens, pos);
						if (success && !(tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")"))) {
							Error_Message (pos, "'...' token must be last one in macro parameter list");
							success = 0;
						} else {
							success = copy_token (tokenizer, tokens);
							break ;
						}
					}
					if (tokens[-1] == Token_punctuator && (0 == strcmp (tokens, ",") || 0 == strcmp (tokens, ")"))) {
						success = copy_token (tokenizer, tokens);
						if (tokens[0] == ',') {
							tokens = next_const_token (tokens, pos);
						} else {
							break ;
						}
					} else {
						Error_Message (pos, "invalid macro parameter separator");
						success = 0;
					}
				}
			} else if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "...")) {
				if ((success = copy_token (tokenizer, tokens))) {
					desc->flags |= Macro_Flag_variadic;
					tokens = next_const_token (tokens, pos);
					success = copy_token (tokenizer, tokens);
					if (success && !(tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")"))) {
						Error_Message (pos, "'...' token must be last one in macro parameter list");
						success = 0;
					} else {
						break ;
					}
				}
			} else if (tokens[-1] == Token (punctuator) && 0 == strcmp (tokens, "!")) {
				success = copy_token (tokenizer, tokens);
				tokens = next_const_token (tokens, pos);
			} else {
				Error_Message (pos, "invalid macro parameter name '%s'", tokens);
				success = 0;
			}
		}
		if (success && tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")")) {
			tokens = next_const_token (tokens, pos);
		} else {
			Error_Message (pos, "invalid macro parameter list");
			success = 0;
		}
	} else {
		success = 1;
	}
	if (success) {
		char	*begin = tokenizer->current;

		desc->pos = *pos;
		if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "/") && 1 == get_token_offset (tokens)) {
			desc->flags |= Macro_Flag_multiline;
			success = copy_token (tokenizer, tokens);
			if (success) {
				const char	*end_tokens;

				do {
					tokens = next_const_token (tokens, pos);
					if (tokens[-1] == Token (punctuator) && 0 == get_token_offset (tokens)) {
						if (0 == strcmp (tokens, "-")) {
							success = copy_token (tokenizer, tokens);
							desc->flags |= Macro_Flag_disable_directives;
						} else if (0 == strcmp (tokens, "!")) {
							success = copy_token (tokenizer, tokens);
							desc->flags |= Macro_Flag_no_recursion_check;
						} else {
							break ;
						}
					} else {
						break ;
					}
				} while (tokens[-1]);

				success = copy_token (tokenizer, tokens);
				end_tokens = tokens;
				while (success && tokens[-1] && tokens[-1] != Token_newline) {
					tokens = next_const_token (tokens, pos);
					success = copy_token (tokenizer, tokens);
				}
				begin = tokenizer->current;
				tokens = next_const_token (tokens, pos);
				desc->pos = *pos;
				while (success && tokens[-1]) {
					if (pos->at_the_beginning && is_token (tokens, Token_punctuator, "#")) {
						if (is_token (next_const_token (tokens, 0), Token_identifier, "end")) {
							const char	*end, *start = tokens;

							end = end_tokens;
							tokens = next_const_token (next_const_token (tokens, pos), pos);
							while (tokens[-1] && tokens[-1] != Token_newline && tokens[-1] == end[-1] && 0 == strcmp (tokens, end)) {
								tokens = next_const_token (tokens, pos);
								end = next_const_token (end, 0);
							}
							if (tokens[-1] == Token_newline && end[-1] == tokens[-1]) {
								break ;
							} else {
								while (success && start[-1] && start[-1] != Token_newline && start != tokens) {
									success = copy_token (tokenizer, start);
									start = next_const_token (start, 0);
								}
								continue ;
							}
						} else {
							while (tokens[-1] && tokens[-1] != Token_newline) {
								success = copy_token (tokenizer, tokens);
								tokens = next_const_token (tokens, pos);
							}
						}
					} else {
						success = copy_token (tokenizer, tokens);
						tokens = next_const_token (tokens, pos);
					}
				}
			}
		} else {
			while (success && tokens[-1] && tokens[-1] != Token_newline) {
				success = copy_token (tokenizer, tokens);
				tokens = next_const_token (tokens, pos);
			}
		}
		success = end_tokenizer (tokenizer, 0) && success;
		desc->body = get_next_from_tokenizer (tokenizer, begin);
	}
	*ptokens = tokens;
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
			if (g_is_builtin_macro) {
				desc->flags |= Macro_Flag_builtin;
			}
			if (success) {
				tokens = next_const_token (tokens, pos);
				success = define_macro_body (desc, tokenizer, &tokens, pos, 0);
				if (success) {
					Debug_Code (print_tokens_until (desc->ident, 1, "Macro Added", Token_eof, stderr));
				}
			}
		} else {
			Error_Message (pos, "invalid macro identifier");
			success = 0;
		}
	} else {
		Error ("something wrong");
		success = 0;
	}
	*ptokens = tokens;
	return (success);
}

int		are_macros_equivalent (const char *left, const char *right) {
	int		result;
	int		is_function_like;

	/* TODO: support multiline macros */
	Debug_Code (print_tokens_until (left, 1, "DEBUG|", Token_eof, stderr));
	if (0 == strcmp (left, right)) {
		if ((is_function_like = is_function_like_macro (next_const_token (left, 0))) == is_function_like_macro (next_const_token (right, 0))) {
			if (is_function_like) {
				result = 1;
				while (result && !(left[-1] == Token_punctuator && 0 == strcmp (left, ")"))) {
					if (!(left[-1] == right[-1] && 0 == strcmp (left, right))) {
						result = 0;
					}
					left = next_const_token (left, 0);
					right = next_const_token (right, 0);
				}
				if (result && left[-1] == right[-1] && 0 == strcmp (left, right)) {
					left = next_const_token (left, 0);
					right = next_const_token (right, 0);
				} else {
					result = 0;
				}
			} else {
				left = next_const_token (left, 0);
				right = next_const_token (right, 0);
				result = 1;
			}
			if (result) {
				result = (left[-1] == Token_newline || !left[-1]) == (right[-1] == Token_newline || !right[-1]);
				if (result && left[-1] && left[-1] != Token_newline) {
					result = (0 == strcmp (left, right));
					left = next_const_token (left, 0);
					right = next_const_token (right, 0);
					while (result && left[-1] == right[-1] && left[-1] != Token_newline && left[-1]) {
						result = left[-1] == right[-1] && 0 == strcmp (left, right);
						left = next_const_token (left, 0);
						right = next_const_token (right, 0);
					}
					result = (result && (left[-1] == Token_newline || !left[-1]) == (right[-1] == Token_newline || !right[-1]));
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

int		concatenate_macro_token (struct tokenizer *tokenizer, const char *token, struct macro_args *args, const struct position *pos) {
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
	usize				overall_size;
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
		pusher->overall_size += punesc - unesc;
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
		pusher->overall_size += 1;
	}
	return (success);
}

int		stringify_tokens (struct tokenizer *tokenizer, const char *tokens, int count, int offset, int is_ws_preserved, const struct position *pos) {
	int		success = 1;
	int		until_end = (count == 0);
	int		is_nl = 0;
	struct unescape_pusher	pusher = {
		.tokenizer = tokenizer,
		.offset = offset,
	};

	while (success && (until_end || count > 0) && tokens[-1]) {
		while (success && (until_end || count > 0) && tokens[-1] && tokens[-1] != Token_newline) {
			const char	*string;

			if (pusher.size > 0 || pusher.push_at_existing || is_nl) {
				if (is_ws_preserved) {
					int		count = get_token_offset (tokens);

					while (count > 0) {
						success = push_char (&pusher, ' ');
						count -= 1;
					}
				} else {
					if (get_token_offset (tokens) > 0) {
						success = push_char (&pusher, ' ');
					}
				}
				is_nl = 0;
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
		if (tokens[-1] == Token_newline) {
			if (pusher.size > 0) {
				success = push_char(&pusher, '\n');
				if (success) {
					success = push_string_token (tokenizer, offset, pusher.buffer, pusher.size, pusher.push_at_existing);
					pusher.size = 0;
					pusher.push_at_existing = 1;
					is_nl = 1;
				}
			}
			if (success) {
				success = push_newline_token (tokenizer, 0);
				if (success) {
					tokens = next_const_token (tokens, 0);
					offset = 0;
					pusher.push_at_existing = 0;
				}
			}
		}
	}
	if (success && (pusher.size > 0 || pusher.overall_size == 0)) {
		success = push_string_token (tokenizer, offset, pusher.buffer, pusher.size, pusher.push_at_existing);
	}
	return (success);
}

int		evaluate_pasting_concatenation_and_stringization (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **pbody, struct macro_desc *desc, struct macro_args *args, const struct position *logpos) {
	int			success;
	const char	*body = *pbody, *next = 0;
	char		*begin = tokenizer->current;
	struct position	cpos = *logpos, *pos = &cpos;

	if (body[-1] == Token_punctuator && 0 == strcmp (body, "##")) {
		Error_Message (logpos, "'##' cannot appear at the beginning of line");
		success = 0;
	} else {
		success = 1;
	}
	while (success && body[-1] && ((desc->flags & Macro_Flag_multiline) || body[-1] != Token_newline)) {
		int		at_the_beginning = pos->at_the_beginning;

		next = next_const_token (body, pos);
		if (next[-1] == Token_punctuator && 0 == strcmp (next, "##") && next_const_token (next, 0)[-1] && next_const_token (next, 0)[-1] != Token_newline) {
			if ((desc->flags & Macro_Flag_multiline) && body[-1] == Token_newline) {
				Error_Message (logpos, "'##' cannot appear at the beginning of line");
				success = 0;
				break ;
			}
			next = next_const_token (next, pos);
			if (next[-1] && next[-1] != Token_newline) {
				int		is_left_empty = 0;

				if (body[-1] == Token_identifier && args) {
					usize	arg_index;

					if (find_macro_arg (args, body, &arg_index, logpos)) {
						if (arg_index < args->size) {
							const char	*arg = args->tokens[arg_index][Macro_Call_Arg];
							int			group_level = 0;

							is_left_empty = !arg[-1];
							while (success && arg[-1]) {
								success = copy_token (tokenizer, arg);
								arg = next_const_token (arg, 0);
							}
						} else {
							Debug ("HE!!!! [%d;%d;%s]", get_token_length (body), body[-1], body);
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
						if (tokenizer->current[-1] == Token (punctuator) && 0 == strcmp (tokenizer->current, ",") &&
							next[-1] == Token (identifier) && 0 == strcmp (next, "__VA_ARGS__")) {
							if (!args->tokens[args->size - 1][Macro_Call_Arg][-1]) {
								revert_token (tokenizer);
							}
							body = next;
							continue ;
						} else {
							success = concatenate_macro_token (tokenizer, next, args, logpos);
							while (success && is_token (next_const_token (next, 0), Token_punctuator, "##")) {
								next = next_const_token (next, pos);
								next = next_const_token (next, pos);
								success = concatenate_macro_token (tokenizer, next, args, logpos);
							}
							next = next_const_token (next, pos);
						}
					}
					if (success) {
						set_token_offset (tokenizer->current, get_token_offset (body));
					}
				}
			} else {
				Error_Message (logpos, "'##' cannot appear at end of macro expansion");
				success = 0;
			}
		} else if (at_the_beginning && body[-1] == Token_punctuator && 0 == strcmp (body, "#") && 0 == get_token_offset (body)) {
			success = copy_token (tokenizer, body);
		} else if (body[-1] == Token_punctuator && 0 == strcmp (body, "#")) {
			int		offset = get_token_offset (body);

			body = next_const_token (body, 0);
			if (body[-1] && body[-1] != Token_newline) {
				if (args && body[-1] == Token_identifier) {
					usize	arg_index = 0;

					if (find_macro_arg (args, body, &arg_index, logpos)) {
						if (arg_index < args->size) {
							const char	*arg = args->tokens[arg_index][2];

							success = stringify_tokens (tokenizer, arg, 0, offset, 0, logpos);
						} else {
							success = stringify_tokens (tokenizer, body, 1, offset, 0, logpos);
						}
					} else {
						success = 0;
					}
				} else {
					success = stringify_tokens (tokenizer, body, 1, offset, 0, logpos);
				}
				next = next_const_token (next, pos);
			} else {
				Error_Message (logpos, "invalid operand for stringify operator");
				success = 0;
			}
		} else if (body[-1] == Token (identifier) && 0 == strcmp (body, "_Newline")) {
			success = push_newline_token (tokenizer, 0);
		} else if (body[-1] == Token (identifier) && 0 == strcmp (body, "_Hash")) {
			success = push_token (tokenizer, get_token_offset (body), Token (punctuator), "#", 1);
		} else if (body[-1] == Token (identifier) && 0 == strcmp (body, "_Va_Args")) {
			success = push_token (tokenizer, get_token_offset (body), Token (identifier), "__VA_ARGS__", 11);
		} else if (args && body[-1] == Token_identifier) {
			usize		arg_index;

			if (find_macro_arg (args, body, &arg_index, pos)) {
				if (arg_index < args->size) {
					const char	*arg = args->tokens[arg_index][Macro_Expanded_Arg];
					char		*begin = tokenizer->current;

					while (success && arg[-1]) {
						success = copy_token (tokenizer, arg);
						arg = next_const_token (arg, 0);
					}
					if (success) {
						begin = get_next_from_tokenizer (tokenizer, begin);
						if (begin) {
							set_token_offset (begin, get_token_offset (body));
						}
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
		body = next;
	}
	success = success && end_tokenizer (tokenizer, 0);
	if (success) {
		begin = get_next_from_tokenizer (tokenizer, begin);
		*pbody = begin;
	}
	return (success);
}

int		evaluate_defined_operator (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **ptokens, struct position *pos) {
	const char	*tokens = *ptokens;
	int			success;
	int			is_grouped;

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
		Error_Message (pos, "invalid operand for 'defined' operator [%s;%s]", get_token_name (tokens[-1]), tokens);
		success = 0;
	}
	*ptokens = tokens;
	return (success);
}

int		evaluate_builtin_foreach (struct bcpp *bcpp, struct tokenizer *tokenizer, struct tokenizer *macro_tokenizer, int offset, struct macro_desc *desc, struct macro_args *args) {
	int		success;

	if (args->is_variadic && args->size == 2 && args->tokens[args->size - 1][Macro_Arg]) {
		struct macro_args	cvargs = {0}, *vargs = &cvargs;
		usize		index;
		const char	*varg;
		int			group_level;
		char		*begin;

		success = 1;
		begin = macro_tokenizer->current;
		varg = args->tokens[args->size - 1][Macro_Call_Arg];
		group_level = 0;
		while (success && varg[-1]) {
			if (group_level == 0 && 0 == strcmp (varg, ",")) {
				success = end_tokenizer (macro_tokenizer, 0);
				if (success) {
					begin = get_next_from_tokenizer (macro_tokenizer, begin);
					success = push_macro_arg (vargs, begin);
					begin = macro_tokenizer->current;
				}
			} else {
				if (varg[-1] == Token_punctuator) {
					group_level += (0 == strcmp (varg, "("));
					group_level -= (0 == strcmp (varg, ")"));
				}
				success = copy_token (macro_tokenizer, varg);
			}
			varg = next_const_token (varg, 0);
		}
		if (success) {
			success = end_tokenizer (macro_tokenizer, 0);
			if (success) {
				begin = get_next_from_tokenizer (macro_tokenizer, begin);
				success = push_macro_arg (vargs, begin);
				begin = macro_tokenizer->current;
			}
		}
		index = 0;
		while (success && index < vargs->size) {
			const char	*macro_body;

			begin = macro_tokenizer->current;
			macro_body = args->tokens[0][Macro_Call_Arg];
			while (success && macro_body[-1]) {
				if (macro_body[-1] == Token_identifier && 0 == strcmp (macro_body, "__ARG__")) {
					const char	*varg;

					varg = vargs->tokens[index][Macro_Arg];
					set_token_offset ((char *) varg, get_token_offset (macro_body));
					while (success && varg[-1]) {
						success = copy_token (macro_tokenizer, varg);
						varg = next_const_token (varg, 0);
					}
				} else {
					success = copy_token (macro_tokenizer, macro_body);
				}
				macro_body = next_const_token (macro_body, 0);
			}
			success = success && end_tokenizer (macro_tokenizer, 0);
			if (success) {
				struct position pos = { .filename = "<macro>", .line = 1, .column = 1, };

				begin = get_next_from_tokenizer (macro_tokenizer, begin);
				if (get_token_offset (begin) == 0 && !(tokenizer->current && tokenizer->current[-1] == Token_newline)) {
					set_token_offset (begin, 1);
				}
				while (success && begin[-1]) {
					success = evaluate_token (bcpp, tokenizer, (const char **) &begin, &pos, 1, 0);
				}
			}
			index += 1;
		}
	} else {
		success = 1;
	}
	return (success);
}

static int g_eval_defined_operator = 0;

int		evaluate_macro_body (struct bcpp *bcpp, struct tokenizer *tokenizer, struct tokenizer *macro_tokenizer, int offset, struct macro_desc *desc, struct macro_args *args) {
	int		success;
	const char	*macro_body;

	if (desc->flags & Macro_Flag_builtin) {
		if (0 == strcmp (desc->ident, "_Foreach")) {
			success = evaluate_builtin_foreach (bcpp, tokenizer, macro_tokenizer, offset, desc, args);
		} else {
			Error ("unrecognized builtin macro");
			success = 0;
		}
	} else {
		Debug_Code (print_tokens (desc->ident, 0, "orig|", stderr));
		macro_body = desc->body;
		success = evaluate_pasting_concatenation_and_stringization (bcpp, macro_tokenizer, &macro_body, desc, args, &desc->pos);
		if (success) {
			struct position	cinner_pos = desc->pos, *inner_pos = &cinner_pos;

			if ((desc->flags & Macro_Flag_multiline) && !(desc->flags & Macro_Flag_disable_directives)) {
				/* TODO: operator evaluation for multiline macros */
				success = evaluate_directives (bcpp, tokenizer, macro_body, inner_pos, 0);
			} else {
				Debug_Code (char	prefix[256]);

				Debug_Code (snprintf (prefix, sizeof prefix, "macro '%s'|", desc->ident));
				Debug_Code (print_tokens (macro_body, 1, prefix, stderr));
				if (macro_body[-1]) {
					int		is_stringify = 0;
					char	*begin;

					begin = tokenizer->current;
					while (success && macro_body[-1]) {
						if (macro_body[-1] == Token_identifier && 0 == strcmp (macro_body, "_Stringify")) {
							is_stringify = 1;
							break ;
						} else {
							success = evaluate_token (bcpp, tokenizer, &macro_body, inner_pos, 1, 0);
						}
					}
					if (success) {
						begin = get_next_from_tokenizer (tokenizer, begin);
						if (begin && !(desc->flags & Macro_Flag_multiline)) {
							set_token_offset (begin, offset);
						}
						if (is_stringify) {
							Assert (macro_body[-1] == Token_identifier && 0 == strcmp (macro_body, "_Stringify"));
							Debug ("STRINGIFY");
							macro_body = next_const_token (macro_body, inner_pos);
							success = stringify_tokens (tokenizer, macro_body, 0, 0, 1, inner_pos);
						}
					}
				}
			}
		}
	}
	return (success);
}

int		evaluate_macro (struct bcpp *bcpp, struct tokenizer *tokenizer, struct macro_desc *desc, const char **ptokens, int offset, struct position *pos, int is_ws_macro_call) {
	int					success;
	struct macro_args	cargs = {0}, *args = &cargs;
	struct tokenizer	cmacro_tokenizer = {0}, *const macro_tokenizer = &cmacro_tokenizer;
	const char			*macro_body;

	if (desc->args && (is_ws_macro_call ? is_function_like_macro_call (*ptokens) : is_function_like_macro (*ptokens))) {
		success = init_macro_args (args, desc);
		if (success) {
			Debug ("eval macro call for %s", desc->ident);
			while (is_ws_macro_call && (*ptokens)[-1] == Token_newline) {
				push_newline_token (macro_tokenizer, 0);
				*ptokens = next_const_token (*ptokens, pos);
			}
			success = success && evaluate_macro_call (bcpp, macro_tokenizer, desc, args, ptokens, pos);
			Debug ("end eval macro call for %s", desc->ident);
		}
	} else if (desc->args) {
		/* TODO: WTF? */
		return (1);
	} else {
		args = 0;
		success = 1;
	}
	success = success && evaluate_macro_body (bcpp, tokenizer, macro_tokenizer, offset, desc, args);
	free_tokenizer (macro_tokenizer);
	return (success);
}

int		evaluate_token (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **ptokens, struct position *pos, int check_macro_stack, int is_top_level) {
	int			success;
	const char	*tokens = *ptokens;
	char		*begin = tokenizer->current;
	int			new_paste_column = -1;
	int			is_fc;
	static char	index_string[64] = "_Index";
	const usize	index_string_size = sizeof "_Index" - 1;

	// if (tokens[-1] == Token_identifier) {
	if (g_eval_defined_operator > 0 && tokens[-1] == Token_identifier && 0 == strcmp (tokens, "defined")) {
		tokens = next_const_token (tokens, pos);
		success = evaluate_defined_operator (bcpp, tokenizer, &tokens, pos);
		*ptokens = tokens;
	} else if (g_index >= 0 && tokens[-1] == Token_identifier &&
			0 == strncmp (tokens, index_string, index_string_size) &&
			isdigit (tokens[index_string_size]) && tokens[index_string_size + 1] == 0 &&
			tokens[index_string_size] - '0' <= g_index) {
		char	index[16];
		usize	length;

		length = snprintf (index, sizeof index, "%d", g_indicies[tokens[index_string_size] - '0']);
		success = push_token (tokenizer, get_token_offset (tokens), Token_preprocessing_number, index, length);
		tokens = next_const_token (tokens, pos);
		*ptokens = tokens;
	} else {
		is_fc = tokenizer->current && tokenizer->current[-1] == Token_identifier && is_function_like_macro_call (tokens);
		if (is_fc || tokens[-1] == Token_identifier) {
			struct macro_desc	*macro;
			const char			*ident = is_fc ? tokenizer->current : tokens;

			macro = find_macro (bcpp->macros, ident);
			if (macro && (!macro->args || (macro->args && (is_fc || is_function_like_macro_call (tokens))))) {
				if (macro->flags & Macro_Flag_no_recursion_check) {
					check_macro_stack = 0;
				}
				if ((check_macro_stack && !is_macro_pushed_to_stack (&bcpp->macro_stack, macro->ident)) || !check_macro_stack) {
					int		old_line = pos->line;

					if ((success = push_macro_stack (&bcpp->macro_stack, macro->ident))) {
						int		offset = get_token_offset (ident);

						if ((macro->flags & Macro_Flag_multiline) && is_top_level) {
							// success = push_line_directive (tokenizer, macro->pos.filename, macro->pos.line);
						}
						bcpp->was_multiline = bcpp->was_multiline || (macro->flags & Macro_Flag_multiline);
						if (is_fc) {
							success = success && revert_token (tokenizer);
						} else {
							*ptokens = next_const_token (*ptokens, pos);
						}
						success = success && evaluate_macro (bcpp, tokenizer, macro, ptokens, offset, pos, 1);
						pop_macro_stack (&bcpp->macro_stack);
						if ((macro->flags & Macro_Flag_multiline)) {
							// new_paste_column = pos->column - 1;
						}
					}
					if (success) {
						if ((macro->flags & Macro_Flag_multiline) || bcpp->was_multiline) {
							if (is_top_level) {
								success = push_line_directive (tokenizer, pos->filename, pos->line);
								bcpp->was_multiline = 0;
							}
						} else if (old_line < pos->line) {
							success = push_compiled_newline_token (tokenizer, pos->line - old_line, 0);
						}
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
		if (success && bcpp->paste_column >= 0) {
			begin = get_next_from_tokenizer (tokenizer, begin);
			if (begin) {
				set_token_offset (begin, bcpp->paste_column);
			}
		}
		bcpp->paste_column = new_paste_column;
	}
	return (success);
}

int		expand_and_evaluate_expression (struct bcpp *bcpp, const char *tokens, isize *ret, const struct position *apos) {
	int			success;
	struct position	cpos = *apos, *pos = &cpos;
	struct tokenizer	ctokenizer = {0}, *tokenizer = &ctokenizer;

	while (tokens[-1] && tokens[-1] != Token_newline) {
		if (tokens[-1] == Token_identifier && 0 == strcmp (tokens, "defined")) {
			tokens = next_const_token (tokens, pos);
			success = evaluate_defined_operator (bcpp, tokenizer, &tokens, pos);
		} else {
			g_eval_defined_operator += 1;
			success = evaluate_token (bcpp, tokenizer, &tokens, pos, 1, 0);
			g_eval_defined_operator -= 1;
		}
	}
	success = success && end_tokenizer (tokenizer, 0);
	if (success) {
		char		*begin;

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
		success = success && end_tokenizer (tokenizer, 0);
		if (success) {
			begin = get_next_from_tokenizer (tokenizer, begin);
			Debug_Code (print_tokens (begin, 1, "if expr|", stderr));
			success = evaluate_expression (begin, ret, pos);
			Debug ("Evaluated to %zd", *ret);
			if (!success) {
				Debug_Code (print_tokens (get_first_token (tokenizer), 1, "if expr expanded|", stderr));
			}
		}
	}
	free_tokenizer (tokenizer);
	return (success);
}

int		evaluate_calleach_directive (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **ptokens, struct position *pos, int is_top_level) {
	int			success;
	const char	*tokens = *ptokens;
	struct tokenizer	cmacro_tokenizer = {0}, *macro_tokenizer = &cmacro_tokenizer;
	const char			*macro_body;
	int					offset = get_token_offset (tokens);
	char				*begin = macro_tokenizer->current;

	success = 1;
	if (tokens[-1] == Token_punctuator && 1 == get_token_offset (tokens) && 0 == strcmp (tokens, "/")) {
		while (tokens[-1] && tokens[-1] != Token_newline) {
			tokens = next_const_token (tokens, pos);
		}
		while (tokens[-1] == Token_newline) {
			tokens = next_const_token (tokens, pos);
		}
		while (success && tokens[-1] && !(pos->at_the_beginning && tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "@"))) {
			success = copy_token (macro_tokenizer, tokens);
			tokens = next_const_token (tokens, pos);
		}
		if (!tokens[-1]) {
			Error_Message (pos, "'@' expected");
			success = 0;
		}
		success = success && end_tokenizer (macro_tokenizer, 0);
	} else {
		while (success && tokens[-1] && !(tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "@"))) {
			success = copy_token (macro_tokenizer, tokens);
			tokens = next_const_token (tokens, pos);
		}
		if (!tokens[-1]) {
			Error_Message (pos, "'@' expected");
			success = 0;
		}
		success = success && end_tokenizer (macro_tokenizer, 0);
	}
	if (success) {
		struct position	inner_pos = { .filename = "<macro>", .line = 1, .column = 1, };

		macro_body = get_next_from_tokenizer (macro_tokenizer, begin);
		begin = macro_tokenizer->current;
		while (success && macro_body[-1]) {
			success = evaluate_token (bcpp, macro_tokenizer, &macro_body, &inner_pos, 1, 0);
		}
		success = success && end_tokenizer (macro_tokenizer, 0);
	}
	if (success) {
		macro_body = get_next_from_tokenizer (macro_tokenizer, begin);
		Debug_Code (print_tokens (macro_body, 1, "calleach body", stderr));
		tokens = next_const_token (tokens, pos);
		if (success) {
			int		is_multiline = 0;
			usize	macros_count = 0;
			struct macro_desc	macros[256];
			const char	*end_tokens;

			memset (macros, 0, sizeof macros);
			if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "/") && 1 == get_token_offset (tokens)) {
				is_multiline = 1;
				tokens = next_const_token (tokens, pos);
				end_tokens = tokens;
				while (tokens[-1] && tokens[-1] != Token_newline) {
					tokens = next_const_token (tokens, pos);
				}
			}
			do {
				if (is_multiline) {
					while (tokens[-1] && tokens[-1] == Token_newline) {
						tokens = next_const_token (tokens, pos);
					}
					if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "#")) {
						tokens = next_const_token (tokens, pos);
						if (tokens[-1] == Token_identifier && 0 == strcmp (tokens, "end")) {
							const char	*end = end_tokens;

							tokens = next_const_token (tokens, pos);
							while (tokens[-1] && tokens[-1] != Token_newline && tokens[-1] == end[-1] && 0 == strcmp (tokens, end)) {
								tokens = next_const_token (tokens, pos);
								end = next_const_token (end, 0);
							}
							if (tokens[-1] == 0 || tokens[-1] == Token_newline) {
								break ;
							} else {
								continue ;
							}
						}
					}
				}
				if (macros_count < Array_Count (macros)) {
					struct macro_desc	*macro;

					macro = macros + macros_count;
					macros_count += 1;
					success = define_macro_body (macro, macro_tokenizer, &tokens, pos, 1);
					macro->ident = macro->args;
				} else {
					Error_Message (pos, "maximum number of cases is reached");
					success = 0;
				}
			} while (is_multiline && success);
			g_index += 1;
			g_indicies[g_index] = 0;
			if (macro_body) while (success && macro_body[-1]) {
				usize	macro_index = 0;
				struct macro_desc	*macro = 0;
				struct macro_args	cargs, *args = &cargs;

				while (success && macro_index < macros_count) {
					const char			*tokens = macro_body;

					memset (args, 0, sizeof *args);
					success = init_macro_args (args, macros + macro_index);
					/* note: ignorance of evaluate_macro_call error is on purpose */
					if (success) {
						if (tokens[-1] == Token (punctuator) && 0 == strcmp (tokens, "(")) {
							if (evaluate_macro_call (bcpp, macro_tokenizer, macros + macro_index, args, &tokens, 0)) {
								macro_body = (char *) tokens;
								macro = macros + macro_index;
								break ;
							}
						} else {
							Error_Message (pos, "calleach body should consist only argument lists");
							Assert (0);
							success = 0;
						}
					}
					macro_index += 1;
				}
				if (success) {
					if (macro) {
						if (is_top_level) {
							success = push_line_directive (tokenizer, macro->pos.filename, macro->pos.line);
						}
						if (success) {
							success = evaluate_macro_body (bcpp, tokenizer, macro_tokenizer, macro->pos.column - 1, macro, args);
							if (success && is_multiline && !(macro->flags & Macro_Flag_multiline)) {
								success = push_newline_token (tokenizer, 0);
							}
						}
					} else {
						Debug ("no case are match");
					}
					while (macro_body[-1] && macro_body[-1] != Token_newline) {
						macro_body = next_const_token (macro_body, 0);
					}
				}
				while (macro_body[-1] == Token_newline) {
					macro_body = next_const_token (macro_body, 0);
				}
				g_indicies[g_index] += 1;
			}
			if (is_top_level) {
				success = success && push_line_directive (tokenizer, pos->filename, pos->line);
			}
			g_index -= 1;
		}
	}
	free_tokenizer (macro_tokenizer);
	*ptokens = tokens;
	return (success);
}

int		evaluate_stringify_directive (struct bcpp *bcpp, struct tokenizer *tokenizer, const char **ptokens, struct position *pos) {
	int		success;
	const char	*tokens = *ptokens;

	if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "/") && 1 == get_token_offset (tokens)) {
		const char	*end_tokens;
		struct tokenizer	cinner_tokenizer = {0}, *inner_tokenizer = &cinner_tokenizer;

		tokens = next_const_token (tokens, pos);
		end_tokens = tokens;
		while (tokens[-1] != Token_newline) {
			tokens = next_const_token (tokens, pos);
		}
		tokens = next_const_token (tokens, pos);
		success = 1;
		while (success && tokens[-1]) {
			if (pos->at_the_beginning && tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "#")) {
				if (is_token (next_const_token (tokens, 0), Token_identifier, "end")) {
					const char	*start = tokens, *end = end_tokens;

					tokens = next_const_token (tokens, pos);
					tokens = next_const_token (tokens, pos);
					while (tokens[-1] && tokens[-1] != Token_newline && tokens[-1] == end[-1] && 0 == strcmp (tokens, end)) {
						tokens = next_const_token (tokens, pos);
						end = next_const_token (end, 0);
					}
					if (tokens[-1] == end[-1] && tokens[-1] == Token_newline) {
						break ;
					} else {
						while (success && start[-1] && start[-1] != Token_newline && start != tokens) {
							success = copy_token (inner_tokenizer, start);
							start = next_const_token (start, 0);
						}
					}
				} else {
					success = copy_token (inner_tokenizer, tokens);
					tokens = next_const_token (tokens, pos);
				}
			} else {
				success = copy_token (inner_tokenizer, tokens);
				tokens = next_const_token (tokens, pos);
			}
		}
		success = success && end_tokenizer (inner_tokenizer, 0);
		if (success) {
			success = stringify_tokens (tokenizer, get_first_token (inner_tokenizer), 0, 0, 1, pos);
		}
		free_tokenizer (inner_tokenizer);
	} else {
		Error_Message (pos, "only multiline stringify is supported right now");
		success = 0;
	}
	*ptokens = tokens;
	return (success);
}

int		evaluate_directives (struct bcpp *bcpp, struct tokenizer *tokenizer, const char *tokens, struct position *pos, int is_top_level) {
	const char			*next = next_const_token (tokens, 0);
	int					success = 1;
	int		is_active = 1;
	int		ifs_level = -1;
	int		is_bypass = 0;
	struct {
		int		is_already_selected;
		int		prev_active;
		int		prev_bypass;
	} ifs[128];

	while (tokens[-1] && success) {
		const char	*next = next_const_token (tokens, 0);

		if ((tokens[-1] == Token_newline && next[-1] == Token_punctuator && 0 == strcmp (next, "#")) ||
			(pos->at_the_beginning && tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "#"))) {
			if (tokens[-1] == Token_newline) {
				copy_token (tokenizer, tokens);
				tokens = next_const_token (tokens, pos);
				tokens = next_const_token (tokens, pos);
			} else {
				next = tokens;
				tokens = next_const_token (tokens, pos);
			}
			if (tokens[-1] == Token_identifier) {
				int		is_unhandled = 0;

				if (0 == strcmp (tokens, "include") || 0 == strcmp (tokens, "import")) {
					if (is_active) {
						int		is_import = 0 == strcmp (tokens, "import");
						int		was_multiline = bcpp->was_multiline;
						int		paste_column = bcpp->paste_column;

						bcpp->was_multiline = 0;
						bcpp->paste_column = 0;
						tokens = next_const_token (tokens, pos);
						/* TODO(Viktor): include with macro argument */
						if (tokens[-1] == Token_path_relative) {
							if (!include_file_relative (bcpp, tokenizer, tokens, pos->filename, pos, is_import)) {
								Error_Message (pos, "while trying to include \"%s\" file", tokens);
								success = 0;
							}
						} else if (tokens[-1] == Token_path_global) {
							if (!include_file_global (bcpp, tokenizer, tokens, pos, is_import)) {
								Error_Message (pos, "while trying to include <%s> file", tokens);
								success = 0;
							}
						} else {
							Error_Message (pos, "invalid argument for 'include' directive, expected \"...\" or <...> strings");
							success = 0;
						}
						success = success && push_line_directive (tokenizer, pos->filename, pos->line);
						bcpp->was_multiline = was_multiline;
						bcpp->paste_column = paste_column;
					} else {
						is_unhandled = 1;
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
									 success = push_compiled_newline_token (tokenizer, pos->line - old_line, 0);
								}
							}
						} else {
							Error_Message (pos, "invalid argument for 'define' directive, expected identifier");
							Assert (0);
							success = 0;
						}
					} else {
						is_unhandled = 1;
					}
				} else if (0 == strcmp (tokens, "undef")) {
					if (is_active) {
						tokens = next_const_token (tokens, pos);
						if (tokens[-1] == Token_identifier) {
							struct macro_desc	*macro;

							macro = find_macro (bcpp->macros, tokens);
							if (macro) {
								macro->flags |= Macro_Flag_undef;
							}
						} else {
							Error_Message (pos, "invalid argument for 'undef' directive, expected identifier");
							success = 0;
						}
					} else {
						is_unhandled = 1;
					}
				} else if (0 == strcmp (tokens, "if")) {
					if (ifs_level + 1 < (int) Array_Count (ifs)) {
						ifs_level += 1;
						memset (ifs + ifs_level, 0, sizeof ifs[0]);
						ifs[ifs_level].prev_active = is_active;
						ifs[ifs_level].prev_bypass = is_bypass;
						if (is_active) {
							isize	ret = 0;

							tokens = next_const_token (tokens, pos);
							if (tokens[-1] == Token_identifier && 0 == strcmp (tokens, "BYPASS")) {
								is_active = 0;
								is_bypass = 1;
								success = 1;
							} else {
								success = expand_and_evaluate_expression (bcpp, tokens, &ret, pos);
								is_active = !!ret;
								if (is_active) {
									ifs[ifs_level].is_already_selected = 1;
								}
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
								if (tokens[-1] == Token_identifier && 0 == strcmp (tokens, "BYPASS")) {
									is_active = 0;
									is_bypass = 1;
									success = 1;
								} else {
									success = expand_and_evaluate_expression (bcpp, tokens, &ret, pos);
									is_active = !!ret;
									if (is_active) {
										ifs[ifs_level].is_already_selected = 1;
									}
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
								is_bypass = 0;
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
						is_bypass = ifs[ifs_level].prev_bypass;
						ifs_level -= 1;
					} else {
						Error_Message (pos, "'endif' directive without 'if', 'ifdef' or 'ifndef'");
						success = 0;
					}
				} else if (0 == strcmp (tokens, "calleach")) {
					if (is_active) {
						tokens = next_const_token (tokens, pos);
						success = evaluate_calleach_directive (bcpp, tokenizer, &tokens, pos, is_top_level);
					}
				} else if (0 == strcmp (tokens, "stringify")) {
					Debug ("STRINGIFY");
					if (is_active) {
						tokens = next_const_token (tokens, pos);
						success = evaluate_stringify_directive (bcpp, tokenizer, &tokens, pos);
					} else {
						is_unhandled = 1;
					}
				} else if (0 == strcmp (tokens, "warning")) {
					if (is_active) {
						char	prefix[256];

						snprintf (prefix, sizeof prefix, "%s:%d:%d: warning:", pos->filename, pos->line, pos->column);
						print_tokens_until (next_const_token (tokens, 0), 0, prefix, Token_newline, stderr);
					} else {
						is_unhandled = 1;
					}
				} else if (0 == strcmp (tokens, "error")) {
					if (is_active) {
						char	prefix[256];

						snprintf (prefix, sizeof prefix, "%s:%d:%d: error:", pos->filename, pos->line, pos->column);
						print_tokens_until (next_const_token (tokens, 0), 0, prefix, Token_newline, stderr);
						success = 0;
					} else {
						is_unhandled = 1;
					}
				} else if (0 == strcmp (tokens, "pragma")) {
					if (is_active) {
						tokens = next_const_token (tokens, pos);
						if (tokens[-1] == Token_identifier && 0 == strcmp (tokens, "once")) {
							success = revert_token (&bcpp->do_not_include_those);
							success = success && push_token (&bcpp->do_not_include_those, 0, Token_string, pos->filename, strlen (pos->filename));
							success = success && end_tokenizer (&bcpp->do_not_include_those, 0);
						} else {
							success = push_token (tokenizer, 0, Token_punctuator, "#", 1);
							success = success && push_token (tokenizer, 0, Token_punctuator, "pragma", 6);
							while (success && tokens[-1] && tokens[-1] != Token_newline) {
								success = copy_token (tokenizer, tokens);
								tokens = next_const_token (tokens, pos);
							}
						}
					} else {
						is_unhandled = 1;
					}
				} else {
					if (is_active) {
						Error_Message (pos, "invalid preprocessing directive '%s'", tokens);
						success = 0;
					}
				}
				if (success) {
					if (is_unhandled && is_bypass) {
						success = copy_token (tokenizer, next);
						while (success && tokens[-1] && tokens[-1] != Token_newline) {
							success = copy_token (tokenizer, tokens);
							tokens = next_const_token (tokens, pos);
						}
					} else {
						while (tokens[-1] && tokens[-1] != Token_newline) {
							tokens = next_const_token (tokens, pos);
						}
					}
					next = tokens;
				}
			} else if (tokens[-1] == Token_newline) {
				next = tokens;
			} else {
				Error_Message (pos, "invalid preprocessing directive");
				success = 0;
			}
			tokens = next;
		} else {
			if (is_active) {
				success = evaluate_token (bcpp, tokenizer, &tokens, pos, 1, is_top_level);
			} else if (is_bypass) {
				copy_token (tokenizer, tokens);
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
		const char	*ptr, *sptr;
		int			is_framework;

		while (isspace (*source)) {
			source += 1;
		}
		ptr = sptr = strchr (source, '\n');
		if (!ptr) {
			break ;
		}
		ptr = strstr (source, " (framework directory)");
		if (ptr && ptr < sptr) {
			is_framework = 1;
		} else {
			is_framework = 0;
			ptr = sptr;
			is_framework = 0;
		}
		if (ptr - source > 0) {
			success = push_string_token (&bcpp->include_paths, is_framework * Is_Framework_Includepath_Flag, source, ptr - source, 0);
			include_count += 1;
		}
		source = sptr;
	}
	if (!success) {
		free_tokenizer (&bcpp->include_paths);
	}
	success = success && end_tokenizer (&bcpp->include_paths, 0);
	if (success) {
		const char	*includes;

		bcpp->include_paths_sorted = malloc ((include_count + 1) * sizeof *bcpp->include_paths_sorted);
		bcpp->include_paths_sorted[include_count] = 0;
		includes = get_first_token (&bcpp->include_paths);
		while (includes[-1] && include_count > 0) {
			include_count -= 1;
			bcpp->include_paths_sorted[include_count] = includes;
			includes = next_const_token (includes, 0);
		}
	}
	return (success);
}

int		init_bcpp (struct bcpp *bcpp, const char *lang, int args_count, char *args[], char *env[]) {
	int			success;
	const char	*whereis_location;

	memset (bcpp, 0, sizeof *bcpp);
	bcpp->paste_column = -1;
	bcpp->frameworks_count = 0;
	end_tokenizer (&bcpp->do_not_include_those, 0);
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
	if (success && 0 != strcmp (lang, "no")) {
		char	*output;

		output = read_output_of_program (whereis_location, 1, 2, (char *[]) { (char *) whereis_location, "cc", 0 }, env);
		if (output) {
			char	*path = strchr (output, '/'), *orig_path = output;

			while (*path && *path != '\n' && *path != ' ') {
				path += 1;
			}
			*path = 0;
			path = strchr (output, '/');
			output = read_output_of_program (path, 2, 6, (char *[]) { path, "-v", "-pthread", "-xc", "-E", "-", 0 }, env);
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
				output = read_output_of_program (path, 1, 6, (char *[]) { path, "-dM", "-E", "-x", (char *) lang, "-", 0 }, env);
				if (output) {
					bcpp->predefined = output;
					bcpp->predefined_size = strlen (bcpp->predefined);
				} else {
					success = 0;
				}
				end_tokenizer (&bcpp->do_not_include_those, 0);
			} else {
				success = 0;
			}
			free (orig_path);
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

int		test_bcpp(struct bcpp *bcpp, const char *filename) {
	char	*preprocessed;
	int		success;

	preprocessed = make_translation_unit (bcpp, filename);
	if (preprocessed) {
		// print_tokens (preprocessed, 1, "output|");
		print_tokens (preprocessed, 0, "", stdout);
		// print_macro_list (bcpp->macros);
		free_tokens (preprocessed);
		// print_macro_list (bcpp->macros);
		success = 1;
	} else {
		// Error ("no preprocessed");
		success = 0;
	}
	return (success);
}

