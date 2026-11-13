#line 2 "D:\\work\\game\\source\\main.cpp"
#include "line_head.h"

int outside(int);

int first(int a)
{
    return in_header(a) + outside(a);
}

#line 40 "D:\\work\\game\\source\\other.cpp"
int second(int a)
{
    return outside(a) + 1;
}
