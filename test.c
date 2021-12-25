


	#define SeqFirst(a, ...) a
	#define SeqSecond(a, b, ...) b
	#define Comma ,
	#define ToString1(a) #a
	#define ToString(a) ToString1 (a)
	#define Concat1(a, b) a ## b
	#define Concat(a, b) Concat1 (a, b)
	#define vformat(...) _Foreach (Concat (m, SeqFirst __ARG__) (ToString (SeqSecond __ARG__)), __VA_ARGS__) _Foreach (Comma Concat (n, SeqFirst __ARG__) (SeqSecond __ARG__), __VA_ARGS__)
	#define printvf(...) printf (vformat (__VA_ARGS__))



	printvf ((m4, view), (m4, proj), (m4, mvp));


