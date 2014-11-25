#version 430

in float discard_fragment;
in vec3 frag_normal; 
in vec3 diffuse_color; 
out vec4 outColor; 

void main() { 
	float near = 5.f;
	float far = 20.f;

	vec3 ambient = vec3( 0.2, 0.2, 0.2);
	float light = max(0.0, dot(frag_normal, vec3(0.0, 1.0, 0.0)));
	
	float depth_falloff = 1.0 - (gl_FragCoord.z / gl_FragCoord.w - near) / (far - near);

	vec3 color = light * diffuse_color + ambient;
	outColor = vec4( color * depth_falloff, 1.0 ); 
}