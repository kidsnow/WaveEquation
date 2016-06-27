#include <stdio.h>
#include "WEgpu.h"
#include "WEmain.h"
#include "device_launch_parameters.h"

#define BLOCK_X 32
#define BLOCK_Y 32

__global__
void computeWave(float4 *previous, float4 *current, float4 *previousOfPrevious, float4 *moreEffects, float diag_el_of_A, float beta, int grid_size) {
	const int c = blockIdx.x * blockDim.x + threadIdx.x;
	const int r = blockIdx.y * blockDim.y + threadIdx.y;

	if ((c >= 64) || (r >= 66)) return;
	const int idx = c + r * 64;	// 1D indexing.

	float temp = 0.0;

	if (previous[idx].w == 1.0) {
		temp += previous[idx - grid_size].y;
		temp += previous[idx - 1].y;
		temp += previous[idx + 1].y;
		temp += previous[idx + grid_size].y;
		current[idx].y = (2 * previous[idx].y - previousOfPrevious[idx].y + beta*temp) / diag_el_of_A;
		current[idx].y += moreEffects[idx].y;
		moreEffects[idx].y = 0;
	}
	previousOfPrevious[idx] = previous[idx];
	previous[idx] = current[idx];
}

void callComputeWave(float4 *pos0_out, float4 *pos1_out, float4 *pos2_out, float4 *pos3_out, float diag_el_of_A, float beta, int grid_size) {
	const dim3 blockSize(BLOCK_X, BLOCK_Y);
	const dim3 gridSize = dim3((64 + BLOCK_X - 1) / BLOCK_X, (66 + BLOCK_Y - 1) / BLOCK_Y);
	for (int i = 0; i < ITERNUM; i ++)
		computeWave <<<gridSize, blockSize>>>(pos0_out, pos1_out, pos2_out, pos3_out, diag_el_of_A, beta, grid_size);
}