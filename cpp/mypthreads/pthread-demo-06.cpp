#include <ctime>
#include <vector>
#include <iostream>
using namespace std;

#include <unistd.h>
#include <pthread.h>

#define MAX_NUM 10000

int numList[MAX_NUM];
int readIdx = 0;
int readCount = 0;

void* pushFunc(void*) {
    for (int i = 0; i < MAX_NUM; i++) {
        numList[i] = i;
    }
}

void* popFunc(void*) {
    while (readCount < MAX_NUM) {
        if (readIdx < MAX_NUM) {
            numList[readIdx] = -1;
            readIdx ++;
            readCount ++;
        }
    }
}

int main()
{
    pthread_t threadIds[4];

    int ret = pthread_create(&threadIds[0], NULL, pushFunc, NULL);
    if (ret) {
        perror("create push thread error");
        exit(-1);
    }
    for (int i = 1; i < 4; i++) {
        ret = pthread_create(&threadIds[i], NULL, popFunc, NULL);
        if (ret) {
            perror("create pop thread error");
            exit(-i);
        }
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threadIds[i], NULL);
    }
    cout << "final count = " << readCount << endl;
    return 0;
}