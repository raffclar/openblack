$input a_position, a_normal
$output v_world, v_normal

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): the sea bed past the land's blocks, out to the map's edge. Its corners are in the world
// already, with their normals.

void main()
{
	v_world = a_position.xyz;
	v_normal = a_normal;
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0f));
}
