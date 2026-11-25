// name_counter: the string literals of the second function get the numbers the list states.

#pragma name_counter 700 after 3
#pragma name_counter 900 after 7

extern void use(const char*);

void First()
{
    use("one");
    use("two");
}

void Second()
{
    use("three");
    use("four");
}
