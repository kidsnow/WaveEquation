#include <cuda_runtime.h>
#include <stdio.h>
#include "WEgpu.h"
#include "WEmain.h"

const int N = 16;
const int blocksize = 16;

__global__
void hello(char *a, int *b)
{
	a[threadIdx.x] += b[threadIdx.x];
}

int callHelloFromGPU() {
	char a[N] = "Hello \0\0\0\0\0\0";
	int b[N] = { 15, 10, 6, 0, -11, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	char *ad;
	int *bd;
	const int csize = N*sizeof(char);
	const int isize = N*sizeof(int);

	printf("%s", a);

	cudaMalloc((void**)&ad, csize);
	cudaMalloc((void**)&bd, isize);
	cudaMemcpy(ad, a, csize, cudaMemcpyHostToDevice);
	cudaMemcpy(bd, b, isize, cudaMemcpyHostToDevice);

	dim3 dimBlock(blocksize, 1);
	dim3 dimGrid(1, 1);
	hello <<<dimGrid, dimBlock >>>(ad, bd);
	cudaMemcpy(a, ad, csize, cudaMemcpyDeviceToHost);
	cudaFree(ad);
	cudaFree(bd);
	 
	printf("%s\n", a);
	return 1;
}

__global__
void computeWave(float diag_el_of_a, float beta, int grid_size, Grid* previous, Grid* current) {
	int x = threadIdx.x + blockIdx.x*blockDim.x;
	int y = threadIdx.y + blockIdx.y*blockDim.y;
	int idx = x + y*blockDim.x*gridDim.x;
	float temp = 0.0;

	if (previous[idx].w == 1.0) {
		temp += previous[idx - grid_size].y;
		temp += previous[idx - 1].y;
		temp += previous[idx + 1].y;
		temp += previous[idx + grid_size].y;
		current[idx].y = (2 * previous[idx].y - current[idx].y + beta*temp) / diag_el_of_a;
	}
}

__global__
void test(float diag_el_of_A, Grid* grid0, Grid* grid1) {
	grid0[0].x += grid1[0].x + diag_el_of_A;
}

void callComputeWave(float diag_el_of_A, float beta, int grid_size, Grid* grid0, Grid* grid1) {
	Grid* GPU_grid0;
	Grid* GPU_grid1;

	cudaMalloc(&GPU_grid0, sizeof(Grid)*GRIDSIDENUM*(GRIDSIDENUM + 2));
	cudaMalloc(&GPU_grid1, sizeof(Grid)*GRIDSIDENUM*(GRIDSIDENUM + 2));

	cudaMemcpy(GPU_grid0, grid0, sizeof(Grid)*GRIDSIDENUM*(GRIDSIDENUM + 2), cudaMemcpyHostToDevice);
	cudaMemcpy(GPU_grid1, grid1, sizeof(Grid)*GRIDSIDENUM*(GRIDSIDENUM + 2), cudaMemcpyHostToDevice);

	computeWave <<<GRIDSIDENUM*(GRIDSIDENUM + 2), GRIDSIDENUM>>> (diag_el_of_A, beta, grid_size, GPU_grid0, GPU_grid1);

	cudaMemcpy(grid0, GPU_grid0, sizeof(Grid)*GRIDSIDENUM*(GRIDSIDENUM + 2), cudaMemcpyDeviceToHost);
	cudaMemcpy(grid1, GPU_grid1, sizeof(Grid)*GRIDSIDENUM*(GRIDSIDENUM + 2), cudaMemcpyDeviceToHost);

	return;
}