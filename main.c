



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












int main (int args_count, char *args[], char *env[]) {

	struct bcpp	cbcpp = {0}, *bcpp = &cbcpp;

	// test_first_four_preprocessing_stages ();
	// test_tokenize_stage ();
	init_bcpp (bcpp, args_count, args, env);
	// test_bcpp (bcpp, "/Users/jsandsla/Projects/bcpp/test.c");
	test_bcpp (bcpp, "main.c");

}




