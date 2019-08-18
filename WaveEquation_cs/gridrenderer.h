#pragma once

/** GridRenderer class.

현재 활성화되어있는 rendering context에서
입력되는 grid의 정보를 rendering해주는 class.

Grid class에 dependency가 있음.
*/

#include <GL/glew.h>

namespace kidsnow {

class Grid;

class GridRenderer {
public:

public:
	GridRenderer(Grid *grid);
	~GridRenderer();

public:
	void UpdateGrid(Grid *grid);
	void Render();

private:
	void UpdateGPUResources();

private:
	Grid *m_currentGrid;
	GLuint *m_gridIndices;
	GLuint m_gridBuffer[4];
	GLuint m_elementBuffer;
};

}