/* preprocessor: macros, #error in a dead branch, __LINE__, token pasting */
#define CAT(a, b) a##b
#define STR(x) #x
#define TWICE(x) ((x) + (x))
#if 0
#error never
#endif
const char *s = STR(CAT(fo, o)) __FILE__;
int CAT(var, 1) = TWICE(__LINE__);
