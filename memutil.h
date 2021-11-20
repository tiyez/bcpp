

#ifndef Header_memutil
#define Header_memutil

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "def.h"

typedef size_t		usize;
typedef ptrdiff_t	isize;
typedef int8_t		i8;
typedef uint8_t		u8;
typedef int16_t		i16;
typedef uint16_t	u16;
typedef int32_t		i32;
typedef uint32_t	u32;
typedef int64_t		i64;
typedef uint64_t	u64;
typedef float		r32;
typedef double		r64;

typedef int8_t		int8;
typedef uint8_t		uint8;
typedef int16_t		int16;
typedef uint16_t	uint16;
typedef int32_t		int32;
typedef uint32_t	uint32;
typedef int64_t		int64;
typedef uint64_t	uint64;
typedef float		real32;
typedef double		real64;

#define Is_Power_Of_Two(X) ((!((X)&((X)-1)))&&(X))

#define Memory_Page_Bit 12
#define Memory_Page (1 << Memory_Page_Bit)

#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFllu
#	define Is_Memory_Size_64 1
#else
#	define Is_Memory_Size_64 0
#endif

inline usize	to_power_of_two (usize val)
{
	val--;
	val |= val >> 1;
	val |= val >> 2;
	val |= val >> 4;
	val |= val >> 8;
	val |= val >> 16;
#if Is_Memory_Size_64
	val |= val >> 32;
#endif
	val++;
	return (val);
}

inline usize	to_page_size (usize val)
{
	return ((val + Memory_Page - 1) & ~(Memory_Page - 1));
}

inline usize	get_aligned_value (usize value, usize alignment)
{
	return ((value + alignment - 1) & ~(alignment - 1));
}

inline usize	get_alignment_diff (usize value, usize alignment)
{
	return (((value + alignment - 1) & ~(alignment - 1)) - value);
}

inline void		*expand_array (void *array, usize *size) {
	void	*new_array;
	usize	new_size;

	new_size = *size ? *size * 2 : Memory_Page;
	new_array = realloc (array, new_size);
	if (new_array) {
		*size = new_size;
	} else {
		Error ("cannot expand array");
	}
	return (new_array);
}

#endif /* Header_memutil */


#if (defined(Implementation_memutil) || defined(Implementation_All)) && !defined(Except_Implementation_memutil) && !defined(Implemented_memutil)
#define Implemented_memutil

usize	to_power_of_two (usize val);
usize	to_page_size (usize val);
usize	get_aligned_value (usize value, usize alignment);
usize	get_alignment_diff (usize value, usize alignment);
void	*expand_array (void *array, usize *size);

#endif /* defined(Memutil_Implementation) || defined(All_Implementation) */
