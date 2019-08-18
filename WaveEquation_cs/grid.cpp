#include "grid.h"

namespace kidsnow {

Grid::Grid(int gridDimension, float gridSize)
	: m_dimension(gridDimension), m_size(gridSize)
{
	for (int i = 0; i < 4; i++)
		InitializeGrid(m_state[i]);
}

void Grid::InitializeGrid(glm::vec4 *state)
{
	state = new glm::vec4[m_dimension * (m_dimension + 2)];

	for (int i = 0; i < m_dimension + 2; i++)
	{
		for (int j = 0; j < m_dimension; j++)
		{
			int idx = m_dimension * i + j;
			state[idx].y = 0.0f;
			state[idx].w = 0.0f;
		}
	}

	for (int i = 0; i < m_dimension; i++)
	{
		for (int j = 0; j < m_dimension; j++)
		{
			int idx = m_dimension * (i + 1) + j;
			state[idx].x = j * m_size / m_dimension;
			state[idx].y = 0.0f;
			state[idx].z = i * m_size / m_dimension;
		}
	}
}

Grid::~Grid()
{
	for (int i = 0; i < 4; i++)
		delete m_state[i];
}

}