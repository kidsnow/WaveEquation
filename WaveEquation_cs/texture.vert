#version 330

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_tex_coord;

out vec2 v_tex_coord;

void main(void) {
	v_tex_coord = a_tex_coord;
    gl_Position =  vec4(a_position, 1.0);
}
