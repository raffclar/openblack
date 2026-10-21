$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

// The game's interface, drawn in screen pixels. The third value of the position is the depth buffer's value of the
// vertex, which a canvas that tests the scene's depth uses.
void main()
{
	gl_Position = mul(u_viewProj, vec4(a_position.xy, 0.0, 1.0));
	gl_Position.z = a_position.z * gl_Position.w;
	v_texcoord0 = vec4(a_texcoord0, 0.0, 0.0);
	v_color0 = a_color0;
}
