$input a_position, a_texcoord0, a_color0, a_color1
$output v_texcoord0, v_color0, v_haze

#include <bgfx_shader.sh>

// One corner of a particle ribbon, already put in its place in the world for this pass

void main()
{
	v_texcoord0 = vec4(a_texcoord0, 0.0, 0.0);
	v_color0 = a_color0;
	// Each corner adds the specular colour of its joint after the texture
	v_haze = vec4(a_color1.rgb, 0.0);
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0));
}
