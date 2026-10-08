$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_celestial; // x, y: cos / sin of the moon phase, z > 0: moon UVs from the phase, w > 0: separate alpha

// Sun and moon meshes: plain textured geometry placed by u_model (the atmosphere's update)
void main()
{
	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	if (u_celestial.z > 0.0f)
	{
		// the moon's UVs regenerated from its local vertices and the phase angle
		v_texcoord0.xy = vec2(a_position.x * u_celestial.x - a_position.z * u_celestial.y, a_position.y) * 0.0025f + 0.25f;
	}
	gl_Position = mul(u_viewProj, mul(u_model[0], vec4(a_position.xyz, 1.0f)));
}
