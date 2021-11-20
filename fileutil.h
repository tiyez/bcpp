
#ifndef Header_fileutil
#define Header_fileutil

#include "memutil.h"

char	*read_entire_file (const char *filename, usize *size);

#endif /* Header_fileutil */

#if (defined(Implementation_tokenizer) || defined(Implementation_All)) && !defined(Except_Implementation_tokenizer) && !defined(Implemented_tokenizer)
#define Implemented_tokenizer

#define Implementation_memutil
#include "memutil.h"
#include "def.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef Option_fileutil_Open_Binary
#	define Open_File_Mode "rb"
#else
#	define Open_File_Mode "r"
#endif

char	*read_entire_file (const char *filename, usize *psize) {
	FILE	*file;
	usize	size;
	char	*result;

	file = fopen (filename, Open_File_Mode);
	if (file) {
		fseek (file, 0, SEEK_END);
		size = ftell (file);
		fseek (file, 0, SEEK_SET);
		if (size > 0) {
			result = malloc (size + 1);
			if (result) {
				if (0 < fread (result, 1, size, file)) {
					result[size] = 0;
					if (psize) {
						*psize = size;
					}
				} else {
					Error ("cannot read file '%s'", filename);
					free (result);
					result = 0;
					*psize = 0;
				}
			} else {
				Error ("cannot allocate memory to read file '%s'", filename);
				*psize = 0;
			}
		} else {
			result = malloc (1);
			*psize = 0;
			if (result) {
				*result = 0;
			} else {
				Error ("cannot allocate one byte memory");
			}
		}
		fclose (file);
		file = 0;
	} else {
		Error ("cannot open file '%s'", filename);
		result = 0;
		*psize = 0;
	}
	return (result);
}


#undef Open_File_Mode

#endif /* (defined(Implementation_tokenizer) || defined(Implementation_All)) && !defined(Except_Implementation_tokenizer) && !defined(Implemented_tokenizer) */
