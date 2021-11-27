
#ifndef Def_H
#define Def_H

// #include <stdio.h>

#define println(...) do { printf (__VA_ARGS__); printf ("\n"); } while (0)

// #define No_Error_Messages
// #define No_Debug_Messages
// #define No_Debug_Code

#if defined(No_Error_Messages) || defined(No_Messages)
#	define Error(...)
#else
#	define Error(...) \
do {fprintf (stderr, "Error:%s:%d: ", __func__, __LINE__);\
	fprintf (stderr, __VA_ARGS__);\
	fprintf (stderr, "\n");\
} while (0)
#endif

#if defined(No_Debug_Messages) || defined(No_Messages)
#	define Debug(...)
#else
#	define Debug(...) \
do {fprintf (stderr, "Debug:%s:%d: ", __func__, __LINE__);\
	fprintf (stderr, __VA_ARGS__);\
	fprintf (stderr, "\n");\
} while (0)
#endif

#if defined No_Debug_Code || defined No_Debug
#	define Debug_Code(...)
#else
#	define Debug_Code(...) __VA_ARGS__
#endif

#define System_Error_Message(pos, ...) \
do {fprintf (stderr, "%s:%d:%d: system error at %s:%d:", (pos)->filename, (pos)->line, (pos)->column, __func__, __LINE__);\
	fprintf (stderr, __VA_ARGS__);\
	fprintf (stderr, "\n");\
} while (0)

#define Error_Message(pos, ...) \
do {fprintf (stderr, "%s:%d:%d: error: ", (pos)->filename, (pos)->line, (pos)->column);\
	fprintf (stderr, __VA_ARGS__);\
	fprintf (stderr, "\n");\
} while (0)

#define Error_Message_p(filename, line, column, ...) \
do {fprintf (stderr, "%s:%d:%d: error: ", filename, line, column);\
	fprintf (stderr, __VA_ARGS__);\
	fprintf (stderr, "\n");\
} while (0)

#include <assert.h>
#define Assert(expr) assert (expr)

#define Member_Offset(type, memb) (&(((type *)0)->memb))
#define Array_Count(arr) (sizeof (arr) / sizeof ((arr)[0]))

#define GL(v) (v); if ((error = glGetError())) do { Error ("GL Error! %d", error); exit (1); } while (0)

#define zeroed(ptr) memset (ptr, 0, sizeof *(ptr))
#define zeroed_array(arr) memset (arr, 0, sizeof (arr))

#define Invalid_Shader_Object (0)
#define Invalid_Shader_Program (0)

#define Min(a,b) ((a) < (b) ? a : b)
#define Max(a,b) ((a) > (b) ? a : b)

#endif /* Def_H */
