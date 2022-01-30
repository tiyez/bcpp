
#if 0

#if 1

#define Sprite_Quad_Z /
(0,0,0, 0,1,0, 1,1,0, 1,0,0)
#end

#define Cube_Quads /
(0,0,0, 0,1,0, 1,1,0, 1,0,0)
(0,0,1, 1,0,1, 1,1,1, 0,1,1)
(0,0,0, 0,0,1, 0,1,1, 0,1,0)
(1,0,0, 1,1,0, 1,1,1, 1,0,1)
(0,0,0, 1,0,0, 1,0,1, 0,0,1)
(0,1,0, 0,1,1, 1,1,1, 1,1,0)
#end

#define Quad_To_Triangle_Map(x1,y1,z1, x2,y2,z2, x3,y3,z3, x4,y4,z4) /
(x1, y1, z1)
(x2, y2, z2)
(x3, y3, z3)
(x1, y1, z1)
(x3, y3, z3)
(x4, y4, z4)
#end

const struct vertex vertices_cube[] = {
	#define Repos(X) ((X) - 0.5f)
	#define Quad(c1,c2,c3, ...) /
#calleach Quad_To_Triangle_Map (__VA_ARGS__) @ (x, y, z) { { Repos (x), Repos (y), Repos (z), }, { c1, c2, c3, 1, } },
	#end

	#calleach Cube_Quads @ (...) Quad (_Index1 <= 1, _Index1 >= 2 && _Index1 <= 3, _Index1 >= 4 && _Index1 <= 5, __VA_ARGS__)

	#undef Quad
	#undef Repos
};






	#calleach Cube_Quads @ (...) /
#calleach Quad_To_Triangle_Map (__VA_ARGS__) @ (X, Y, Z) / 1
	v3_l ((*verticies)->position, x + X, y + Y, z + Z);
	v4_l ((*verticies)->color, _Index1 <= 1, _Index1 >= 2 && _Index1 <= 3, _Index1 >= 4 && _Index1 <= 5, 1);
	(*verticies) += 1;
#end 1
	#end

#else

#define Sprite_Quad_Z(z) (0,0,z, 0,1,z, 1,1,z, 1,0,z)
#define Sprite_Quad_X(x) (x,0,0, x,0,1, x,1,1, x,1,0)
#define Sprite_Quad_Y(y) (0,y,0, 1,y,0, 1,y,1, 0,y,1)
#define Flip_Quad(x1,y1,z1, x2,y2,z2, x3,y3,z3, x4,y4,z4) (x1,y1,z1, x4,y4,z4, x3,y3,z3, x2,y2,z2)

Sprite_Quad_Z (0)
Flip_Quad Sprite_Quad_Z (1)
Sprite_Quad_X (0)
Flip_Quad Sprite_Quad_X (1)
Sprite_Quad_Y (0)
Flip_Quad Sprite_Quad_Y (1)

ss

#endif


#calleach (1, 2) @ /
(A) 1
(A, B) 2
(A, B, ...) 3 va
(A, B, C) 3 non-va
#end

#define Concat1(A, B) A ## B
#define Concat(A, B) Concat1 (A, B)


// Concat (A, B)
Concat (A, B)

// Concat (A, B, C)
Concat (A, Concat (B, C))

// Concat (A, B, C, D)
Concat (A, Concat (B, Concat (C, D)))



#define Concat2(A, B, ...) /!
#calleach (A, B, ## __VA_ARGS__) @ / 1
(a, b) Concat1 (a, b)
(a, b, ...) Concat2 (a, Concat2 (b, __VA_ARGS__))
#end 1
#end

// Concat2 (A, B)
Concat2 (A, B)

// Concat2 (A, B, C)
Concat2 (A, Concat2 (B, C))

// Concat2 (A, B, C, D)
Concat2 (A, Concat2 (B, Concat2 (C, D)))

Concat2 (A, B, C, D)

#endif

#if 0

#define Concat1(A, B) A ## B

#define Concat(A, B, ...) /^
#calleach (A, B, ## __VA_ARGS__) @ / 1
(a, b) Concat1 (a, b)
(a, b, ...) Concat (a, Concat (b, _Va_Args))
#end 1
#end

#define Call(Fn, Params) Fn Params

#define SeqFirst(a, ...) a
#define SeqSecond(a, b, ...) b
#define Comma ,
#define Make_String1(a) #a
#define Make_String(a) Make_String1 (a)

#define Assert_Macro(!name) /
#ifndef name
#error name must be defined!
#endif
#end

#define List_Enum_Name(!list) list ## _enum_name
#define List_Params(!list) list ## _params

#define Make_Enum_From_List(!list, name, !body) /
#define list ## _enum_name name
enum name {
#calleach list @ _Eval (List_Params (list)) body,
}
#end

#define Make_Enum_Name_Getter(!list, !selector) /
Assert_Macro (_Eval !(List_Enum_Name (list)))
Assert_Macro (_Eval !(List_Params (list)))
const char	*(Concat (get_, List_Enum_Name (list), _name)) (enum List_Enum_Name (list) value) {
	switch (value) {
#calleach list @ _Eval (List_Params (list)) return (Make_String (selector));
	}
	return ("<invalid>");
}
#end

#define DEFINE(!name) /
#define name ## _hello world
#end

#define bye yell
DEFINE (bye)
Assert_Macro (bye_hello)


#define Primitive_Type(name) Concat (Primitive_Type_, name)
#define Primitive_Types_params (name, value)
#define Primitive_Types /
(default, 0) /* triangles */
(points, GL_POINTS)
(lines, GL_LINES)
(line_strip, GL_LINE_STRIP)
(line_loop, GL_LINE_LOOP)
(triangles, GL_TRIANGLES)
(triangle_strip, GL_TRINALGE_STRIP)
(triangle_fan, GL_TRIANGLE_FAN)
#end

Make_Enum_From_List (Primitive_Types, primitive_type, Primitive_Type (name) = value)
Make_Enum_Name_Getter (Primitive_Types, Primitive_Type (name))


#endif

#if 0

#define COMMA ,

#define List_Enum_Name(!list) list ## _enum_name

#define Make_Enum_Generic(!list, enum_name, !...) /
#define _Eval !(List_Enum_Name (list)) enum_name
enum enum_name {
#calleach list @ / 1
_Eval !(_Foreach (__ARG__ COMMA, __VA_ARGS__))
(...)
#end 1
};
#end


#define List /
(hello)
(world, 1)
(some, other, case)
#end

Make_Enum_Generic (List, enumerator, (name) name, (name, value) name = value)

#endif

#define Make_String1(!a, !b) a ## b
#define Make_String(!a, !b) Make_String1(a, b)

#define Hello(!...) Make_String1(__VA_ARGS__)

#define A a
#define B b

Hello(A, B)










