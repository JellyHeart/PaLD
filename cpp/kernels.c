#include "kernels.h"

// linear indexing function assuming column major
inline int lin(int i, int j, int n) { return i + j * n; }

/*
params
D    in  distance matrix: D(x,y) is distance between x and y (symmetric)
beta in  conflict focus parameter: z is in focus of (x,y) if
         min(d(z,x),d(z,y)) <= beta * d(x,y)
n    in  number of points
C    out cohesion matrix: C(x,z) is z's support for x
*/
void pald_orig(float *D, float beta, int n, float *C) {
    // input checking
    if (beta < 0)
        fprintf(stderr, "beta must be positive\n");

    // loop over pairs of points x and y (only for x < y)
    for (int x = 0; x < n - 1; x++)
        for (int y = x + 1; y < n; y++) {
            int cfs = 0;                // conflict focus size of x,y
            float dxy = D[lin(x, y, n)]; // distance between x and y

            // loop over all points z to determine conflict focus size
            for (int z = 0; z < n; z++) {
                if (D[lin(z, x, n)] <= beta * dxy || D[lin(z, y, n)] <= beta * dxy)
                    cfs++;
            }

            // loop over all points z to determine contributions to x or y
            for (int z = 0; z < n; z++) {
                float dzx = D[lin(z, x, n)]; // dist between z and x
                float dzy = D[lin(z, y, n)]; // dist between z and y

                // z contributes to x or y only if in conflict focus
                if (dzx <= beta * dxy || dzy <= beta * dxy) {
                    if (dzx < dzy)
                        C[lin(x, z, n)] += 1.0 / cfs; // z closer to x than y
                    else if (dzy < dzx)
                        C[lin(y, z, n)] += 1.0 / cfs; // z closer to y than x

                    else {
                        // z equidistant to x and y
                        C[lin(x, z, n)] += 0.5 / cfs;
                        C[lin(y, z, n)] += 0.5 / cfs;
                    }
                }
            }
        }
}

void pald_orig_par(float *D, float beta, int n, float *C, int t) {
    // input checking
    if (beta < 0)
        fprintf(stderr, "beta must be positive\n");

    // loop over pairs of points x and y (only for x < y)
    for (int x = 0; x < n - 1; x++)
        for (int y = x + 1; y < n; y++) {
            int cfs = 0;                // conflict focus size of x,y
            float dxy = D[lin(x, y, n)]; // distance between x and y

            // loop over all points z to determine conflict focus size
	    #pragma omp parallel for num_threads(t) reduction(+:cfs)
            for (int z = 0; z < n; z++) {
                if (D[lin(z, x, n)] <= beta * dxy || D[lin(z, y, n)] <= beta * dxy)
                    cfs++;
            }

            // loop over all points z to determine contributions to x or y
	    #pragma omp parallel for num_threads(t)
            for (int z = 0; z < n; z++) {
                float dzx = D[lin(z, x, n)]; // dist between z and x
                float dzy = D[lin(z, y, n)]; // dist between z and y

                // z contributes to x or y only if in conflict focus
                if (dzx <= beta * dxy || dzy <= beta * dxy) {
                    if (dzx < dzy)
                        C[lin(x, z, n)] += 1.0 / cfs; // z closer to x than y
                    else if (dzy < dzx)
                        C[lin(y, z, n)] += 1.0 / cfs; // z closer to y than x

                    else {
                        // z equidistant to x and y
                        C[lin(x, z, n)] += 0.5 / cfs;
                        C[lin(y, z, n)] += 0.5 / cfs;
                    }
                }
            }
        }
}
/*
params
D    in  distance matrix: D(x,y) is distance between x and y (symmetric,
         but assumed to be stored in full)
beta in  conflict focus parameter: z is in focus of (x,y) if
         min(d(z,x),d(z,y)) <= beta * d(x,y)
n    in  number of points
C    out cohesion matrix: C(x,z) is z's support for x
b    in  blocking parameter for cache efficiency
*/
void pald_opt(float *D, float beta, int n, float *C, const int b) {
    // declare indices
    int x, y, z, i, j, xb, yb, ib;

    // pre-allocate conflict focus and distance cache blocks
    int *UXY = (int *) malloc(b * b * sizeof(int));
    float *DXY = (float *) malloc(b * b * sizeof(float));

    // initialize pointers for cache-block subcolumn vectors
    float *DXz, *DYz, *CXz, *CYz;

    // loop over blocks of points Y = (y,...,y+b-1)
    for (y = 0; y < n; y += b) {
        // define actual block size (for corner cases)
        yb = (b < n - y ? b : n - y);

        // loop over blocks of points X = (x,...,x+b-1)
        for (x = 0; x <= y; x += b) {
            // define actual block size (for corner cases)
            xb = (b < n - x ? b : n - x);

            // copy distances into cache block one column at a time
            for (j = 0; j < yb; j++) {
                // DXY(:,j) = D(x:x+xb,y+j) in off-diagonal case
                ib = (x == y ? j : xb); // handle diagonal blocks
                memcpy(DXY + j * xb, D + x + (y + j) * n, ib * sizeof(float));
            }

            // DEBUG: print out DXY cache block
            /*printf("x %d y %d xb %d yb %d\n",x,y,xb,yb);
            printf("\nDXY\n");
            for (int i = 0; i < xb; i++)
            {
                for (int j = 0; j < yb; j++)
                {
                    printf("%f ", DXY[i+j*xb]);
                }
                printf("\n");
            }
            printf("\n");*/

            // compute block's conflict focus sizes by looping over all points z
            memset(UXY, 0, b * b * sizeof(int)); // clear old values
            DXz = D + x;
            DYz = D + y; // init pointers to subcolumns of D
            for (z = 0; z < n; z++) {
                // loop over all (i,j) pairs in block
                for (j = 0; j < yb; j++) {
                    ib = (x == y ? j : xb); // handle diagonal blocks
                    for (i = 0; i < ib; i++)
                        // DXY[i+j*xb] is distance between x+i and y+j
                        // DXz[i] is distance between x+i and z
                        // DYz[j] is distance between y+j and z

                        // determine if z is in conflict focus of x+i and y+j
                        if (DYz[j] <= beta * DXY[i + j * xb] || DXz[i] <= beta * DXY[i + j * xb])
                            UXY[i + j * xb]++;
                }

                // update pointers to subcolumns of D
                DXz += n;
                DYz += n;
            }

            // DEBUG: print out UXY cache block
            /*for (int i = 0; i < xb; i++)
            {
                for (int j = 0; j < yb; j++)
                {
                    printf("%d ", UXY[i+j*xb]);
                }
                printf("\n");
            }
            printf("\n");*/

            // update cohesion values according to conflicts between X and Y
            // by looping over all points z
            DXz = D + x;
            DYz = D + y; // init pointers to subcolumns of D
            CXz = C + x;
            CYz = C + y; // init pointers to subcolumns of C
            for (z = 0; z < n; z++) {
                // loop over all (i,j) pairs in block
                for (j = 0; j < yb; j++) {
                    ib = (x == y ? j : xb); // handle diagonal blocks
                    for (i = 0; i < ib; i++) {
                        // DXY[i+j*xb] is distance between x+i and y+j
                        // DXz[i] is distance between x+i and z
                        // DYz[j] is distance between y+j and z

                        // check if z is in conflict of (x+i,y+j)
                        if (DYz[j] <= beta * DXY[i + j * xb] || DXz[i] <= beta * DXY[i + j * xb]) {
                            // z supports x+i
                            if (DXz[i] < DYz[j])
                                CXz[i] += 1.0f / UXY[i + j * xb];
                                // z supports y+j
                            else if (DYz[j] < DXz[i])
                                CYz[j] += 1.0f / UXY[i + j * xb];
                                // z splits its support
                            else {
                                CXz[i] += 0.5f / UXY[i + j * xb];
                                CYz[j] += 0.5f / UXY[i + j * xb];
                            }
                        }
                    }
                }

                // update pointers to subcolumns of D and C
                DXz += n;
                DYz += n;
                CXz += n;
                CYz += n;
            }
        }
    }

    // free up cache blocks
    free(DXY);
    free(UXY);
}

/*
params
D    in  distance matrix: D(x,y) is distance between x and y (symmetric,
         but assumed to be stored in full)
beta in  conflict focus parameter: z is in focus of (x,y) if
         min(d(z,x),d(z,y)) <= beta * d(x,y)
n    in  number of points
C    out cohesion matrix: C(x,z) is z's support for x
b    in  blocking parameter for cache efficiency
t    in  number of OMP threads to use
*/
void pald_opt_par(float *D, float beta, int n, float *C, const int b, int t) {

    // decare timers
    float tic, cum_time_U = 0, cum_time_C = 0;

    // pre-allocate conflict focus and distance cache blocks
    int *UXY = (int *) malloc(b * b * sizeof(int));
    float *DXY = (float *) malloc(b * b * sizeof(float));

    // loop over blocks of points Y = (y,...,y+b-1)
    for (int y = 0; y < n; y += b) {
        // define actual block size (for corner cases)
        int yb = (b < n - y ? b : n - y);

        // loop over blocks of points X = (x,...,x+b-1)
        for (int x = 0; x <= y; x += b) {
            // define actual block size (for corner cases)
            int xb = (b < n - x ? b : n - x);

            // copy distances into cache block one column at a time
            for (int j = 0; j < yb; j++) {
                // DXY(:,j) = D(x:x+xb,y+j) in off-diagonal case
                int ib = (x == y ? j : xb); // handle diagonal blocks
                memcpy(DXY + j * xb, D + x + (y + j) * n, ib * sizeof(float));
            }

            // DEBUG: print out DXY cache block
            /*printf("x %d y %d xb %d yb %d\n",x,y,xb,yb);
            printf("\nDXY\n");
            for (int i = 0; i < xb; i++)
            {
                for (int j = 0; j < yb; j++)
                {
                    printf("%f ", DXY[i+j*xb]);
                }
                printf("\n");
            }
            printf("\n");*/

            // compute block's conflict focus sizes by looping over all points z
            memset(UXY, 0, b * b * sizeof(int)); // clear old values
            tic = omp_get_wtime();
            #pragma omp parallel for num_threads(t) reduction(+:UXY[:b*b])
            for (int z = 0; z < n; z++) {
                // set pointers to subcolumns of D
                float* DXz = D + x + z*n;
                float* DYz = D + y + z*n; 
                // loop over all (i,j) pairs in block
                for (int j = 0; j < yb; j++) {
                    int ib = (x == y ? j : xb); // handle diagonal blocks
                    for (int i = 0; i < ib; i++)
                        // DXY[i+j*xb] is distance between x+i and y+j
                        // DXz[i] is distance between x+i and z
                        // DYz[j] is distance between y+j and z

                        // determine if z is in conflict focus of x+i and y+j
                        if (DYz[j] <= beta * DXY[i + j * xb] || DXz[i] <= beta * DXY[i + j * xb])
                            UXY[i + j * xb]++;
                }
            }
            cum_time_U += omp_get_wtime() - tic;

            // DEBUG: print out UXY cache block
            /*for (int i = 0; i < xb; i++)
            {
                for (int j = 0; j < yb; j++)
                {
                    printf("%d ", UXY[i+j*xb]);
                }
                printf("\n");
            }
            printf("\n");*/

            // update cohesion values according to conflicts between X and Y
            // by looping over all points z
            tic = omp_get_wtime();
            #pragma omp parallel for num_threads(t)
            for (int z = 0; z < n; z++) {
                // set pointers to subcolumns of D
                float* DXz = D + x + z*n;
                float* DYz = D + y + z*n; 
                // set pointers to subcolumns of C
                float* CXz = C + x + z*n;
                float* CYz = C + y + z*n; 
                // loop over all (i,j) pairs in block
                for (int j = 0; j < yb; j++) {
                    int ib = (x == y ? j : xb); // handle diagonal blocks
                    for (int i = 0; i < ib; i++) {
                        // DXY[i+j*xb] is distance between x+i and y+j
                        // DXz[i] is distance between x+i and z
                        // DYz[j] is distance between y+j and z

                        // check if z is in conflict of (x+i,y+j)
                        if (DYz[j] <= beta * DXY[i + j * xb] || DXz[i] <= beta * DXY[i + j * xb]) {
                            // z supports x+i
                            if (DXz[i] < DYz[j])
                                CXz[i] += 1.0 / UXY[i + j * xb];
                                // z supports y+j
                            else if (DYz[j] < DXz[i])
                                CYz[j] += 1.0 / UXY[i + j * xb];
                                // z splits its support
                            else {
                                CXz[i] += 0.5 / UXY[i + j * xb];
                                CYz[j] += 0.5 / UXY[i + j * xb];
                            }
                        }
                    }
                }
            }
            cum_time_C += omp_get_wtime() - tic;
        }
    }

    // free up cache blocks
    free(DXY);
    free(UXY);

    // print out timing results before returning
    printf("Time spent computing U: %f\n", cum_time_U);
    printf("Time spent computing C: %f\n", cum_time_C);
}
