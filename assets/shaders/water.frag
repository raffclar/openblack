#version 150 core

varying vec4 screenSpacePosition;

uniform sampler2D sReflection;

out vec4 outColor;

void main()
{

	vec3 reflect_colour = texture2DProj(sReflection, screenSpacePosition).rgb;
	vec3 water_color = vec3(0.35, 0.612, 0.643);

	outColor = vec4(mix(reflect_colour, water_color, 0.8), 1.0);
}
