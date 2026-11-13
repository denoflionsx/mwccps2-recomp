// stale_helper_args: converting a float to an int calls a runtime helper. Once a0 and a1 count as
// arguments of the helpers, the values computed ahead of that call stay out of a1.

struct Tex {
    int u, v, w, h;
};

struct Mask {
    int wu, wv, wi;
    float sy;
    Tex* tex;

    void Before();
    void After();
};

void Mask::Before()
{
    Tex* t = tex;
    int a = wu;
    int b = wv;
    int c = wi;

    t->u = a + b;
    t->v = b + c;
    t->w = c + a;
    t->h = (int)sy;
}

#pragma stale_helper_args(a0, a1)

void Mask::After()
{
    Tex* t = tex;
    int a = wu;
    int b = wv;
    int c = wi;

    t->u = a + b;
    t->v = b + c;
    t->w = c + a;
    t->h = (int)sy;
}
