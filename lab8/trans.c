/*
 * MaXiaoqian 520030910288
 */

/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, iOffset, jOffset;
    int tmp1, tmp2, tmp3, tmp4;
    int tmp5, tmp6, tmp7, tmp8;

    /*
     *  M = 32, N = 32, divide the matrix into 8x8 blocks
     *  Every line (include 8 elements) read from matrix A can be fully used
     */
    if(M == 32 && N == 32) {
        for (j = 0; j < 32; j += 8) {
            for (i = 0; i < 32; i++) {
                tmp1 = A[i][j];
                tmp2 = A[i][j + 1];
                tmp3 = A[i][j + 2];
                tmp4 = A[i][j + 3];
                tmp5 = A[i][j + 4];
                tmp6 = A[i][j + 5];
                tmp7 = A[i][j + 6];
                tmp8 = A[i][j + 7];

                B[j][i] = tmp1;
                B[j + 1][i] = tmp2;
                B[j + 2][i] = tmp3;
                B[j + 3][i] = tmp4;
                B[j + 4][i] = tmp5;
                B[j + 5][i] = tmp6;
                B[j + 6][i] = tmp7;
                B[j + 7][i] = tmp8;
            }
        }
    } 

    /*
     *  M = 64, N = 64, also divide the matrix into 8x8 blocks
     *  Firstly transpose the leftTop 4x4 area of A
     *  Store the transposed rightTop 4x4 area of A in B rightTop area
     *  Then transpose the bottom 4x8 area of A in columns
     *  Copy B rightTop area to leftBottom area in rows
     */
    else if(M == 64 && N == 64) {
        for(i = 0; i < 64; i += 8) {
            for(j = 0; j < 64; j += 8) {
                for (iOffset = 0; iOffset < 4; iOffset++) {
                    tmp1 = A[i + iOffset][j];
                    tmp2 = A[i + iOffset][j + 1];
                    tmp3 = A[i + iOffset][j + 2];
                    tmp4 = A[i + iOffset][j + 3];
                    tmp5 = A[i + iOffset][j + 4];
                    tmp6 = A[i + iOffset][j + 5];
                    tmp7 = A[i + iOffset][j + 6];
                    tmp8 = A[i + iOffset][j + 7];

                    B[j][i + iOffset] = tmp1;
                    B[j + 1][i + iOffset] = tmp2;
                    B[j + 2][i + iOffset] = tmp3;
                    B[j + 3][i + iOffset] = tmp4;
                    B[j][i + 4 + iOffset] = tmp5;
                    B[j + 1][i + 4 + iOffset] = tmp6;
                    B[j + 2][i + 4 + iOffset] = tmp7;
                    B[j + 3][i + 4 + iOffset] = tmp8;
                }
                for(jOffset = 0; jOffset < 4; jOffset++) {
                    tmp1 = A[i + 4][j + jOffset];
                    tmp2 = A[i + 5][j + jOffset];
                    tmp3 = A[i + 6][j + jOffset];
                    tmp4 = A[i + 7][j + jOffset];
                    tmp5 = B[j + jOffset][i + 4];
                    tmp6 = B[j + jOffset][i + 5];
                    tmp7 = B[j + jOffset][i + 6];
                    tmp8 = B[j + jOffset][i + 7];
                    B[j + jOffset][i + 4] = tmp1;
                    B[j + jOffset][i + 5] = tmp2;
                    B[j + jOffset][i + 6] = tmp3;
                    B[j + jOffset][i + 7] = tmp4;
                    B[j + jOffset + 4][i + 0] = tmp5;
                    B[j + jOffset + 4][i + 1] = tmp6;
                    B[j + jOffset + 4][i + 2] = tmp7;
                    B[j + jOffset + 4][i + 3] = tmp8;
                    tmp1 = A[i + 4][j + jOffset + 4];
                    tmp2 = A[i + 5][j + jOffset + 4];
                    tmp3 = A[i + 6][j + jOffset + 4];
                    tmp4 = A[i + 7][j + jOffset + 4];
                    B[j + jOffset + 4][i + 4] = tmp1;
                    B[j + jOffset + 4][i + 5] = tmp2;
                    B[j + jOffset + 4][i + 6] = tmp3;
                    B[j + jOffset + 4][i + 7] = tmp4;
                }
            }
        }
    }
    
    /*
     *  M = 61, N = 67, firstly divide the matrix into 8x8 blocks
     *  transpose 56 x 64 part similiar to M = 64, N = 64
     *  then deal with the other part using simple method
     */
    else if(M == 61 && N == 67) {
        for(i = 0; i < 64; i += 8) {
            for(j = 0; j < 56; j += 8) {
                for (iOffset = 0; iOffset < 4; iOffset++) {
                    tmp1 = A[i + iOffset][j];
                    tmp2 = A[i + iOffset][j + 1];
                    tmp3 = A[i + iOffset][j + 2];
                    tmp4 = A[i + iOffset][j + 3];
                    tmp5 = A[i + iOffset][j + 4];
                    tmp6 = A[i + iOffset][j + 5];
                    tmp7 = A[i + iOffset][j + 6];
                    tmp8 = A[i + iOffset][j + 7];
                    B[j][i + iOffset] = tmp1;
                    B[j + 1][i + iOffset] = tmp2;
                    B[j + 2][i + iOffset] = tmp3;
                    B[j + 3][i + iOffset] = tmp4;
                    B[j][i + 4 + iOffset] = tmp5;
                    B[j + 1][i + 4 + iOffset] = tmp6;
                    B[j + 2][i + 4 + iOffset] = tmp7;
                    B[j + 3][i + 4 + iOffset] = tmp8;
                }
                for(jOffset = 0; jOffset < 4; jOffset++) {
                    tmp1 = A[i + 4][j + jOffset];
                    tmp2 = A[i + 5][j + jOffset];
                    tmp3 = A[i + 6][j + jOffset];
                    tmp4 = A[i + 7][j + jOffset];
                    tmp5 = B[j + jOffset][i + 4];
                    tmp6 = B[j + jOffset][i + 5];
                    tmp7 = B[j + jOffset][i + 6];
                    tmp8 = B[j + jOffset][i + 7];
                    B[j + jOffset][i + 4] = tmp1;
                    B[j + jOffset][i + 5] = tmp2;
                    B[j + jOffset][i + 6] = tmp3;
                    B[j + jOffset][i + 7] = tmp4;
                    B[j + jOffset + 4][i + 0] = tmp5;
                    B[j + jOffset + 4][i + 1] = tmp6;
                    B[j + jOffset + 4][i + 2] = tmp7;
                    B[j + jOffset + 4][i + 3] = tmp8;
                    tmp1 = A[i + 4][j + jOffset + 4];
                    tmp2 = A[i + 5][j + jOffset + 4];
                    tmp3 = A[i + 6][j + jOffset + 4];
                    tmp4 = A[i + 7][j + jOffset + 4];
                    B[j + jOffset + 4][i + 4] = tmp1;
                    B[j + jOffset + 4][i + 5] = tmp2;
                    B[j + jOffset + 4][i + 6] = tmp3;
                    B[j + jOffset + 4][i + 7] = tmp4;
                }
            }
        }
        for(i = 0; i < 67; i++) {
            tmp1 = A[i][56];
            tmp2 = A[i][57];
            tmp3 = A[i][58];
            tmp4 = A[i][59];
            tmp5 = A[i][60];
            B[56][i] = tmp1;
            B[57][i] = tmp2;
            B[58][i] = tmp3;
            B[59][i] = tmp4;
            B[60][i] = tmp5;
        }
        for(j = 0; j < 56; j++) {
            tmp1 = A[64][j];
            tmp2 = A[65][j];
            tmp3 = A[66][j];
            B[j][64] = tmp1;
            B[j][65] = tmp2;
            B[j][66] = tmp3;
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

