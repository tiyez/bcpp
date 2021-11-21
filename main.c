



#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "def.h"

#include "memutil.h"
#define Option_fileutil_Open_Binary
#include "fileutil.h"
#include "systemutil.h"

#include "text_preprocessor.c"
#include "tokenizer.c"
// #include "decl.c"
#include "preproc.c"

#define Implementation_All
#include "memutil.h"
#define Option_fileutil_Open_Binary
#include "fileutil.h"
#include "systemutil.h"






void	tokenizer_stess_test (void) {
	struct tokenizer	temp_tokenizer = {0}, *tokenizer = &temp_tokenizer;
	int		success;
	usize	index;

	success = 1;
	index = 0;
	while (success && index < 1024 * 1024) {
		char	buf[64];

		snprintf (buf, sizeof buf, "TOKEN_%zu", index);
		success = push_token (tokenizer, 0, Token_identifier, buf, strlen (buf));
		success = success && push_newline_token (tokenizer);
		index += 1;
	}
	if (success) {
		success = end_tokenizer (tokenizer);
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
		success = success && push_newline_token (tokenizer);
		success = success && end_tokenizer (tokenizer);
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


int main (int args_count, char *args[], char *env[]) {

	struct bcpp	cbcpp = {0}, *bcpp = &cbcpp;

	// test_first_four_preprocessing_stages ();
	// test_tokenize_stage ();
	init_bcpp (bcpp, args_count, args, env);
	// test_bcpp (bcpp, "/Users/jsandsla/Projects/bcpp/test.c");
	// test_bcpp (bcpp, "test.c");
	if (args_count > 1) {
		test_bcpp (bcpp, args[1]);
	} else {
		test_bcpp (bcpp, "main.c");
	}

	// macro_desc_stress_test (bcpp);
	// tokenizer_stess_test ();

}




