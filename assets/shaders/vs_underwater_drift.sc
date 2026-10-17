$input a_position, i_data0, i_data1
$output v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): a leaf or a twig that drifts on the water of a river or the sea. Each instance is one
// (i_data0: xyz where it floats, w which way it is turned in radians; i_data1: x its length, y its kind from 0 to 1,
// z how much it shows, w unused). It lies flat on the water.

void main()
{
	vec2 corner = a_position.xy;
	float angle = i_data0.w;
	float length_ = i_data1.x;
	float kind = i_data1.y;
	// A twig is long and thin; a leaf is wider
	float width = kind < 0.7f ? 0.6f : 0.18f;
	vec3 along = vec3(cos(angle), 0.0f, sin(angle));
	vec3 across = vec3(-along.z, 0.0f, along.x);
	vec3 world = i_data0.xyz + (along * corner.x + across * corner.y * width) * length_ * 0.5f;
	v_texcoord0 = vec4(corner, kind, i_data1.z);
	v_texcoord1 = vec4(world, 0.0f);
	gl_Position = i_data1.z > 0.001f ? mul(u_viewProj, vec4(world, 1.0f)) : vec4(0.0f, 0.0f, 2.0f, 1.0f);
}
