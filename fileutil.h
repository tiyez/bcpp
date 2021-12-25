
#ifndef Header_fileutil
#define Header_fileutil

#include "memutil.h"

char	*read_entire_file (const char *filename, usize *size);

#endif /* Header_fileutil */

#if (defined(Implementation_fileutil) || defined(Implementation_All)) && !defined(Except_Implementation_fileutil) && !defined(Implemented_fileutil)
#define Implemented_fileutil

#define Implementation_memutil
#include "memutil.h"
#include "def.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef Option_fileutil_Open_Binary
#	define Open_File_Mode "rb"
#else
#	define Open_File_Mode "r"
#endif

char	*read_entire_file (const char *filename, usize *psize) {
	int		fd;
	usize	size;
	char	*result;

	fd = open (filename, O_RDONLY);
	if (fd >= 0) {
		size = lseek (fd, 0, SEEK_END);
		lseek (fd, 0, SEEK_SET);
		if (size > 0) {
			result = malloc (size + 1);
			if (result) {
				if (0 < read (fd, result, size)) {
					result[size] = 0;
					if (psize) {
						*psize = size;
					}
				} else {
					Error ("cannot read fd '%s'", filename);
					free (result);
					result = 0;
					*psize = 0;
				}
			} else {
				Error ("cannot allocate memory to read fd '%s'", filename);
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
		close (fd);
		fd = 0;
	} else {
		Error ("cannot open fd '%s'", filename);
		result = 0;
		*psize = 0;
	}
	return (result);
}


#undef Open_File_Mode

#endif /* (defined(Implementation_fileutil) || defined(Implementation_All)) && !defined(Except_Implementation_fileutil) && !defined(Implemented_fileutil) */
