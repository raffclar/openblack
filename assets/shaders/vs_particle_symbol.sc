$input a_position, i_data0, i_data1, i_data2
$output v_texcoord0, v_color0, v_haze

#include <bgfx_shader.sh>

// One sprite of a symbol of belief per instance. The shader makes a quad from the plane's corners (-1..1 across and up).
// It is a particle sprite that faces the screen, with a height equal to its width, no offset of its origin, a square
// sheet cell, no added light and no colour of the land. Thus it needs fewer bytes than a particle sprite:
// i_data0: where it is, and half its width
// i_data1: its roll, the top left corner of its sheet cell in texture space, and the size of the cell
// i_data2: its colour and alpha
// The calculation is the same as that of vs_particle_instanced for such a sprite, thus the pixels are the same.

void main()
{
	vec2 corner = a_position.xy;
	vec4 shape = vec4(i_data0.w, i_data1.x, 0.0, 0.0);
	vec4 cell = vec4(i_data1.yz, i_data1.ww);
	float c = cos(shape.y);
	float s = sin(shape.y);
	float x = corner.x * i_data0.w - shape.z;
	// In the plane of the screen, rolled
	float y = corner.y * shape.x - shape.w;
	vec3 right = mul(u_invView, vec4(1.0, 0.0, 0.0, 0.0)).xyz;
	vec3 up = mul(u_invView, vec4(0.0, 1.0, 0.0, 0.0)).xyz;
	vec3 world = i_data0.xyz + right * (x * c + y * s) + up * (y * c - x * s);
	v_texcoord0 = vec4(cell.xy + vec2(corner.x * 0.5 + 0.5, 0.5 - corner.y * 0.5) * cell.zw, 0.0, 0.0);
	v_color0 = i_data2;
	// No light is added after the texture
	v_haze = vec4(0.0, 0.0, 0.0, 0.0);
	gl_Position = mul(u_viewProj, vec4(world, 1.0));
}
