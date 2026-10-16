$input a_position, i_data0
$output v_texcoord0, v_color0, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the spray that a wave throws up as it hits the foot of a cliff. Each instance is a point
// at the foot of a cliff (i_data0: xy where it is across the ground, zw the direction out to sea) with 64 particles in
// four layers: a white sheet of water that climbs the rock and breaks into lumps, a dense core, large drops that arc
// out and fall back, stretched along their motion, and a slow mist that the wind and the current carry. The burst
// starts when the shore wave reaches the cliff, and each crash has its own height, width, delay and direction.

SAMPLER2D(s_shore, 2);
// rgb: the colour of the sky
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

	// The crash: when the shore wave reaches the coast here, and which crash it is
	float omega = mix(1.1f, 1.5f, storm);
	float theta = omega * t + UwNoise(foot * 0.008f) * 1.2f;
	float period = 6.2831853f / omega;
	float crash = floor(theta / 6.2831853f);
	float crashSeed = crash * 3.7f + dot(foot, vec2(0.0173f, 0.0291f));
	// Each crash is a little different: higher or lower, wider or narrower, later, turned
	float crashHeight = 0.6f + 0.8f * SprayHash(crashSeed + 11.0f);
	float crashWidth = 0.6f + 0.8f * SprayHash(crashSeed + 12.0f);
	float crashDelay = SprayHash(crashSeed + 13.0f) * 0.3f;
	float crashTurn = (SprayHash(crashSeed + 14.0f) - 0.5f) * 0.8f;
	vec2 away = vec2(outward.x * cos(crashTurn) - outward.y * sin(crashTurn),
	                 outward.x * sin(crashTurn) + outward.y * cos(crashTurn));
	vec2 side = vec2(-away.y, away.x);

	// How hard the wave hits: with its height, a storm and the height of the tide on the cliff
	float tide = clamp((u_uwParams.z + UW_LOWEST_TIDE) / UW_LOWEST_TIDE, 0.0f, 1.0f);
	float power = u_uwWaves.w * (0.7f + 0.3f * tide) * cliff * crashHeight;

	// The layers: 0 to 15 the sheet, 16 to 39 the core, 40 to 55 the drops, 56 to 63 the mist
	float layer = index < 16.0f ? 0.0f : (index < 40.0f ? 1.0f : (index < 56.0f ? 2.0f : 3.0f));
	float seed = index * 7.13f + crashSeed;
	float h1 = SprayHash(seed + 1.0f);
	float h2 = SprayHash(seed + 2.0f);
	float h3 = SprayHash(seed + 3.0f);
	float since = fract(theta / 6.2831853f) * period - crashDelay - h1 * 0.15f;
	if (layer > 2.5f)
	{
		// The mist stays long after the crash, from the crash before as well
		since = fract(theta / 6.2831853f) * period + period * h1;
	}
	vec3 base = vec3(foot.x, u_uwParams.z, foot.y) + vec3(side.x, 0.0f, side.y) * (h3 - 0.5f) * 6.0f * crashWidth;
	vec3 velocity;
	float life;
	float size;
	float opacity;
	if (layer < 0.5f)
	{
		// The sheet climbs the face of the rock and falls back, in lumps
		velocity = vec3(away.x, 0.0f, away.y) * (0.3f + 0.6f * h2) +
		           vec3(0.0f, (4.0f + 14.0f * power) * (0.7f + 0.3f * h2), 0.0f);
		life = 1.4f + storm;
		size = 0.8f + 1.6f * max(since, 0.0f);
		opacity = 0.85f;
	}
	else if (layer < 1.5f)
	{
		// The dense core, thrown up and out
		velocity = vec3(away.x, 0.0f, away.y) * (0.5f + 3.0f * h2) + vec3(side.x, 0.0f, side.y) * (h3 - 0.5f) * 3.0f +
		           vec3(0.0f, (3.0f + 18.0f * power) * (0.5f + 0.5f * h1), 0.0f);
		life = 1.6f + storm;
		size = 0.25f + 0.9f * max(since, 0.0f);
		opacity = 0.7f;
	}
	else if (layer < 2.5f)
	{
		// The large drops that arc out and fall back
		velocity = vec3(away.x, 0.0f, away.y) * (2.0f + 5.0f * h2) + vec3(side.x, 0.0f, side.y) * (h3 - 0.5f) * 4.0f +
		           vec3(0.0f, (4.0f + 16.0f * power) * (0.6f + 0.4f * h1), 0.0f);
		life = 2.2f;
		size = 0.06f + 0.06f * h3;
		opacity = 0.9f;
	}
	else
	{
		// The mist drifts with the wind and the current
		vec2 shift;
		vec2 flow = UwCurrent(base, t, shift);
		vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z)) * (0.6f + 2.0f * storm);
		velocity = vec3(flow.x + wind.x, 0.4f + 0.6f * h2, flow.y + wind.y);
		life = 5.0f + 3.0f * storm;
		size = 2.0f + 3.0f * max(since, 0.0f) * (0.5f + h2);
		opacity = 0.18f * smoothstep(0.15f, 0.5f, power);
	}
	vec3 world = base + velocity * since;
	if (layer < 2.5f)
	{
		world.y -= 4.9f * since * since;
	}

	float alpha = since > 0.0f && since < life ? (1.0f - since / life) : 0.0f;
	alpha *= smoothstep(0.0f, 0.08f, since);
	alpha *= world.y > u_uwParams.z - 0.5f ? 1.0f : 0.0f;
	alpha *= power > 0.02f ? opacity : 0.0f;
	size *= 0.6f + 0.6f * SprayHash(seed + 5.0f);

	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 toEye = normalize(u_uwEye.xyz - world);
	vec3 placed;
	if (layer > 1.5f && layer < 2.5f)
	{
		// A fast drop is a streak along its motion
		vec3 moving = velocity + vec3(0.0f, -9.8f * since, 0.0f);
		vec3 along = normalize(moving - toEye * dot(moving, toEye) + vec3(1e-4f, 0.0f, 0.0f));
		vec3 across = normalize(cross(along, toEye));
		float stretch = 1.0f + 0.06f * length(moving);
		placed = world + along * corner.y * size * stretch * 2.5f + across * corner.x * size;
	}
	else
	{
		placed = world + (right * corner.x + up * corner.y) * size;
	}

	// Lit as a lump of white water: brighter on the side towards the sun or the moon, and lighter where the light
	// comes through a thin part towards the eye
	vec3 normal = normalize(right * corner.x + up * corner.y + toEye * 0.8f);
	vec3 towards = u_uwAbove.xyz;
	float lit = 0.45f + 0.55f * max(dot(normal, towards), 0.0f);
	float through = pow(max(dot(-toEye, towards), 0.0f), 4.0f) * (layer > 2.5f ? 0.3f : 0.6f);
	vec3 white = vec3_splat(0.92f);
	vec3 light = u_uwLightColour.rgb * 1.2f + u_uwSky.rgb * 0.8f + u_uwScatter.rgb * 0.4f;
	vec3 colour = mix(white, white * light, 0.55f) * lit + u_uwLightColour.rgb * through;

	v_texcoord0 = vec4(corner, layer, alpha);
	v_color0 = vec4(min(colour, vec3_splat(1.2f)), 1.0f);
	v_world = placed;
	gl_Position = alpha > 0.0f ? mul(u_viewProj, vec4(placed, 1.0f)) : vec4(0.0f, 0.0f, 2.0f, 1.0f);
}
