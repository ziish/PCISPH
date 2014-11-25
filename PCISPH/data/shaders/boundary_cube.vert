#version 430

in vec3 cube_pos; 
in vec3 pos; 
in vec3 vert_normal; 

uniform mat4 trans;
uniform float cube_size;

out vec3 frag_normal;

void main() { 
	frag_normal = vert_normal;
	gl_Position = trans * vec4(cube_pos + 0.5f * cube_size * pos, 1.0); 
}