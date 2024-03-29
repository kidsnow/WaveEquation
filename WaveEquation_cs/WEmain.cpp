#define _CRT_SECURE_NO_WARNINGS
//#define CPU_COMPUTE

#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <FreeImage/FreeImage.h>
#include <math.h>

#include "grid.h"
#include "gridrenderer.h"

#include "WEmain.h"
//#include "WEgpu.h"
//#include <cuda_gl_interop.h>

struct Grid{
	float x, y, z, w;
};

// Begin of shader setup
#include "Shaders/LoadShaders.h"
GLuint h_ShaderProgram, ComputeShaderProgram, h_ShaderProgram_texture; // handle to shader program
GLint loc_ModelViewProjectionMatrix, loc_ModelViewProjectionMatrix2, loc_primitive_color, loc_scale; // indices of uniform variables
GLint loc_a, loc_b, loc_c;
int gridTotalNum = GRIDSIDENUM*GRIDSIDENUM;
int turn = 0, result = 0;

float beta, diag_el_of_A;
float time_interval = TIMEINTERVAL;
int grid_size = GRIDSIDENUM;
float grid_length = GRIDLENGTH;
float h = GRIDLENGTH / GRIDSIDENUM;

struct cudaGraphicsResource *cuda_resource[4];

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void WEGaussSeidel(Grid grid0[], Grid grid1[], Grid grid2[]){

	for (int i = 0; i < GRIDSIDENUM; i++){
		for (int j = 0; j < GRIDSIDENUM; j++){
			int idx = i*GRIDSIDENUM + j;
			float i0, i1, j0, j1;

			if (idx - GRIDSIDENUM < 0) i0 = 0;
			else i0 = grid2[idx - GRIDSIDENUM].y;
			if (idx + GRIDSIDENUM > GRIDSIDENUM*GRIDSIDENUM - 1) i1 = 0;
			else i1 = grid1[idx + GRIDSIDENUM].y;
			if (j - 1 < 0)j0 = 0;
			else j0 = grid2[idx - 1].y;
			if (j + 1 > GRIDSIDENUM - 1) j1 = 0;
			else j1 = grid1[idx + 1].y;

			//grid2[idx].y = -(i0 + j0)*matB - (i1 + j1)*matB + (2 * grid1[idx].y - grid0[idx].y);
			//grid2[idx].y /= matA;
		}
	}
}

void WEJacobi(Grid grid0[], Grid grid1[], Grid grid2[]){

	for (int i = 0; i < GRIDSIDENUM; i++){
		for (int j = 0; j < GRIDSIDENUM; j++){
			int idx = i*GRIDSIDENUM + j;
			float i0, i1, j0, j1;

			if (idx - GRIDSIDENUM < 0) i0 = 0;
			else i0 = grid1[idx - GRIDSIDENUM].y;
			if (idx + GRIDSIDENUM > GRIDSIDENUM*GRIDSIDENUM - 1) i1 = 0;
			else i1 = grid1[idx + GRIDSIDENUM].y;
			if (j - 1 < 0)j0 = 0;
			else j0 = grid1[idx - 1].y;
			if (j + 1 > GRIDSIDENUM - 1) j1 = 0;
			else j1 = grid1[idx + 1].y;

			//grid2[idx].y = (2 * grid1[idx].y - grid0[idx].y) - (i0 + j0 + i1 + j1)*matB;
			//grid2[idx].y /= matA;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void My_glTexImage2D_from_file(char *filename) {
	FREE_IMAGE_FORMAT tx_file_format;
	int tx_bits_per_pixel;
	FIBITMAP *tx_pixmap, *tx_pixmap_32;

	int width, height;
	GLvoid *data;

	tx_file_format = FreeImage_GetFileType(filename, 0);
	// assume everything is fine with reading texture from file: no error checking
	tx_pixmap = FreeImage_Load(tx_file_format, filename);
	tx_bits_per_pixel = FreeImage_GetBPP(tx_pixmap);

	fprintf(stdout, " * A %d-bit texture was read from %s.\n", tx_bits_per_pixel, filename);
	if (tx_bits_per_pixel == 32)
		tx_pixmap_32 = tx_pixmap;
	else {
		fprintf(stdout, " * Converting texture from %d bits to 32 bits...\n", tx_bits_per_pixel);
		tx_pixmap_32 = FreeImage_ConvertTo32Bits(tx_pixmap);
	}

	width = FreeImage_GetWidth(tx_pixmap_32);
	height = FreeImage_GetHeight(tx_pixmap_32);
	data = FreeImage_GetBits(tx_pixmap_32);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, data);
	fprintf(stdout, " * Loaded %dx%d RGBA texture into graphics memory.\n\n", width, height);

	FreeImage_Unload(tx_pixmap_32);
	if (tx_bits_per_pixel != 32)
		FreeImage_Unload(tx_pixmap);
}
void prepare_shader_program(void) {
	ShaderInfo shader_info[3] = {
		{ GL_VERTEX_SHADER, "Shaders/simple.vert" },
		{ GL_FRAGMENT_SHADER, "Shaders/simple.frag" },
		{ GL_NONE, NULL }
	};

	h_ShaderProgram = LoadShaders(shader_info);
	//glUseProgram(h_ShaderProgram);

	loc_ModelViewProjectionMatrix = glGetUniformLocation(h_ShaderProgram, "u_ModelViewProjectionMatrix");
	loc_primitive_color = glGetUniformLocation(h_ShaderProgram, "u_primitive_color");

	ShaderInfo cs_shader_info[2] = {
		{ GL_COMPUTE_SHADER, "Shaders/wecs.comp" },
		{ GL_NONE, NULL }
	};

#ifndef CUDA
	ComputeShaderProgram = LoadShaders(cs_shader_info);
	//glUseProgram(ComputeShaderProgram);
	loc_a = glGetUniformLocation(ComputeShaderProgram, "diag_el_of_A");
	loc_b = glGetUniformLocation(ComputeShaderProgram, "beta");
	loc_c = glGetUniformLocation(ComputeShaderProgram, "grid_size");
#endif

	ShaderInfo shader_info_texture[3] = {
		{ GL_VERTEX_SHADER, "Shaders/texture.vert" },
		{ GL_FRAGMENT_SHADER, "Shaders/texture.frag" },
		{ GL_NONE, NULL }
	};

	h_ShaderProgram_texture = LoadShaders(shader_info_texture);
	loc_ModelViewProjectionMatrix2 = glGetUniformLocation(h_ShaderProgram_texture, "u_ModelViewProjectionMatrix");
	loc_scale = glGetUniformLocation(h_ShaderProgram_texture, "u_scale");
}
// End of shader setup
// Begin of geometry setup

#define BUFFER_OFFSET(offset) ((GLvoid *) (offset))
#define INDEX_VERTEX_POSITION 0

GLuint axes_VBO, axes_VAO;
GLfloat axes_vertices[6][3] = {
	{ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }
};
GLfloat axes_color[3][3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

void prepare_axes(void) {
	// Initialize vertex buffer object.
	glGenBuffers(1, &axes_VBO);

	glBindBuffer(GL_ARRAY_BUFFER, axes_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axes_vertices), &axes_vertices[0][0], GL_STATIC_DRAW);

	// Initialize vertex array object.
	glGenVertexArrays(1, &axes_VAO);
	glBindVertexArray(axes_VAO);

	glBindBuffer(GL_ARRAY_BUFFER, axes_VBO);
	glVertexAttribPointer(INDEX_VERTEX_POSITION, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(INDEX_VERTEX_POSITION);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_axes(void) {
	glBindVertexArray(axes_VAO);
	glUniform3fv(loc_primitive_color, 1, axes_color[0]);
	glDrawArrays(GL_LINES, 0, 2);
	glUniform3fv(loc_primitive_color, 1, axes_color[1]);
	glDrawArrays(GL_LINES, 2, 2);
	glUniform3fv(loc_primitive_color, 1, axes_color[2]);
	glDrawArrays(GL_LINES, 4, 2);
	glBindVertexArray(0);
}

GLfloat vVertices[] = {
	-1.0f, -1.0f, 1.0f, 0.0f, 0.0f,
	1.0f, -1.0f, 1.0f, 1.0f, 0.0f,
	1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
	-1.0f, -1.0f, 1.0f, 0.0f, 0.0f,
	1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
};
struct Grid grid0[GRIDSIDENUM*(GRIDSIDENUM + 2)], grid1[GRIDSIDENUM*(GRIDSIDENUM + 2)], grid2[GRIDSIDENUM*(GRIDSIDENUM + 2)], grid3[GRIDSIDENUM*(GRIDSIDENUM + 2)];
GLuint bufs[6], gridBuf0, gridBuf1, gridBuf2, gridBuf3, elBuf, elBuf2, grid_VBO, grid_VAO, shadow_VBO, shadow_VAO;
GLuint GridIndices[(GRIDSIDENUM - 1)*(GRIDSIDENUM - 1) * 6];
GLuint GridIndices2[(GRIDSIDENUM - 1)*(GRIDSIDENUM - 1) * 12];
int triangleNum = (GRIDSIDENUM - 1)*(GRIDSIDENUM - 1) * 2;
int trianglePointNum = triangleNum * 3;
int gridMaxIdx = 0, gridMaxIdx2 = 0;

void draw_texture()
{
	glDisable(GL_DEPTH_TEST);

	glBindVertexArray(shadow_VAO);
	glDrawElements(GL_TRIANGLES, gridMaxIdx2, GL_UNSIGNED_INT, BUFFER_OFFSET(0));
	glBindVertexArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glEnable(GL_DEPTH_TEST);
}

void prepare_texture(void) {
	glGenBuffers(1, &shadow_VBO);

	glBindBuffer(GL_ARRAY_BUFFER, shadow_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices), vVertices, GL_STATIC_DRAW);

	glGenVertexArrays(1, &shadow_VAO);

	glBindVertexArray(shadow_VAO);

	glBindBuffer(GL_ARRAY_BUFFER, gridBuf1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(INDEX_VERTEX_POSITION);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elBuf2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
}

void prepare_grid(void){
	//grid vertex initialization
	for (int i = 0; i < GRIDSIDENUM + 2; i++) {
		for (int j = 0; j < GRIDSIDENUM; j++) {
			int idx = GRIDSIDENUM*i + j;
			grid0[idx].y = 0.0f;
			grid1[idx].y = 0.0f;
			grid2[idx].y = 0.0f;
			grid3[idx].y = 0.0f;
			grid0[idx].w = 0.0f;
			grid1[idx].w = 0.0f;
			grid2[idx].w = 0.0f;
			grid3[idx].w = 0.0f;
		}
	}

	for (int i = 0; i < GRIDSIDENUM; i++){
		for (int j = 0; j < GRIDSIDENUM; j++){
			int idx = GRIDSIDENUM * (i + 1) + j;
			grid0[idx].x = j*GRIDLENGTH / GRIDSIDENUM;
			grid0[idx].y = 0.0f;
			grid0[idx].z = i*GRIDLENGTH / GRIDSIDENUM;
			grid0[idx].w = 1.0f;

			grid1[idx].x = j*GRIDLENGTH / GRIDSIDENUM;
			grid1[idx].y = 0.0f;
			grid1[idx].z = i*GRIDLENGTH / GRIDSIDENUM;
			grid1[idx].w = 1.0f;

			grid2[idx].x = j*GRIDLENGTH / GRIDSIDENUM;
			grid2[idx].y = 0.0f;
			grid2[idx].z = i*GRIDLENGTH / GRIDSIDENUM;
			grid2[idx].w = 1.0f;

			grid3[idx].x = j*GRIDLENGTH / GRIDSIDENUM;
			grid3[idx].y = 0.0f;
			grid3[idx].z = i*GRIDLENGTH / GRIDSIDENUM;
			grid3[idx].w = 1.0f;
		}
	}

	//grid1[GRIDSIDENUM*GRIDSIDENUM / 3 - GRIDSIDENUM / 3].y = INITSPEED * TIMEINTERVAL;
#ifdef RAINFALL
	int x_mid = GRIDSIDENUM * 1 / 4 + 1;
	int y_mid = GRIDSIDENUM * 3 / 4;
	for (int i = 0; i < GRIDSIDENUM; i++) {
		for (int j = 0; j < GRIDSIDENUM; j++) {
			int idx = GRIDSIDENUM * (i + 1) + j;
			grid0[idx].y = (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
		}
	}
	x_mid = GRIDSIDENUM / 3 + 1;
	y_mid = GRIDSIDENUM / 3;
	for (int i = 0; i < GRIDSIDENUM; i++) {
		for (int j = 0; j < GRIDSIDENUM; j++) {
			int idx = GRIDSIDENUM * (i + 1) + j;
			grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
		}
	}
	x_mid = GRIDSIDENUM * 2 / 3 + 1;
	y_mid = GRIDSIDENUM / 3;
	for (int i = 0; i < GRIDSIDENUM; i++) {
		for (int j = 0; j < GRIDSIDENUM; j++) {
			int idx = GRIDSIDENUM * (i + 1) + j;
			grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
		}
	}
	x_mid = GRIDSIDENUM * 2 / 3 + 1;
	y_mid = GRIDSIDENUM / 3;
	for (int i = 0; i < GRIDSIDENUM; i++) {
		for (int j = 0; j < GRIDSIDENUM; j++) {
			int idx = GRIDSIDENUM * (i + 1) + j;
			grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
		}
	}
	x_mid = GRIDSIDENUM / 3 + 1;
	y_mid = GRIDSIDENUM / 3;
	for (int i = 0; i < GRIDSIDENUM; i++) {
		for (int j = 0; j < GRIDSIDENUM; j++) {
			int idx = GRIDSIDENUM * (i + 1) + j;
			grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
		}
	}
	x_mid = GRIDSIDENUM / 3 + 1;
	y_mid = GRIDSIDENUM / 3;
	for (int i = 0; i < GRIDSIDENUM; i++) {
		for (int j = 0; j < GRIDSIDENUM; j++) {
			int idx = GRIDSIDENUM * (i + 1) + j;
			grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
		}
	}
#else
	int x_mid = GRIDSIDENUM / 2 + 1;
	int y_mid = GRIDSIDENUM / 2;
	for (int i = 0; i < GRIDSIDENUM; i++) {
		for (int j = 0; j < GRIDSIDENUM; j++) {
			int idx = GRIDSIDENUM * (i + 1) + j;
			grid0[idx].y = (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
		}
	}
#endif
	//triangle mesh initialization			INDEXING!!!

	for (int i = 1; i < GRIDSIDENUM; i++){
		for (int j = 0; j < GRIDSIDENUM - 1; j++){
			int idx = i*GRIDSIDENUM + j;
			GridIndices[gridMaxIdx] = idx;
			gridMaxIdx++;
			GridIndices[gridMaxIdx] = idx + 1;
			gridMaxIdx++;
			GridIndices[gridMaxIdx] = idx;
			gridMaxIdx++;
			GridIndices[gridMaxIdx] = idx + GRIDSIDENUM;
			gridMaxIdx++;
		}
	}
	for (int i = 1; i < GRIDSIDENUM; i++) {
		GridIndices[gridMaxIdx] = GRIDSIDENUM*i + GRIDSIDENUM - 1;
		gridMaxIdx++;
		GridIndices[gridMaxIdx] = GRIDSIDENUM*(i + 1) + GRIDSIDENUM - 1;
		gridMaxIdx++;
		GridIndices[gridMaxIdx] = GRIDSIDENUM*GRIDSIDENUM + i - 1;
		gridMaxIdx++;
		GridIndices[gridMaxIdx] = GRIDSIDENUM*GRIDSIDENUM + i;
		gridMaxIdx++;
	}

	for (int i = 1; i < GRIDSIDENUM; i++){
		for (int j = 0; j < GRIDSIDENUM - 1; j++){
			int idx = i*GRIDSIDENUM + j;
			GridIndices2[gridMaxIdx2] = idx;
			gridMaxIdx2++;
			GridIndices2[gridMaxIdx2] = idx + 1;
			gridMaxIdx2++;
			GridIndices2[gridMaxIdx2] = idx + GRIDSIDENUM;
			gridMaxIdx2++;
			GridIndices2[gridMaxIdx2] = idx + 1;
			gridMaxIdx2++;
			GridIndices2[gridMaxIdx2] = idx + 1 + GRIDSIDENUM;
			gridMaxIdx2++;
			GridIndices2[gridMaxIdx2] = idx + GRIDSIDENUM;
			gridMaxIdx2++;
		}
	}

	/*for (int i = 1; i < GRIDSIDENUM; i++) {
		GridIndices[i] = ;
	}*/

	beta = ALPHASQUARE*time_interval*time_interval / (h*h);
	diag_el_of_A = 1 + 4 * beta;

	glGenBuffers(6, bufs);
	gridBuf0 = bufs[0];
	gridBuf1 = bufs[1];
	gridBuf2 = bufs[2];
	gridBuf3 = bufs[3];
	elBuf = bufs[4];
	elBuf2 = bufs[5];

#ifdef CUDA
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf0);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid0), &grid0[0], GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf1);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid1), &grid1[0], GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf2);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid2), &grid2[0], GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf3);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid3), &grid3[0], GL_DYNAMIC_DRAW);

	cudaGraphicsGLRegisterBuffer(&cuda_resource[0], gridBuf0, cudaGraphicsMapFlagsWriteDiscard);
	cudaGraphicsGLRegisterBuffer(&cuda_resource[1], gridBuf1, cudaGraphicsMapFlagsWriteDiscard);
	cudaGraphicsGLRegisterBuffer(&cuda_resource[2], gridBuf2, cudaGraphicsMapFlagsWriteDiscard);
	cudaGraphicsGLRegisterBuffer(&cuda_resource[3], gridBuf3, cudaGraphicsMapFlagsWriteDiscard);
#else
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridBuf0);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid0), &grid0[0], GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridBuf1);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid1), &grid1[0], GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridBuf2);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid2), &grid2[0], GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gridBuf3);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid3), &grid3[0], GL_DYNAMIC_DRAW);
#endif
	//element indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elBuf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GridIndices), &GridIndices[0], GL_DYNAMIC_COPY);

	//element indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elBuf2);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GridIndices2), &GridIndices2[0], GL_DYNAMIC_COPY);

	// Initialize vertex array object.
	glGenVertexArrays(1, &grid_VAO);
	glBindVertexArray(grid_VAO);

	glBindBuffer(GL_ARRAY_BUFFER, gridBuf1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(INDEX_VERTEX_POSITION);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elBuf);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#ifndef CUDA
	glUseProgram(ComputeShaderProgram);
	glUniform1f(loc_a, diag_el_of_A);
	glUniform1f(loc_b, beta);
	glUniform1i(loc_c, grid_size);
#endif
}


void draw_grid(void) {

#ifdef CPU_COMPUTE
	glBindBuffer(GL_ARRAY_BUFFER, gridBuf2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(grid2), &grid2[0], GL_DYNAMIC_DRAW);
#else
	//glBindBuffer(GL_ARRAY_BUFFER, bufs[result]);
#endif
	glUniform3f(loc_primitive_color, 1.0f, 1.0f, 0.2f);
	glBindVertexArray(grid_VAO);
	glDrawElements(GL_LINES, gridMaxIdx, GL_UNSIGNED_INT, BUFFER_OFFSET(0));
	glBindVertexArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

}

// Begin of Callback function definitions
#define TO_RADIAN 0.01745329252f  
#define TO_DEGREE 57.295779513f

// include glm/*.hpp only if necessary
// #include <glm/glm.hpp> 
#include <glm/gtc/matrix_transform.hpp> //translate, rotate, scale, lookAt, perspective, etc.
// ViewProjectionMatrix = ProjectionMatrix * ViewMatrix
glm::mat4 ViewProjectionMatrix, ViewMatrix, ProjectionMatrix, ViewProjectionMatrix2, ViewMatrix2, ProjectionMatrix2;
// ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * ModelMatrix
glm::mat4 ModelViewProjectionMatrix; // This one is sent to vertex shader when ready.

float rotation_angle_tiger = 0.0f, rotation_angle_cow = 0.0f;
int flag_fill_floor = 0;

#ifdef CUDA
void cudaRender() {
	float4 *pos_out[4];

	cudaGraphicsMapResources(4, cuda_resource, 0);

	for (int i = 0; i < 4; i ++)
		cudaGraphicsResourceGetMappedPointer((void **)&pos_out[i], NULL, cuda_resource[i]);

	callComputeWave(pos_out, diag_el_of_A, beta, grid_size);

	cudaGraphicsUnmapResources(4, cuda_resource, 0);
}
#endif

#include <windows.h>
#include <winbase.h>
#include <time.h>

int count = 0;
int total_time = 0.0;
int window_width, window_height;
void display(void) {
	LARGE_INTEGER seed;
	QueryPerformanceCounter(&seed);
	srand(seed.QuadPart);
	LARGE_INTEGER start, end, f;
	QueryPerformanceFrequency(&f);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glViewport(0, 0, window_width - window_height, window_height);
	glClearColor(0.4f, 0.4f, 1.0f, 1.0f); // CYAN


	// fps ���.
	static int nbFrames = 0;
	static double lastTime = glutGet(GLUT_ELAPSED_TIME);
	double currTime = glutGet(GLUT_ELAPSED_TIME);

	nbFrames++;
	if(currTime - lastTime >= 1000.0)
	{
		char buf[128];
#ifdef CUDA
		sprintf(buf, "Wave Equation with CUDA (GTX 690) : %.2f fps\n", double(nbFrames));
#else
		sprintf(buf, "Wave Equation with CS (GTX 690) : %.2f fps\n", double(nbFrames));
#endif
		glutSetWindowTitle(buf);
		nbFrames = 0;
		lastTime = currTime;
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	int iteration = ITERNUM;
#ifdef CPU_COMPUTE
	//cpu computing
	for (int i = 0; i < iteration; i++){

		//WEGaussSeidel(t0, t1, t2);
		WEJacobi(grid0, grid1, grid2);

		if (i == iteration - 1)
			break;

		for (int i = 0; i < GRIDSIDENUM; i++){
			for (int j = 0; j < GRIDSIDENUM; j++){
				grid0[GRIDSIDENUM * i + j].y = grid1[GRIDSIDENUM * i + j].y;
				grid1[GRIDSIDENUM * i + j].y = grid2[GRIDSIDENUM * i + j].y;
			}
		}
	}
#else
#ifdef CUDA
	cudaRender();
#else
	glUseProgram(ComputeShaderProgram);
	for (int i = 0; i < iteration; i++){
		glDispatchCompute(GRIDSIDENUM / 8, GRIDSIDENUM / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[turn % 3]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[(turn + 1) % 3]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bufs[(turn + 2) % 3]);
		result = (turn + 1) % 3;
		turn++;
	}
#endif
#endif
	glUseProgram(h_ShaderProgram);
	//ModelViewProjectionMatrix = glm::scale(ViewProjectionMatrix, glm::vec3(3.0f, 3.0f, 3.0f));
	ModelViewProjectionMatrix = ViewProjectionMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);

	//glLineWidth(3.0f);
	//draw_axes();
	glLineWidth(1.0f);
	draw_grid();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glViewport(window_width - window_height, 0, window_height, window_height);
	glUseProgram(h_ShaderProgram_texture);
	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix2, 1, GL_FALSE, &ViewProjectionMatrix2[0][0]);
	GLfloat scale = SCALE;
	glUniform1f(loc_scale, scale);
	draw_texture();

#ifdef DOUBLE
	glutSwapBuffers();
#else
	glutPostRedisplay();
	glFlush();
#endif
}
int x_mid, y_mid;
float speed;
void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 27: // ESC key
		glutLeaveMainLoop(); // Incur destuction callback for cleanups.
		break;
	case 's':
		for (int i = 0; i < GRIDSIDENUM; i++){
			for (int j = 0; j < GRIDSIDENUM; j++){
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].y = 0.0f;
				grid1[idx].y = 0.0f;
				grid2[idx].y = 0.0f;
				grid3[idx].y = 0.0f;
			}
		}
#ifdef CUDA
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf0);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid0), &grid0[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf1);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid1), &grid1[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf2);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid2), &grid2[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf3);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid3), &grid3[0], GL_DYNAMIC_DRAW);

		cudaGraphicsGLRegisterBuffer(&cuda_resource[0], gridBuf0, cudaGraphicsMapFlagsWriteDiscard);
		cudaGraphicsGLRegisterBuffer(&cuda_resource[1], gridBuf1, cudaGraphicsMapFlagsWriteDiscard);
		cudaGraphicsGLRegisterBuffer(&cuda_resource[2], gridBuf2, cudaGraphicsMapFlagsWriteDiscard);
		cudaGraphicsGLRegisterBuffer(&cuda_resource[3], gridBuf3, cudaGraphicsMapFlagsWriteDiscard);
#else
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridBuf0);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid0), &grid0[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridBuf1);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid1), &grid1[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridBuf2);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid2), &grid2[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gridBuf3);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid3), &grid3[0], GL_DYNAMIC_DRAW);
#endif
		break;
	case 't':
#ifdef RAINFALL
		x_mid = rand() * 1000 % GRIDSIDENUM;
		y_mid = rand() * 1000 % GRIDSIDENUM;
		speed = (1.0 + (0.5 - (rand() % 5) * 0.1)) * INITSPEED;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid3[idx].y = (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * speed * TIMEINTERVAL;
			}
		}
#else
		x_mid = GRIDSIDENUM / 2 + 1;
		y_mid = GRIDSIDENUM / 2;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid3[idx].y = (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
			}
		}
#endif
#ifdef CUDA
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf3);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid3), &grid3[0], GL_DYNAMIC_DRAW);
		cudaGraphicsGLRegisterBuffer(&cuda_resource[3], gridBuf3, cudaGraphicsMapFlagsWriteDiscard);
#else
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gridBuf3);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid3), &grid3[0], GL_DYNAMIC_DRAW);
#endif
		break;
	case 'r':

		//grid vertex initialization
		for (int i = 0; i < GRIDSIDENUM + 2; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM*i + j;
				grid0[idx].y = 0.0f;
				grid1[idx].y = 0.0f;
				grid2[idx].y = 0.0f;
				grid0[idx].w = 0.0f;
				grid1[idx].w = 0.0f;
				grid2[idx].w = 0.0f;
			}
		}

		for (int i = 0; i < GRIDSIDENUM; i++){
			for (int j = 0; j < GRIDSIDENUM; j++){
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].x = j*GRIDLENGTH / GRIDSIDENUM;
				grid0[idx].y = 0.0f;
				grid0[idx].z = i*GRIDLENGTH / GRIDSIDENUM;
				grid0[idx].w = 1.0f;

				grid1[idx].x = j*GRIDLENGTH / GRIDSIDENUM;
				grid1[idx].y = 0.0f;
				grid1[idx].z = i*GRIDLENGTH / GRIDSIDENUM;
				grid1[idx].w = 1.0f;

				grid2[idx].x = j*GRIDLENGTH / GRIDSIDENUM;
				grid2[idx].y = 0.0f;
				grid2[idx].z = i*GRIDLENGTH / GRIDSIDENUM;
				grid2[idx].w = 1.0f;
			}
		}
		//grid1[GRIDSIDENUM*GRIDSIDENUM / 3 - GRIDSIDENUM / 3].y = INITSPEED * TIMEINTERVAL;
#ifdef RAINFALL
		int x_mid = GRIDSIDENUM * 1 / 4 + 1;
		int y_mid = GRIDSIDENUM * 3 / 4;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].y = (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
			}
		}
		x_mid = GRIDSIDENUM / 3 + 1;
		y_mid = GRIDSIDENUM / 3;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
			}
		}
		x_mid = GRIDSIDENUM * 2 / 3 + 1;
		y_mid = GRIDSIDENUM / 3;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
			}
		}
		x_mid = GRIDSIDENUM * 2 / 3 + 1;
		y_mid = GRIDSIDENUM / 3;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
			}
		}
		x_mid = GRIDSIDENUM / 3 + 1;
		y_mid = GRIDSIDENUM / 3;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
			}
		}
		x_mid = GRIDSIDENUM / 3 + 1;
		y_mid = GRIDSIDENUM / 3;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].y += (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
			}
		}
#else
		x_mid = GRIDSIDENUM / 2 + 1;
		y_mid = GRIDSIDENUM / 2;
		for (int i = 0; i < GRIDSIDENUM; i++) {
			for (int j = 0; j < GRIDSIDENUM; j++) {
				int idx = GRIDSIDENUM * (i + 1) + j;
				grid0[idx].y = (pow(EEE, (-1)*(((i - x_mid) * (i - x_mid) + (j - y_mid) * (j - y_mid))) / (2 * SIGMA*SIGMA))) / (2 * PI*SIGMA*SIGMA) * INITSPEED * TIMEINTERVAL;
			}
		}
#endif
		if (grid0[GRIDSIDENUM * (32 + 1) + 32].y < 0) {
			printf("FK!\n");
		}
		//triangle mesh initialization			INDEXING!!!

#ifdef CUDA
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf0);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid0), &grid0[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf1);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid1), &grid1[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf2);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid2), &grid2[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gridBuf3);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(grid3), &grid3[0], GL_DYNAMIC_DRAW);

		cudaGraphicsGLRegisterBuffer(&cuda_resource[0], gridBuf0, cudaGraphicsMapFlagsWriteDiscard);
		cudaGraphicsGLRegisterBuffer(&cuda_resource[1], gridBuf1, cudaGraphicsMapFlagsWriteDiscard);
		cudaGraphicsGLRegisterBuffer(&cuda_resource[2], gridBuf2, cudaGraphicsMapFlagsWriteDiscard);
		cudaGraphicsGLRegisterBuffer(&cuda_resource[3], gridBuf3, cudaGraphicsMapFlagsWriteDiscard);
#else
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridBuf0);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid0), &grid0[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridBuf1);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid1), &grid1[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridBuf2);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid2), &grid2[0], GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gridBuf3);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(grid3), &grid3[0], GL_DYNAMIC_DRAW);
#endif
		break;
	}
	glutPostRedisplay();
}

void mousepress(int button, int state, int x, int y)  {
	if ((button == GLUT_LEFT_BUTTON) && (state == GLUT_DOWN)) {
		flag_fill_floor = 1;
		glutPostRedisplay();
	}
	else if ((button == GLUT_LEFT_BUTTON) && (state == GLUT_UP)) {
		flag_fill_floor = 0;
		glutPostRedisplay();
	}
}

void reshape(int width, int height) {
	float aspect_ratio;
	glViewport(0, 0, width - 400, height);
	window_width = width;
	window_height = height;

	aspect_ratio = (float)(width - 400) / height;
	ProjectionMatrix = glm::perspective(15.0f*TO_RADIAN, aspect_ratio, 1.0f, 1000.0f);

	ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
	glutPostRedisplay();
}

void timer_scene(int timestamp_scene) {
	glutPostRedisplay();
	glutTimerFunc(10, timer_scene, 0);
}

void cleanup(void) {
	glDeleteVertexArrays(1, &axes_VAO);
	glDeleteBuffers(1, &axes_VBO);

	glDeleteVertexArrays(1, &grid_VAO);
	glDeleteBuffers(1, &grid_VAO);
}
// End of callback function definitions

void register_callbacks(void) {
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutMouseFunc(mousepress);
	glutReshapeFunc(reshape);
	glutTimerFunc(WAITFOR, timer_scene, 0);
	glutCloseFunc(cleanup);
}

void initialize_OpenGL(void) {
	glClearColor(0.4f, 0.4f, 1.0f, 1.0f); // CYAN
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glEnable(GL_DEPTH_TEST);

	ViewMatrix = glm::lookAt(glm::vec3(200.0f, 100.0f, 200.0f), glm::vec3(GRIDLENGTH / 2, 0.0f, GRIDLENGTH / 2),
		glm::vec3(0.0f, 1.0f, 0.0f));

	ViewMatrix2 = glm::lookAt(glm::vec3(48.4375f, 100.0f, 48.4375f), glm::vec3(48.4375f, 0.0f, 48.4375f),
		glm::vec3(-1.0f, 0.0f, 0.0f));

	ProjectionMatrix2 = glm::ortho(-48.4375f, 48.4375f, -48.4375f, 48.4375f);
	ProjectionMatrix2[2][2] = 0.0;
	ViewProjectionMatrix2 = ProjectionMatrix2 * ViewMatrix2;
}

void prepare_scene(void) {
	kidsnow::Grid *grid = new kidsnow::Grid(64, 100.0f);
	kidsnow::GridRenderer *renderer = new kidsnow::GridRenderer(grid);
	prepare_axes();
	prepare_grid();
	prepare_texture();
}

void initialize_renderer(void) {
	register_callbacks();
	prepare_shader_program();
	initialize_OpenGL();
	prepare_scene();
}

void initialize_glew(void) {
	GLenum error;

	glewExperimental = GL_TRUE;

	error = glewInit();
	if (error != GLEW_OK) {
		fprintf(stderr, "Error: %s\n", glewGetErrorString(error));
		exit(-1);
	}
	fprintf(stdout, "*********************************************************\n");
	fprintf(stdout, " - GLEW version supported: %s\n", glewGetString(GLEW_VERSION));
	fprintf(stdout, " - OpenGL renderer: %s\n", glGetString(GL_RENDERER));
	fprintf(stdout, " - OpenGL version supported: %s\n", glGetString(GL_VERSION));
	fprintf(stdout, "*********************************************************\n\n");
}

void print_message(const char * m) {
	fprintf(stdout, "%s\n\n", m);
}

void greetings(char *program_name, char messages[][256], int n_message_lines) {
	fprintf(stdout, "**************************************************************\n\n");
	fprintf(stdout, "  PROGRAM NAME: %s\n\n", program_name);

	for (int i = 0; i < n_message_lines; i++)
		fprintf(stdout, "%s\n", messages[i]);
	fprintf(stdout, "\n**************************************************************\n\n");

	initialize_glew();
}

#define N_MESSAGE_LINES 3
void main(int argc, char *argv[]) {
	char program_name[64] = "Wave Equation Simulation";
	char messages[N_MESSAGE_LINES][256] = { "    - Keys used: 'ESC'",
		"    - Mouse used: Left Butten Click"
	};

	glutInit(&argc, argv);
#ifdef DOUBLE
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH);
#endif
	glutInitWindowSize(2000, 800);
	glutInitContextVersion(4, 0);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutCreateWindow(program_name);

	greetings(program_name, messages, N_MESSAGE_LINES);
	initialize_renderer();

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutMainLoop();
}
