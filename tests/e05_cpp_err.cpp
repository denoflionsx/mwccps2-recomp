// C++ errors: ambiguous overload, access, missing member
class A { int priv; public: void f(int); void f(long); };
void t(A &a) { a.f(1.0); a.priv = 2; a.nope(); }
