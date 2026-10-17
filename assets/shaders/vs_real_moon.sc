$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): the square that holds the moon that looks like the real moon and its halo
// (graphics::real_moon). a_texcoord0: the place across the square, in numbers of the moon's radius from its middle,
// along the moon's east and north.

void main()
{
	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0f));
}
