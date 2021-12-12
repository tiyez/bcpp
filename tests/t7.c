
#define __CF_ENUM_GET_MACRO(_1, _2, NAME, ...) NAME

#define __CF_NAMED_ENUM(_type, _name) _type _name; enum
#define __CF_ANON_ENUM(_type) enum
#define CF_CLOSED_ENUM(_type, _name) _type _name; enum
#define CF_OPTIONS(_type, _name) _type _name; enum
#define CF_ENUM(...) __CF_ENUM_GET_MACRO(__VA_ARGS__, __CF_NAMED_ENUM, __CF_ANON_ENUM, )(__VA_ARGS__)

typedef CF_ENUM(CFIndex, CFComparisonResult) {
    kCFCompareLessThan = -1L,
    kCFCompareEqualTo = 0,
    kCFCompareGreaterThan = 1
};

typedef CF_ENUM(CFIndex) {
    kCFCompareLessThan = -1L,
    kCFCompareEqualTo = 0,
    kCFCompareGreaterThan = 1
};
