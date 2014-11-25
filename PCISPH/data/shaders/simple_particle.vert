#version 430

in vec3 particle_pos; 
//in vec3 particle_normal; 
in vec3 pos; 
in vec3 vert_normal; 
in float color_factor;

uniform mat4 trans;
uniform float particle_radius;
uniform float color_factor_min;
uniform float color_factor_neutral;
uniform float color_factor_max;

out float discard_fragment;
out vec3 frag_normal; 
out vec3 diffuse_color; 

void main() { 
	vec3 max_color = vec3(1.f, 0.3f, 0.f);
	vec3 neutral_color = vec3(0.5f, 0.3f, 0.5f);
	vec3 min_color = vec3(0.f, 0.3f, 1.f);

	vec3 offset_color;
	if(color_factor > color_factor_neutral) {
		float f = clamp((color_factor - color_factor_neutral) / (color_factor_max - color_factor_neutral), 0.f, 1.f);
		diffuse_color = mix(neutral_color, max_color, f);
	}
	else {
		float f = clamp((color_factor - color_factor_min) / (color_factor_neutral - color_factor_min), 0.f, 1.f);
		diffuse_color = mix(min_color, neutral_color, f);
	}
	
	float scale = 1.0f;
	frag_normal = vert_normal;
	//frag_normal = particle_normal;
	gl_Position = trans * vec4(particle_pos + scale * particle_radius * pos, 1.0); 
}