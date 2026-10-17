$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "clouds.sh"

// EXPERIMENT (not for merge): the map of the clouds' shadow round the eye, seen from above. Each texel follows the ray
// from the ground at the sea's level towards the sun or the moon through the layer. Red is the light that comes
// through, 1 with no cloud. The land's shade and the water read it.

// xy: where the map starts; z: the units that it covers; w: how dark the shadow is, 0 to 1
uniform vec4 u_cloudShadow;

void main()
{
	vec2 xz = u_cloudShadow.xy + gl_FragCoord.xy * u_viewTexel.xy * u_cloudShadow.z;
	vec3 towards = u_cloudLight.xyz;
	towards.y = max(towards.y, 0.2f);
	towards = normalize(towards);
	float ground = u_cloudLayer.w;
	float enter = (u_cloudLayer.x - CLOUD_LOWEST_DROP - ground) / towards.y;
	float leave = (u_cloudLayer.x + CLOUD_CUMULONIMBUS_DEPTH - ground) / towards.y;
	float dt = (leave - enter) / 12.0f;
	float depth = 0.0f;
	for (int i = 0; i < 12; ++i)
	{
		vec3 p = vec3(xz.x, ground, xz.y) + towards * (enter + dt * (float(i) + 0.5f));
		depth += CloudDensity(p, p.y, false) * dt;
	}
	float through = exp(-depth * CLOUD_EXTINCTION * 0.6f);
	float lit = mix(1.0f, through, u_cloudShadow.w);
	gl_FragColor = vec4(lit, lit, lit, 1.0f);
}
