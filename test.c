


#if 0

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

#endif

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
