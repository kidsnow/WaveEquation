struct Grid{
	float x, y, z, w;
};

int callHelloFromGPU();
void callComputeWave(float diag_el_of_A, float beta, int grid_size, Grid* grid0, Grid* grid1);