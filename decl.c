



struct state_enum {
	const char	*identifier;
	const char	*constants;
	enum {
		State_Enum_invalid,
		State_Enum_enum_keyword,
		State_Enum_identifier_or_body,
		State_Enum_body_or_end,
		State_Enum_constant,
		State_Enum_intializer_or_constant,
		State_Enum_intializer,
	} state;
};

struct decl {
	struct tokenizer	tokens;
	enum decl_state {
		State_Decl_search_state,
		State_Decl_enum_state,
		State_Decl_skip_line,
	} state;
	enum decl_state		next_state;
	int					is_newline;

	#define Paged_Array (type, name) \
	usize				name##_cap;\
	usize				name##_count;\
	type				*name;\
	type				*name##_data;

	Paged_Array (struct state_enum, enums);
};

int		push_state (void *pstates, usize size, usize *count, usize *cap) {
	void	*states = *(void **) pstates;
	int		success;

	if (*count + 1 >= *cap * size) {
		void	*memory;

		memory = expand_array (0, cap);
		if (memory) {
			*(void **) ((char *) states + *count * size) = memory;
			*count = 0;
			success = 1;
		} else {
			success = 0;
		}
	} else {
		success = 1;
	}
	if (success) {
		memset ((char *) states + *count * size, 0, size * 2);
		*count += 1;
	}
	return (success);
}

#define Push_Paged_Array (from, name) ##1
push_state (&(from)->name##_data, sizeof *(from)->name, &(from)->name##_count, &(from)->name##_cap)

#define Paged_Array_Iterator(type, name, invalid_condition) ##
type	*next_##name (type *state) {
	if (invalid_condition) {
		if (*(void **) state != 0) {
			state = *(void **) state;
		} else {
			state = 0;
		}
	} else {
		state += 1;
		if (invalid_condition) {
			if (*(void **) state != 0) {
				state = *(void **) state;
			} else {
				state = 0;
			}
		}
	}
	return (state);
}
#end Paged_Array_Iterator

#define Paged_Array_Iterator(type, name, invalid_condition) \
type	*next_##name (type *state) {\
	if (invalid_condition) {\
		if (*(void **) state != 0) {\
			state = *(void **) state;\
		} else {\
			state = 0;\
		}\
	} else {\
		state += 1;\
		if (invalid_condition) {\
			if (*(void **) state != 0) {\
				state = *(void **) state;\
			} else {\
				state = 0;\
			}\
		}\
	}\
	return (state);\
}

Paged_Array_Iterator (struct enum_state, enum_state, state->state == Enum_State_invalid);

void	init_decl (struct decl *decl) {
	memset (decl, 0, sizeof *decl);
	decl->is_newline = 1;
	decl->
}

int		step_decl_token (struct decl *decl, const char *token) {
	if (token[-1] == Token)

	switch (decl->state) {

	}
}







#undef Paged_Array_Iterator
#undef Push_Paged_Array
#undef Paged_Array


specifiers-and-qualifiers  declarators-and-initializers ;

type_specifiers:
	void
	arithmetic type
	atomic type
	typedef name
	struct, union, enum
storage_class_specifiers:
	static
	register
	extern
	auto
	_Thread_local
type qualifiers:
	const
	volatile
	restrict
	_Atomic
function specifiers:
	inline
	_Noreturn
alignment specifiers:
	_Alignas


Declor_group
Declor_pointer
Declor_funcall
Declor_array


int (*main) ()








