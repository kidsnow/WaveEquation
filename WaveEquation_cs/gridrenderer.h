#pragma once

/** GridRenderer class.

���� Ȱ��ȭ�Ǿ��ִ� rendering context����
�ԷµǴ� grid�� ������ rendering���ִ� class.

Grid class�� dependency�� ����.
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