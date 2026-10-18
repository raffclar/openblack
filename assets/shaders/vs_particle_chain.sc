$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

// One corner of a particle ribbon, already put in its place in the world for this pass

void main()
{
	v_texcoord0 = vec4(a_texcoord0, 0.0, 0.0);
	v_color0 = a_color0;
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0));
}
