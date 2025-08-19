#include<pthread.h>
#include<stdio.h>
#include<sys/time.h>
#include<math.h>
#include<assert.h>

#define N 20000
int D[N][N];

int _sum = 0;
struct timeval _tv0;
#define _tim gettimeofday(&_tv0, NULL)
#define _start (_tim, _sum -= (1000000*_tv0.tv_sec + _tv0.tv_usec));
#define _stop  (_tim, _sum += (1000000*_tv0.tv_sec + _tv0.tv_usec));

int f(int i, int j) {
    volatile double xd = i;
    volatile double yd = j;
    double d = ceil(sqrt(xd*xd+yd*yd));
    return static_cast<int>(d);
}

int n;

void *thread(void *ptr) {
    int id = *static_cast<int*>(ptr);
    for (int i=id; i < N; i+=n)
        for (int j=0; j < N; ++j)
            D[i][j] = f(i, j);
    return NULL;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    n = atoi(argv[1]);
    assert(1 <= n && n <= 4);
    pthread_t thread1, thread2, thread3, thread4;
    int thr = 0;
    int thr2 = 1;
    int thr3 = 2;
    int thr4 = 3;

_start
    pthread_create(&thread1, NULL, *thread, static_cast<void *>(&thr));
    if (n >= 2) pthread_create(&thread2,
                               NULL, *thread, static_cast<void *>(&thr2));
    if (n >= 3) pthread_create(&thread3,
                               NULL, *thread, static_cast<void *>(&thr3));
    if (n >= 4) pthread_create(&thread4,
                               NULL, *thread, static_cast<void *>(&thr4));

    pthread_join(thread1, NULL);
    if (n >= 2) pthread_join(thread2, NULL);
    if (n >= 3) pthread_join(thread3, NULL);
    if (n >= 4) pthread_join(thread4, NULL);
_stop
    printf("%dus\n", _sum);

    return 0;
}
