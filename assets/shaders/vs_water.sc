$input a_position, a_color0
$output v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

void main()
{
	// One triangle over the whole screen: fs_water finds the sea under each pixel, out to the horizon
	gl_Position = vec4(a_position.x, a_position.y, 0.5f, 1.0f);
	v_texcoord0 = vec4_splat(0.0f);
	v_texcoord1 = vec4_splat(0.0f);
}
