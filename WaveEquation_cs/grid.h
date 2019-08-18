#pragma once

#include "glm/glm.hpp"

/** Grid class.

정사각형의 2차원 grid에 대한 descriptor.
Grid의 크기와 상태값에 대한 정보가 담겨있다.
*/

namespace kidsnow {

class Grid {
public:
	Grid::Grid(int gridDimension, float gridLength);
	Grid::~Grid();

public:
	int m_dimension;
	float m_size;
	glm::vec4 *m_state[4];

public:
	inline int GetDimension() { return m_dimension; }
	inline glm::vec4* GetState(int i) { return m_state[i]; }

private:
	void InitializeGrid(glm::vec4 *state);
};

}