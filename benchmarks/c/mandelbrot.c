// gcc -Ofast -flto -o mandelbrot mandelbrot.c -lm
// clang -Ofast -flto -o mandelbrot mandelbrot.c -lm

#include <stdio.h>

#define WIDTH 4096
#define HEIGHT 4096
#define MAX_ITER 1000

int mandelbrot(double cr, double ci) {
    double zr = 0.0;
    double zi = 0.0;
    int n = 0;

    while (n < MAX_ITER) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) break;
        double temp = zr2 - zi2 + cr;
        zi = 2.0 * zr * zi + ci;
        zr = temp;
        n++;
    }
    return n;
}

int main() {
    long sum = 0;

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            double cr = ((double)x / (double)WIDTH) * 3.5 - 2.5;
            double ci = ((double)y / (double)HEIGHT) * 2.0 - 1.0;
            sum += mandelbrot(cr, ci);
        }
    }

    printf("%ld\n", sum);
    return 0;
}
