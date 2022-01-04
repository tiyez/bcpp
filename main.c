
#define Release


#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "def.h"

#include "memutil.h"
#define Option_fileutil_Open_Binary
#include "fileutil.h"
#include "systemutil.h"

int		g_no_line_directives;

#include "text_preprocessor.c"
#include "tokenizer.c"
// #include "decl.c"
#include "preproc.c"

#define Implementation_All
#include "memutil.h"
#define Option_fileutil_Open_Binary
#include "fileutil.h"
#include "systemutil.h"






void	tokenizer_stress_test (void) {
	struct tokenizer	temp_tokenizer = {0}, *tokenizer = &temp_tokenizer;
	int		success;
	usize	index;

	success = 1;
	index = 0;
	while (success && index < 1024 * 1024) {
		char	buf[64];

		snprintf (buf, sizeof buf, "TOKEN_%zu", index);
		success = push_token (tokenizer, 0, Token_identifier, buf, strlen (buf));
		success = success && push_newline_token (tokenizer, 0);
		index += 1;
	}
	if (success) {
		success = end_tokenizer (tokenizer, 0);
		if (success) {
			print_tokens_until (get_first_token (tokenizer), 1, "", Token_eof, stderr);
		}
	} else {
		Error ("something wrong");
	}
}


void	macro_desc_stress_test (struct bcpp *bcpp) {
	usize	index;
	struct tokenizer	temp_tokenizer = {0}, *tokenizer = &temp_tokenizer;
	int		success;

	success = 1;
	index = 0;
	while (success && index < 32 * 1024) {
		char	buf[64];
		const char	*tokens;
		struct position	pos = { .filename = "<undefined>", .line = 1, .column = 1, };

		reset_tokenizer (tokenizer);
		snprintf (buf, sizeof buf, "MACRO_%zu", index);
		success = push_token (tokenizer, 0, Token_identifier, buf, strlen (buf));
		success = success && push_newline_token (tokenizer, 0);
		success = success && end_tokenizer (tokenizer, 0);
		tokens = get_first_token (tokenizer);
		success = success && define_macro (bcpp, &tokens, &pos);
		index += 1;
	}
	if (success) {
		print_macro_list (bcpp->macros);
	} else {
		Error ("something wrong");
	}
}

#include <getopt.h>

void	print_deps (struct bcpp *bcpp, const char *token, FILE *file, const char *stack[], usize *pstack_size);

void	usage(const char *progname) {
	printf ("BCPP v0.1\n\nUsage: %s filename\n", progname);
	printf ("\nOptions:\n");
	printf ("    -d --depfile filename - generate Makefile rules for each translation unit\n");
	printf ("    -x language - pass language parameter to cc when grabbing built-in macros\n");
}

int main (int args_count, char *args[], char *env[]) {
	const struct option options[] = {
		{ .name = "depfile", required_argument, 0, 'd' },
		{ 0 },
	};
	const char	*depfile = 0, *lang = "c", *outputfile = 0, *filename = 0;
	struct bcpp	cbcpp = {0}, *bcpp = &cbcpp;
	struct tokenizer	cuserincludes = {0}, *userincludes = &cuserincludes;
	int			success;

	success = optind < args_count;
	while (success && optind < args_count) {
		int ch;

		Debug ("optind: %d", optind);
		if ((ch = getopt_long (args_count, args, "d:x:o:I:hl", options, 0)) >= 0) {
			Debug ("ch: %c", ch);
			switch (ch) {
				case 'd': depfile = optarg; break ;
				case 'x': lang = optarg; break ;
				case 'o': outputfile = optarg; break ;
				case 'l': g_no_line_directives = 1; break ;
				case 'I': {
					if (userincludes->current) {
						success = revert_token (userincludes);
					}
					success = success && push_string_token (userincludes, 0, optarg, strlen (optarg), 0);
					success = success && end_tokenizer (userincludes, 0);
				} break ;
				case '?': case 'h': usage (args[0]); return 0;
			}
		} else if (filename) {
			Error ("only one filename can be specified");
			success = 0;
		} else {
			filename = args[optind];
			Debug ("filename: %s", filename);
			optind += 1;
		}
	}
	if (success) {
		success = init_bcpp (bcpp, lang, args_count, args, env);
		bcpp->userincludes = cuserincludes;
		if (success) {
			if (filename) {
				char	*preprocessed;

				preprocessed = make_translation_unit (bcpp, filename);
				if (preprocessed) {
					print_tokens (preprocessed, 0, "", stdout);
					free_tokens (preprocessed);
					success = 1;
				} else {
					success = 0;
				}
			} else {
				Error ("no file specified");
				success = 0;
			}
		} else {
			Error ("cannot initialize bcpp");
		}
	}
	if (success) {
		if (depfile) {
			FILE	*file = fopen (depfile, "w");

			if (file) {
				const char	*token;

				token = get_first_token (&bcpp->filecache.filenames);
				if (token && token[-1]) {
					const char	*dep;
					const char	*stack[128];
					usize		stack_size = 0;

					if (outputfile) {
						fprintf (file, "%s: ", outputfile);
					} else {
						fprintf (file, "%.*so: ", get_token_length (token) - 1, token);
					}
					print_deps (bcpp, token, file, stack, &stack_size);
					fprintf (file, "\n\n");
					token = next_const_token (token, 0);
					while (token[-1]) {
						fprintf (file, "%s: ", token);
						print_deps (bcpp, token, file, stack, &stack_size);
						fprintf (file, "\n\n");
						token = next_const_token (token, 0);
					}
				} else {
					Error ("no translation unit file in the file cache");
				}
			} else {
				Error ("cannot open file to write makefile dependencies");
			}
		}
	}
	return (!success);
}

void	print_deps (struct bcpp *bcpp, const char *token, FILE *file, const char *stack[], usize *pstack_size) {
	const char	*dep;

	stack[(*pstack_size)++] = token;
	dep = get_file_dep (&bcpp->filecache, get_token_offset (token));
	while (dep) {
		fprintf (file, "%s ", dep);
		token = dep;
		if (get_file_cache_index (&bcpp->filecache, &token) >= 0) {
			usize	index = 0;

			while (index < *pstack_size && 0 != strcmp (token, stack[index])) {
				index += 1;
			}
			if (index >= *pstack_size) {
				print_deps (bcpp, token, file, stack, pstack_size);
			}
		}
		dep = get_next_file_dep (&bcpp->filecache, dep);
	}
	*pstack_size -= 1;
}



