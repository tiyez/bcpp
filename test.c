

#define CAT(a, b) pp ## oo

CAT(hello, world)

#define __ENUM(name, type, ...) enum { __VA_ARGS__ }; typedef type name##_t

__ENUM(hello, unsigned int, wo, ha, di);


