/* warnings that print but still produce an object */
#pragma warn_unusedvar on
#pragma warn_implicitconv on
#pragma warn_possunwant on
int h(int a) {
    int unused;
    char c = a;
    if (a = 3) return c;
    return 0;
}
