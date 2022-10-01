//
// Created by McRebel on 19.09.2022.
//

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define A_DIMX 10
#define B_DIMY 12
#define I_DIM  20

#define I2D_TO_1D(x, y, c) (x)*(c)+(y)

typedef struct {
    int xi;
    int yi;
    int thread_id;
    int* A;
    int* B;
    int* C;
} thread_data;

int *A, *B, *C;

void* thread_func(void* arg) {
    thread_data* info = (thread_data*)arg;
    int sum = 0;
    for (int i = 0; i < I_DIM; i++) {
        int ai = I2D_TO_1D(info->xi, i, I_DIM);
        int bi = I2D_TO_1D(i, info->yi, B_DIMY);
        sum += A[ai] * B[bi];
    }
    C[I2D_TO_1D(info->xi,info->yi,B_DIMY)] = sum;
    // printf("ThreadId = %d \t", pthread_self());
    return NULL;
}


void printMatrix(int* C, int xmax, int ymax) {
    //print Matrix
    for (int xi = 0; xi < xmax; xi++) {
        for (int yi = 0; yi < ymax; yi++) {
            printf("%d\t", C[I2D_TO_1D(xi,yi,ymax)]);
        }
        printf("\n");
    }
    //minValue maxValue and Sum
    int myminvalue = 100000;
    int mymaxvalue = 0;
    int mysum = 0;
    for (int xi = 0; xi < xmax; xi++) {
        for (int yi = 0; yi < ymax; yi++) {
            mysum += C[I2D_TO_1D(xi, yi,ymax)];
            if(C[I2D_TO_1D(xi, yi,ymax)] < myminvalue){
                myminvalue = C[I2D_TO_1D(xi, yi,ymax)];
            }
            if(C[I2D_TO_1D(xi, yi,ymax)] > mymaxvalue){
                mymaxvalue = C[I2D_TO_1D(xi, yi,ymax)];
            }
        }
    }
    printf("Minimaler Wert: %d;", myminvalue);
    printf(" Maximaler Wert: %d;", mymaxvalue);
    printf(" Summe aller Werte: %d", mysum);


}

int main() {
    A = (int*)malloc(A_DIMX*I_DIM*sizeof(int));
    for (int i = 0; i < A_DIMX*I_DIM; i++) {
        A[i] = rand() % 10;
    }
    B = (int*)malloc(B_DIMY*I_DIM*sizeof(int));
    for (int i = 0; i < B_DIMY*I_DIM; i++) {
        B[i] = rand() % 10;
    }
    C = (int*)malloc(A_DIMX*B_DIMY*sizeof(int));

    pthread_t threads[A_DIMX*B_DIMY];
    thread_data infos[A_DIMX * B_DIMY];
    for (int xi = 0; xi < A_DIMX; xi++) {
        for (int yi = 0; yi < B_DIMY; yi++) {
            int i = I2D_TO_1D(xi,yi,B_DIMY);
            infos[i].xi=xi;
            infos[i].yi=yi;
            infos[i].thread_id=i;
            infos[i].A=A;
            infos[i].B=B;
            infos[i].C=C;
            pthread_create(&threads[i], NULL, thread_func, &infos[i]);

        }
    }

    for (int i = 0; i < A_DIMX*B_DIMY; i++) {
        pthread_join(threads[i], NULL);
    }



    printf("\nErgebnismatrix C:\n");
    printMatrix(C, A_DIMX, B_DIMY);
    free(A);
    free(B);
    free(C);

    return 0;
}
