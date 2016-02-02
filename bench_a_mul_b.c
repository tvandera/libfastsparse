#include <stdio.h>
#include <stdlib.h>
#include "sparse.h"
#include "timing.h"

void usage() {
  printf("Usage:\n");
  printf("  bench_a_mul_c <matrix_file> [block_size] [-t]\n");
  printf("  -t  transpose matrix\n");
}

void extrema(int* x, long n, int* min, int* max) {
  *min = x[0];
  *max = x[0];

  for (int i = 0; i < n; i++) {
    if (x[i] < min[0]) *min = x[i];
    if (x[i] > max[0]) *max = x[i];
  }
}

/** returns -1 if A is hilbert sorted
 *  otherwise idx of the first non-increasing elemetn */
long check_if_sorted(struct SparseBinaryMatrix *A) {
  int n = ceilPower2(A->nrow > A->ncol ? A->nrow : A->ncol);
  long h = xy2d(n, A->rows[0], A->cols[0]);
  for (long j = 1; j < A->nnz; j++) {
    long h2 = xy2d(n, A->rows[j], A->cols[j]);
    if (h2 <= h) return j;
    h = h2;
  }
  return -1;
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("Need matrix file name.\n");
    usage();
    exit(1);
  }
  char* filename = argv[1];
  int block_size = 1024;
  int t = 0;
  if (argc >= 3) {
    block_size = atoi(argv[2]);
  }
  if (argc >= 4) {
    if (strcmp("-t", argv[3]) == 0) {
      t = 1;
    }
  }
  int nrepeats = 10;
  int cgrepeats = 100;

  printf("Benchmarking A*x with '%s'.\n", filename);
  struct SparseBinaryMatrix* A = read_sbm(filename);
  if (t) {
    transpose(A);
  }
  printf("Size of A is %d x %d.\n", A->nrow, A->ncol);
  printf("Number of repeats = %d\n", nrepeats);
  printf("Number of CG repeats = %d\n", cgrepeats);

  double* y  = (double*)malloc(A->nrow * sizeof(double));
  double* y2 = (double*)malloc(A->nrow * sizeof(double));
  double* x  = (double*)malloc(A->ncol * sizeof(double));

  double* Y  = (double*)malloc(2 * A->nrow * sizeof(double));
  double* X  = (double*)malloc(2 * A->ncol * sizeof(double));

  double* Y4  = (double*)malloc(4 * A->nrow * sizeof(double));
  double* X4  = (double*)malloc(4 * A->ncol * sizeof(double));

  //double* Y8  = (double*)malloc(8 * A->nrow * sizeof(double));
  //double* X8  = (double*)malloc(8 * A->ncol * sizeof(double));

  for (int i = 0; i < A->ncol; i++) {
    x[i] = sin(7.0*i + 0.3);
    X[i*2] = sin(7.0*i + 0.3);
    X[i*2+1] = sin(11*i - 0.2);
    for (int k = 0; k < 4; k++) {
      X4[i*4+k] = sin(7*i + 17*k + 0.3);
    }
    //for (int k = 0; k < 8; k++) {
    //  X8[i*8+k] = sin(7*i + 17*k + 0.3);
    //}
  }

  double wall_start, cpu_start;
  double wall_stop,  cpu_stop;

  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    A_mul_B(y, A, x);
  }
  timing(&wall_stop, &cpu_stop);

  printf("[unsorted]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);

  // sorting sbm
  int max_row = -1, min_row = -1;
  int max_col = -1, min_col = -1;
  extrema(A->rows, A->nnz, &min_row, &max_row);
  extrema(A->cols, A->nnz, &min_col, &max_col);

  sort_sbm(A);

  extrema(A->rows, A->nnz, &min_row, &max_row);
  extrema(A->cols, A->nnz, &min_col, &max_col);

  // making sure sbm contents is within bounds
  for (int j = 0; j < A->nnz; j++) {
    if (A->rows[j] < 0 || A->rows[j] >= A->nrow) {
      printf("rows[%d] = %d\n", j, A->rows[j]);
      return(1);
    }
    if (A->cols[j] < 0 || A->cols[j] >= A->ncol) {
      printf("cols[%d] = %d\n", j, A->cols[j]);
      return(1);
    }
  }
  A_mul_B(y2, A, x);
  for (int j = 0; j < A->nrow; j++) {
    if (abs(y2[j] - y[j]) > 1e-6) {printf("y2[%d]=%f, y[%d]=%f\n", j, y2[j], j, y[j]); return 1;}
  }

  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    A_mul_B(y, A, x);
  }
  timing(&wall_stop, &cpu_stop);

  printf("[sort]  \tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);

  ////// Blocked SBM //////
  printf("Block size = %d\n", block_size);
  struct BlockedSBM* B = new_bsbm(A, block_size);
  struct SparseBinaryMatrix* At = new_transpose(A);
  struct BlockedSBM* Bt = new_bsbm(At, block_size);
  bsbm_A_mul_B(y2, B, x);
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    bsbm_A_mul_B(y, B, x);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[block]\t\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);

  ////// Blocked SBM 2x //////
  bsbm_A_mul_B2(Y, B, X);
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    bsbm_A_mul_B2(Y, B, X);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[2xblock]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);
  
  ////// Blocked SBM 2x* //////
  bsbm_A_mul_B2(Y, B, X);
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    bsbm_A_mul_Bn(Y, B, X, 2);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[2xblock*]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);

  /////// CG with x //////
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < cgrepeats; i++) {
    bsbm_A_mul_B(y, B,  x);
    bsbm_A_mul_B(x, Bt, y);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[cg]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / cgrepeats, (cpu_stop - cpu_start)/cgrepeats);

  /////// CG with X //////
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < cgrepeats; i++) {
    bsbm_A_mul_B2(Y, B,  X);
    bsbm_A_mul_B2(X, Bt, Y);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[cg2]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / cgrepeats, (cpu_stop - cpu_start)/cgrepeats);


  ////// Blocked SBM 4x //////
  bsbm_A_mul_B4(Y4, B, X4);
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    bsbm_A_mul_B4(Y4, B, X4);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[4xblock]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);

  ////// Blocked SBM 8x //////
  /*
  bsbm_A_mul_Bn(Y8, B, X8, 8);
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    bsbm_A_mul_Bn(Y8, B, X8, 8);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[8xblock]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);
  */

  ////// Sort each block ///////
  sort_bsbm(B);
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    bsbm_A_mul_B(y, B, x);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[sort+block]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);

  ////// Sort by row ///////
  sort_bsbm_byrow(B);
  timing(&wall_start, &cpu_start);
  for (int i = 0; i < nrepeats; i++) {
    bsbm_A_mul_B(y, B, x);
  }
  timing(&wall_stop, &cpu_stop);
  printf("[rowsort+block]\tWall: %0.5e\tcpu: %0.5e\n", (wall_stop - wall_start) / nrepeats, (cpu_stop - cpu_start)/nrepeats);
}
