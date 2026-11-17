$input a_position, a_texcoord0
$output v_texcoord0, v_world

#include <bgfx_shader.sh>

void main()
{
	// x: the height that the water fades by, y: the level of its lake
	v_texcoord0 = vec4(a_texcoord0.x, a_texcoord0.y, 0.0f, 0.0f);
	v_world = a_position.xyz;
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0f));
}
