

#define Token_List ##
	eof 0
	newline 1
	identifier 2
	preprocessing_number 3
	punctuator 4
	string 5
	character 6
	path_global 54
	path_relative 88
	link 99
#end Token_List

#define Shader ##

	int main () {

	}

#end Shader
#define String(x) #x

const char *shader = String (Shader);

#define Token_Prefix Token_
#define Token(token) Token_Prefix##token

enum token {
	#expand(token) Token_List -> Token(token),
};

const char	get_token_name (enum token token) {
	switch (token) {
		#expand(token) Token_List -> case Token_Prefix##token: return #token;
		#expand(token) Token_List ##
			case Token_Prefix##token: {
				return #token;
			} break ;
		#end
	}
}




