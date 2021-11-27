
#define Token(name) Token_##name

#define Token_List ##
(eof)
(newline)
(identifier)
(preprocessing_number)
(punctuator)
(string, 5)
(character)
(path_global)
(path_relative)
(link)
#end

enum token {
	#calleach Token_List ##
		(name) Token (name),
		(name, value) Token (name) = value,
	#end
};

const char	*get_token_name (enum token token) {
	switch (token) {
	#calleach Token_List ##
		(name) ##
		case Token (name): return #name;
		#end
		(name, ...) case Token (name): return #name;
	#end
	}
}




