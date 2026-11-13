// C++: classes, virtuals, templates, exceptions, overloads
struct Base { virtual ~Base() {} virtual int get() const { return 1; } };
struct Der : Base { int v; Der(int x) : v(x) {} int get() const { return v; } };
template <class T> T maxv(T a, T b) { return a < b ? b : a; }
int over(int) { return 1; }
int over(float) { return 2; }
int use(Base &b, int n) {
    Der d(n);
    int r = maxv(b.get(), d.get()) + over(1.0f) + over(n);
    try { if (n < 0) throw n; } catch (int e) { r += e; }
    return r;
}
