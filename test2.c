


#define Token_List /
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


#define BODY(arg) / 111
Hello
#	calleach Token_List /
	(name) Token (name),
	(name,...) Token (name), __VA_ARGS__
#	end
#if 0
World
#else
WORLD##arg
#endif
#end 111


#define Token(x) Token_##x










	#calleach Token_List /
		(name) Token (name),
		(name, value) Token (name) = value,
	#end




BODY(yeah)

#define Shader_Source /-

#version 4.1
int main () {
	return ;
}


#end

#define Stringify(Macro) _Stringify Macro

const char *source = Stringify (Shader_Source);


