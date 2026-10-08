$input a_position, a_color0
$output v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

uniform vec4 u_seaMode; // x: 0 the screen rows, 1 the level-0 world quad

void main()
{
	v_texcoord1 = vec4_splat(0.0f);
	if (u_seaMode.x < 0.5f)
	{
		// The rows span the whole screen width; a_position is the full-screen quad in clip space and
		// fs_water finds the rows, their points on the sea plane and the part of the screen they cover
		gl_Position = vec4(a_position.x, a_position.y, 0.5f, 1.0f);
		v_texcoord0 = vec4_splat(0.0f);
	}
	else
	{
		vec4 vertex = vec4(vec3(a_position.x, 0.0, a_position.y), 1.0);
		vec4 viewSpacePos = mul(u_view, vertex);
		gl_Position = mul(u_viewProj, vertex);
		// world x/z and view depth; the fragment shader builds the UVs
		v_texcoord0 = vec4(vertex.x, vertex.z, viewSpacePos.z, 0.0f);
	}
}
