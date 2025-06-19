// -O3 -flto

#include <stdio.h>

#define N 150000000

static int spf[N+1] = {0};
static int d[N+1] = {0};
static int e[N+1] = {0};
static int primes[N+1];

int main() {
    int pcount = 0;
    
    d[1] = 1;
    e[1] = 0;
    spf[1] = 0;
    
    for (int i = 2; i <= N; i++) {
        if (spf[i] == 0) {
            spf[i] = i;
            d[i] = 2;
            e[i] = 1;
            primes[pcount++] = i;
        } else {
            int p = spf[i];
            int j = i / p;
            if (spf[j] == p) {
                e[i] = e[j] + 1;
                d[i] = d[j] * (e[i] + 1) / (e[j] + 1);
            } else {
                e[i] = 1;
                d[i] = d[j] * 2;
            }
        }

        for (int j_index = 0; j_index < pcount; j_index++) {
            int p = primes[j_index];
            if (p > spf[i] || (long long)p * i > N) 
                break;
            spf[p * i] = p;
        }
    }
    
    unsigned long long total = 0;
    for (int i = 1; i <= N; i++) {
        total += d[i];
    }
    printf("%llu\n", total);
    return 0;
}
