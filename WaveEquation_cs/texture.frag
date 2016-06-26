#version 330

layout (location = 0) out vec4 final_color;

//uniform sampler2D u_base_texture;

in vec2 v_tex_coord;

void main(void) {
	//float z = texture(u_base_texture, v_tex_coord).r;
	
	final_color = vec4(1.0, 1.0, 1.0, 1.0);
}
