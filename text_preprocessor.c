


static int		read_next_char_offset (const char *content) {
	return (content[1] == '\\' && content[2] == '\n' ? 3 : 1);
}

static char	read_next_char (const char *content) {
	return (content[read_next_char_offset (content)]);
}

struct newline_array {
	int		*data;
	usize	size;
	usize	cap;
};

int		push_newline (struct newline_array *array, int line) {
	int		result;

	if (array->size >= array->cap / sizeof *array->data) {
		void	*data;

		data = expand_array (array->data, &array->cap);
		if (data) {
			array->data = data;
			result = 1;
		} else {
			result = 0;
		}
	} else {
		result = 1;
	}
	if (result) {
		array->data[array->size] = line;
		array->size += 1;
	}
	return (result);
}

usize	preprocess_text (char *content, char *end, int **nl_array) {
	char	*read = content, *write = content;
	struct newline_array	cnewline_array = {0}, *newline_array = &cnewline_array;
	int		line = 1;
	int		is_string = 0;

	while (read < end) {
		if (*read == '?' && read[1] == '?') {
			read += 2;
			switch (*read) {
				case '(':	*write++ = '['; break ;
				case ')':	*write++ = ']'; break ;
				case '<':	*write++ = '{'; break ;
				case '>':	*write++ = '}'; break ;
				case '=':	*write++ = '#'; break ;
				case '/':	*write++ = '\\'; break ;
				case '\'':	*write++ = '^'; break ;
				case '!':	*write++ = '|'; break ;
				case '-':	*write++ = '~'; break ;
				default: {
					read -= 2;
					*write++ = *read;
				} break ;
			}
			read += 1;
		} else if (*read == '\\' && read[1] == '\n') {
			read += 2;
			push_newline (newline_array, line);
			line += 1;
		} else if (*read == '\\' && read[1] == '\"') {
			*write++ = *read++;
			*write++ = *read++;
		} else if (!is_string && *read == '/' && read_next_char (read) == '/') {
			int		offset = read_next_char_offset (read);

			if (offset > 1) {
				push_newline (newline_array, line);
			}
			read += offset;
			while (*read && *read != '\n') {
				offset = read_next_char_offset (read);
				if (offset > 1) {
					push_newline (newline_array, line);
				}
				read += offset;
			}
			*write++ = ' ';
		} else if (!is_string && *read == '/' && read_next_char (read) == '*') {
			int		offset = read_next_char_offset (read);

			if (offset > 1) {
				push_newline (newline_array, line);
				line += 1;
			}
			read += 1 + offset;
			while (*read) {
				offset = read_next_char_offset (read);

				if (*read == '*' && read[offset] == '/') {
					read += 1 + offset;
					break ;
				}
				if (*read == '\n') {
					push_newline (newline_array, line);
					line += 1;
				}
				read += 1;
			}
			if (offset > 1) {
				push_newline (newline_array, line);
				line += 1;
			}
			*write++ = ' ';
		} else if (*read == '\"') {
			is_string = !is_string;
			*write++ = *read++;
		} else if (*read == 0) {
			*write++ = ' ';
			read += 1;
		} else if (write != read) {
			line += *read == '\n';
			*write++ = *read++;
		} else {
			line += *read == '\n';
			write = read += 1;
		}
	}
	*write = 0;
	push_newline (newline_array, 0);
	*nl_array = newline_array->data;
	return (write - content);
}

#ifndef Without_Tests
void	test_first_four_preprocessing_stages (void) {
	usize	size;
	char	*content = read_entire_file (__FILE__, &size);
	int		*newline_array;

	if (content) {
		preprocess_text (content, content + size, &newline_array);
/\
/ comment
	if/\
* com\
ment *\
/(con\
tent) {
			Debug ("file loaded:\n%s", content);
			(void)content??(0??);
			int		*array = newline_array;
			while (*array) {
				Debug ("newline at %d", *array);
				array += 1;
			}
			free (newline_array);
			free (content);
		} else {
			Error ("no content");
		}
	}
}
#endif
