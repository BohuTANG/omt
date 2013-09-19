#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include "omt.h"
long long get_ustime_sec(void)
{
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec)*1000000;
    ust += tv.tv_usec;
    return ust / 1000000;
}

void _random_key(char *key,int length) {
    int i;
    char salt[36]= "abcdefghijklmnopqrstuvwxyz0123456789";

    for (i = 0; i < length; i++)
        key[i] = salt[rand() % 36];
}

void _bench(int n)
{
    int i;
    char buf[256];
    double cost;
    long long start,end;

    struct slice s;
    struct omt_tree *omt;

    omt = omt_new();

    start = get_ustime_sec();
    for (i = 0; i < n; i++) {
         _random_key(buf, 16);
        s.size = 16;
        s.data = buf;
        omt_insert(omt, &s);
    }
    end = get_ustime_sec();
    cost = end - start;
    printf("write [%d]: %.6f sec/op; %.1f ops/sec(estimated); cost:%.3f(sec)\n",
            n,
            (double)(cost / n),
            (double)(n/ cost),
            cost);

    omt_free(omt);
}

int main(int argc, char *argv[])
{
    int c;

    c = atoi(argv[1]);

    _bench(c);
    return 1;
}
