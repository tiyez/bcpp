


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


#define BODY ## 111
Hello
#	calleach Token_List ##
	(name) Token (name),
#	end
#if 1
World
#else
WORLD
#endif
#end 111


#define Token(x) Token_##x










	#calleach Token_List ##
		(name) Token (name),
		(name, value) Token (name) = value,
	#end




BODY





