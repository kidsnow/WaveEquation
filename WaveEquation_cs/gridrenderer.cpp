#include "gridrenderer.h"
#include "grid.h"

namespace kidsnow {

GridRenderer::GridRenderer(Grid *grid)
	: m_currentGrid(nullptr), m_gridIndices(nullptr)
{
	glGenBuffers(4, m_gridBuffer);
	glGenBuffers(1, &m_elementBuffer);

	m_currentGrid = grid;
	UpdateGPUResources();
}

GridRenderer::~GridRenderer()
{

}

void GridRenderer::UpdateGrid(Grid *grid)
{
	m_currentGrid = grid;
	UpdateGPUResources();
}

void GridRenderer::UpdateGPUResources()
{
	int dim = m_currentGrid->GetDimension();
	m_gridIndices = new GLuint[(dim - 1)*(dim - 1) * 6];

	int curIdx = 0;
	for (int i = 1; i < dim; i++)
	{
		for (int j = 0; j < dim - 1; j++)
		{
			int idx = i * dim + j;
			m_gridIndices[curIdx] = idx;
			curIdx++;
			m_gridIndices[curIdx] = idx + 1;
			curIdx++;
			m_gridIndices[curIdx] = idx;
			curIdx++;
			m_gridIndices[curIdx] = idx + dim;
			curIdx++;
		}
	}

	for (int i = 1; i < dim; i++)
	{
		m_gridIndices[curIdx] = dim * i + dim - 1;
		curIdx++;
		m_gridIndices[curIdx] = dim * (i + 1) + dim - 1;
		curIdx++;
		m_gridIndices[curIdx] = dim * dim + i - 1;
		curIdx++;
		m_gridIndices[curIdx] = dim * dim + i;
		curIdx++;
	}

	m_currentGrid->GetState(0)[0].x = 7.0f;
	m_currentGrid->GetState(0)[0].y = 7.0f;
	m_currentGrid->GetState(0)[0].z = 7.0f;
	m_currentGrid->GetState(0)[0].w = 7.0f;
	for (int i = 0; i < 4; i++)
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, m_gridBuffer[i]);
		glBufferData(GL_SHADER_STORAGE_BUFFER, dim * (dim + 2), m_currentGrid->GetState(i), GL_DYNAMIC_DRAW);
	}
}

void GridRenderer::Render()
{

}

}