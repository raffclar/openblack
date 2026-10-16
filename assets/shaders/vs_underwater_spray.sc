$input a_position, i_data0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the spray that a wave throws up as it hits the foot of a cliff. Each instance is a point
// at the foot of a cliff (i_data0: xy where it is across the ground, zw the direction out to sea) with sixteen puffs.
// The puffs leave as the shore wave reaches the cliff, so the spray goes up in time with the waves. They go higher
// as the waves get higher, with the tide, and in a storm.

SAMPLER2D(s_shore, 2);
// rgb: the sky's colour
uniform vec4 u_uwSky;

float SprayHash(float n)
{
	return fract(sin(n * 12.9898f) * 43758.5453f);
}

void main()
{
	vec2 foot = i_data0.xy;
	vec2 outward = i_data0.zw;
	vec2 corner = a_position.xy;
	float index = a_position.z;
	float t = u_uwParams.y;
	float storm = u_uwWaves.y;
	vec4 shore = texture2DLod(s_shore, UwShoreUv(foot), 0.0f);
	float cliff = UwShoreCliff(shore);

	// The time since the shore wave last reached the coast here
	float omega = mix(1.1f, 1.5f, storm);
	float theta = omega * t + UwNoise(foot * 0.008f) * 1.2f;
	float period = 6.2831853f / omega;
	float seed = index * 7.13f + dot(foot, vec2(0.173f, 0.291f));
	float since = fract(theta / 6.2831853f) * period - SprayHash(seed + 1.0f) * 0.2f;
	float life = 1.6f + storm;

	// How hard the wave hits: with its height, a storm and the tide's height on the cliff
	float tide = clamp((u_uwParams.z + UW_LOWEST_TIDE) / UW_LOWEST_TIDE, 0.0f, 1.0f);
	float power = u_uwWaves.w * (0.7f + 0.3f * tide) * cliff;
	float rise = (2.0f + 16.0f * power) * (0.6f + 0.4f * SprayHash(seed + 2.0f));
	vec2 side = vec2(-outward.y, outward.x) * (SprayHash(seed + 3.0f) - 0.5f) * 3.0f;
	vec2 away = outward * (0.5f + 2.5f * SprayHash(seed + 4.0f)) + side;
	vec3 base = vec3(foot.x, u_uwParams.z, foot.y);
	vec3 world = base + vec3(away.x, rise, away.y) * since + vec3(0.0f, -4.9f * since * since, 0.0f);

	float alpha = since > 0.0f && since < life ? (1.0f - since / life) : 0.0f;
	alpha *= world.y > u_uwParams.z - 0.5f ? 1.0f : 0.0f;
	alpha *= power > 0.02f ? 0.65f : 0.0f;
	float size = (0.2f + 0.8f * since * (1.0f + storm)) * (0.6f + 0.6f * SprayHash(seed + 5.0f));
	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 placed = world + (right * corner.x + up * corner.y) * size;

	// White water, a little darker where the light is low
	vec3 lit = u_uwLightColour.rgb * 0.8f + u_uwSky.rgb * 0.9f + u_uwScatter.rgb * 0.3f;
	vec3 colour = mix(vec3_splat(0.9f), lit, 0.35f);
	v_texcoord0 = vec4(corner, 0.0f, alpha);
	v_color0 = vec4(min(colour, vec3_splat(1.0f)), 1.0f);
	gl_Position = alpha > 0.0f ? mul(u_viewProj, vec4(placed, 1.0f)) : vec4(0.0f, 0.0f, 2.0f, 1.0f);
}
