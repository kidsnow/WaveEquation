#include <cuda_runtime.h>

struct Grid{
	float x, y, z, w;
};

void callComputeWave(float4 *pos0_out, float4 *pos1_out, float4 *pos2_out, float4 *pos3_out, float diag_el_of_A, float beta, int grid_size);