
void *memcpy(void *d, void *s, unsigned long size)
{
    char *sp = s;
    char *dp = d;
    for (unsigned long i = 0; i < size; ++i)
        dp[i] = sp[i];
    return d;
}
