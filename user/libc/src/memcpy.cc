
void *memcpy(void *d, void *s, unsigned long size)
{
	char *sp = (char*)s;
	char *dp = (char*)d;
    for (unsigned long i = 0; i < size; ++i)
        dp[i] = sp[i];
    return d;
}
