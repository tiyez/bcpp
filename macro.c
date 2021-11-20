




enum preproc_mode {
	Preproc_Mode_idle,
	Preproc_Mode_active,
};

enum preproc_state {
	Preproc_State_invalid,
	Preproc_State_directive,
};

struct preproc {
	enum preproc_mode	mode;
	enum preproc_state	state;
};



int		step_preproc_token (struct preproc *pp, const char *token, const struct position *pos) {
	int		success;

	if (pp->mode == Preproc_Mode_idle) {
		if (pos->at_the_beginning && is_token (token, Token_punctuator, "#")) {
			pp->mode = Preproc_Mode_active;
			pp->state = Preproc_State_directive;
		}
		success = 1;
	} else if (pp->mode == Preproc_Mode_active) {
		if (pp->state == Preproc_State_directive) {
			if (0 == strcmp (token, "if")) {
				
			}
		}
	}
	return (success);
}






