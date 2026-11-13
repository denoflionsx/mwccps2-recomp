void callee(float, float, float, float);

void plain(float a)
{
    callee(a, 1.0f, 2.0f, 3.0f);
}

void marked(float a)
{
#pragma stale_hascall(1, 0, 1)
    callee(a, 1.0f, 2.0f, 3.0f);
}

void all_on(float a)
{
#pragma stale_hascall on
    callee(a, 1.0f, 2.0f, 3.0f);
#pragma stale_hascall reset
}

void all_off(float a)
{
#pragma stale_hascall off
    callee(a, 1.0f, 2.0f, 3.0f);
#pragma stale_hascall reset
}
