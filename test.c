
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

#introspection
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

#define STRING1(x) #x
#define STRING(x) STRING1(x)

STRING(Token_List)

#introspection
struct tokenizer {
	char	*start_data;
	char	*data;
	usize	size;
	usize	cap;
	char	*current;
	char	*prev;
};

_Calleach _Introspection(tokenizer) ##
	(typename, name) { offsetof (tokenizer, name) }

_Introspect (tokenizer, start_data, _IOffset)
_Introspect (tokenizer, data, _IName)
_Introspect (tokenizer, current, _ITypename)
_Introspect (token, Token (punctuator), _IValue)
