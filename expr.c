


struct value_stack {
	usize	count;
	isize	*values;
	isize	memory[256];
};

void	init_value_stack (struct value_stack *stack) {
	stack->count = 0;
	stack->values = stack->memory;
}

int		push_value (struct value_stack *stack, isize value) {
	int		success;

	if (stack->count + (stack->values - stack->memory) < Array_Count (stack->memory)) {
		stack->values[stack->count] = value;
		stack->count += 1;
		success = 1;
	} else {
		Error ("value stack overflow");
		success = 0;
	}
	return (success);
}

isize	view_value (struct value_stack *stack) {
	return (stack->values[stack->count - 1]);
}

isize	pop_value (struct value_stack *stack) {
	if (stack->count > 0) {
		stack->count -= 1;
		return (stack->values[stack->count]);
	} else {
		return (0);
	}
}

int		push_value_stack (struct value_stack *stack) {
	int		success;

	if (push_value (stack, stack->count)) {
		stack->values += stack->count;
		stack->count = 0;
		success = 1;
	} else {
		Error ("value stack overflow");
		success = 0;
	}
	return (success);
}

void	pop_value_stack (struct value_stack *stack) {
	if (stack->values > stack->memory) {
		stack->count = stack->values[-1];
		stack->values -= stack->count + 1;
	}
}

struct op_stack {
	usize	count;
	u8		*ops;
	u8		memory[256];
};

void	init_op_stack (struct op_stack *stack) {
	stack->count = 0;
	stack->ops = stack->memory;
}

int		push_op (struct op_stack *stack, usize index) {
	int		success;

	if (stack->count + (stack->ops - stack->memory) < Array_Count (stack->memory)) {
		stack->ops[stack->count] = index;
		stack->count += 1;
		success = 1;
	} else {
		Error ("op stack overflow");
		success = 0;
	}
	return (success);
}

usize	view_op (struct op_stack *stack) {
	return (stack->ops[stack->count - 1]);
}

usize	pop_op (struct op_stack *stack) {
	if (stack->count > 0) {
		stack->count -= 1;
		return (stack->ops[stack->count]);
	} else {
		return (0);
	}
}

int		push_op_stack (struct op_stack *stack) {
	int		success;

	if (push_op (stack, stack->count)) {
		stack->ops += stack->count;
		stack->count = 0;
		success = 1;
	} else {
		Error ("op stack overflow");
		success = 0;
	}
	return (success);
}

void	pop_op_stack (struct op_stack *stack) {
	if (stack->ops > stack->memory) {
		stack->count = stack->ops[-1];
		stack->ops -= stack->count + 1;
	}
}

struct op {
	int		precedence;
	int		is_unary;
	char	string[8];
};

enum op_enum {
	Op_unary_plus,
	Op_unary_minus,
	Op_logical_not,
	Op_bitwise_not,

	Op_multiply,
	Op_divide,
	Op_remaining,
	Op_add,
	Op_subtract,
	Op_shift_left,
	Op_shift_right,
	Op_less,
	Op_greater,
	Op_less_eq,
	Op_greater_eq,
	Op_eq,
	Op_not_eq,
	Op_bitwise_and,
	Op_bitwise_xor,
	Op_bitwise_or,
	Op_logical_and,
	Op_logical_or,
};

const struct op	ops[] = {
	{ 1, 1, "+" },
	{ 1, 1, "-" },
	{ 1, 1, "!" },
	{ 1, 1, "~" },

	{ 2, 0, "*" },
	{ 2, 0, "/" },
	{ 2, 0, "%" },

	{ 3, 0, "+" },
	{ 3, 0, "-" },

	{ 4, 0, "<<" },
	{ 4, 0, ">>" },

	{ 5, 0, "<" },
	{ 5, 0, ">" },
	{ 5, 0, "<=" },
	{ 5, 0, ">=" },

	{ 6, 0, "==" },
	{ 6, 0, "!=" },

	{ 7, 0, "&" },

	{ 8, 0, "^" },

	{ 9, 0, "|" },

	{ 10, 0, "&&" },

	{ 11, 0, "||" },
};

int		evaluate_operator (struct value_stack *vstack, struct op_stack *ostack) {
	int			success;
	usize		op_index;
	const struct op	*op;

	op_index = pop_op (ostack);
	op = &ops[op_index];
	if (op->is_unary) {
		isize	value = pop_value (vstack), result;

		switch (op_index) {
			case Op_unary_plus: result = +value; break ;
			case Op_unary_minus: result = -value; break ;
			case Op_logical_not: result = !value; break ;
			case Op_bitwise_not: result = ~value; break ;
		}
		// Debug ("evaluating unary '%s %zd', result is %zd", op->string, value, result);
		success = push_value (vstack, result);
	} else {
		isize	left, right, result;

		right = pop_value (vstack);
		left = pop_value (vstack);
		switch (op_index) {
			case Op_multiply: result = left * right; break ;
			case Op_divide: result = left / right; break ;
			case Op_remaining: result = left % right; break ;
			case Op_add: result = left + right; break ;
			case Op_subtract: result = left - right; break ;
			case Op_shift_left: result = left << right; break ;
			case Op_shift_right: result = left >> right; break ;
			case Op_less: result = left < right; break ;
			case Op_greater: result = left > right; break ;
			case Op_less_eq: result = left <= right; break ;
			case Op_greater_eq: result = left >= right; break ;
			case Op_eq: result = left == right; break ;
			case Op_not_eq: result = left != right; break ;
			case Op_bitwise_and: result = left & right; break ;
			case Op_bitwise_xor: result = left ^ right; break ;
			case Op_bitwise_or: result = left | right; break ;
			case Op_logical_and: result = left && right; break ;
			case Op_logical_or: result = left || right; break ;
		}
		// Debug ("evaluating binary '%zd %s %zd', result is %zd", left, op->string, right, result);
		success = push_value (vstack, result);
	}
	return (success);
}

int		evaluate_expression_scope (struct value_stack *vstack, struct op_stack *ostack, const char **ptokens, isize *ret, const struct position *pos);

int		evaluate_expression (const char *tokens, isize *ret, const struct position *pos) {
	int					success;
	struct value_stack	cvstack, *vstack = &cvstack;
	struct op_stack		costack, *ostack = &costack;

	init_value_stack (vstack);
	init_op_stack (ostack);
	success = evaluate_expression_scope (vstack, ostack, &tokens, ret, pos);
	return (success);
}

int		evaluate_expression_scope (struct value_stack *vstack, struct op_stack *ostack, const char **ptokens, isize *ret, const struct position *pos) {
	int					success;
	const char			*tokens = *ptokens;
	int					is_unary = 1;

	success = 1;
	while (success && tokens[-1] && tokens[-1] != Token_newline && !(tokens[-1] == Token_punctuator && 0 == strcmp (tokens, ")"))) {
		if (tokens[-1] == Token_punctuator && 0 == strcmp (tokens, "(")) {
			if (is_unary) {
				Debug ("push %zu %zu", vstack->count, ostack->count);
				success = push_value_stack (vstack);
				success = success && push_op_stack (ostack);
				if (success) {
					tokens = next_const_token (tokens, 0);
					success = evaluate_expression_scope (vstack, ostack, &tokens, ret, pos);
					if (success) {
						pop_op_stack (ostack);
						pop_value_stack (vstack);
						Debug ("pop %zu %zu", vstack->count, ostack->count);
						push_value (vstack, *ret);
						is_unary = 0;
					}
				}
			} else {
				Error_Message (pos, "invalid token");
				success = 0;
			}
		} else if (tokens[-1] == Token_preprocessing_number) {
			if (is_unary) {
				success = push_value (vstack, atoi (tokens));
				is_unary = 0;
			} else {
				Error_Message (pos, "invalid token '%s'", tokens);
				success = 0;
			}
		} else if (tokens[-1] == Token_punctuator) {
			usize	index = 0;

			while (index < Array_Count (ops) && (is_unary != ops[index].is_unary || (is_unary == ops[index].is_unary && 0 != strcmp (tokens, ops[index].string)))) {
				index += 1;
			}
			if (index < Array_Count (ops)) {
				if (!is_unary) {
					while (success && ostack->count > 0 && ops[index].precedence >= ops[view_op (ostack)].precedence) {
						success = evaluate_operator (vstack, ostack);
					}
					is_unary = 1;
				}
				success = push_op (ostack, index);
			} else {
				Error_Message (pos, "unrecognized operator '%s'", tokens);
				success = 0;
			}
		} else {
			Error_Message (pos, "unrecognized token '%s'", tokens);
			success = 0;
		}
		tokens = next_const_token (tokens, 0);
	}
	while (success && ostack->count > 0) {
		success = evaluate_operator (vstack, ostack);
	}
	if (success) {
		if (vstack->count > 0) {
			*ret = pop_value (vstack);
		} else {
			Error ("no value in the stack");
			*ret = 0;
		}
		*ptokens = tokens;
	}
	return (success);
}

#if 0
int main (int args_count, char *args[]) {
	int		success;
	usize	length = 0;
	usize	index = 1;
	char	*string = 0;

	while (index < (usize) args_count) {
		length += strlen (args[index]);
		index += 1;
	}
	if (length > 0) {
		isize	ret = 0;

		string = malloc (length + 1);
		length = 0;
		index = 1;
		while (index < (usize) args_count) {
			length = stpcpy (string + length, args[index]) - string;
			index += 1;
		}
		string[length] = 0;
		char	*tokens = tokenize (string, &(int) {0}, "file.c");
		if (tokens) {
			success = evaluate_expression (tokens, &ret);
			if (success) {
				printf ("%zd\n", ret);
				success = 0;
			} else {
				Error ("something went wrong");
				success = 0;
			}
		} else {
			Error ("cannot tokenize expression");
			success = 0;
		}
	} else {
		Error ("empty expression");
		success = 0;
	}
	return (!success);
}
#endif




