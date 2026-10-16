$input a_position, i_data0
$output v_texcoord0, v_texcoord1, v_color0, v_normal, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the spray that a wave throws up as it hits the foot of a cliff. Each instance is a point
// at the foot of a cliff (i_data0: xy where it is across the ground, zw the direction out to sea) with 128 particles in
// five layers:
// - 0 to 7: a sheet of white water that climbs the face of the rock in panels, curls out at the top and breaks into
//   strands as it falls,
// - 8 to 39: a dense core of white water thrown up and out,
// - 40 to 87: large drops that arc out and fall back, stretched along their motion,
// - 88 to 111: a fine mist that rises from the crash and drifts,
// - 112 to 127: a haze at the foot of the cliff that the wind and the current move.
// The crash is the shore wave's (UwCliffCrash): each crash has its own strength, from the wave's own height, the swell
// under it and the breakers at that place, and its own point of impact along the cliff. Far cliffs draw fewer layers.

SAMPLER2D(s_shore, 2);
// rgb: the colour of the sky
uniform vec4 u_uwSky;

float SprayHash(float n)
{
	return fract(sin(n * 12.9898f) * 43758.5453f);
}

// How high the swell is at a point and time, about -1 to 1 times its height: the two long waves of the wave table
float SpraySwell(vec2 p, float t)
{
	vec2 sets = UwWaveSets(p, t);
	float height = 0.0f;
	float top = 0.0f;
	for (int i = 0; i < 2; i++)
	{
		vec4 wave = u_uwWaveTable[i];
		vec2 along = vec2(cos(wave.x), sin(wave.x));
		float k = 6.2831853f / wave.y;
		float omega = sqrt(9.8f * k) * 0.8f;
		height += wave.z * sets.y * sin(k * dot(along, p) - omega * t + wave.w);
		top += wave.z;
	}
	return top > 0.0f ? height / top : 0.0f;
}

void main()
{
	vec2 foot = i_data0.xy;
	vec2 outward = i_data0.zw;
	vec2 corner = a_position.xy;
	float index = a_position.z;
	float t = u_uwParams.y;
	float level = u_uwParams.z;
	float storm = u_uwWaves.y;
	vec4 hidden = vec4(0.0f, 0.0f, 2.0f, 1.0f);

	float layer = index < 8.0f ? 0.0f : (index < 40.0f ? 1.0f : (index < 88.0f ? 2.0f : (index < 112.0f ? 3.0f : 4.0f)));

	// Fewer layers for far cliffs, and none behind the eye
	vec3 eye = u_uwEye.xyz;
	vec3 forward = normalize(mul(u_invView, vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz);
	vec3 toFoot = vec3(foot.x, level, foot.y) - eye;
	float range = length(toFoot);
	bool culled = dot(toFoot, forward) < -40.0f || range > 2600.0f;
	culled = culled || (layer > 1.5f && layer < 3.5f && range > 1100.0f);
	culled = culled || (layer > 1.5f && layer < 3.5f && range > 450.0f && fract(index * 0.5f) > 0.25f);
	culled = culled || (layer > 3.5f && range > 1800.0f) || (layer > 2.5f && u_uwFlags2.w < 0.5f);

	vec4 shore = texture2DLod(s_shore, UwShoreUv(foot), 0.0f);
	float cliff = UwShoreCliff(shore);
	float since;
	float crash;
	float period;
	float strength = UwCliffCrash(foot, t, since, crash, period);
	float crashSeed = crash * 3.7f + dot(foot, vec2(0.0173f, 0.0291f));

	// Each crash is turned a little, and its wave hits hardest at one point along the cliff
	float crashWidth = 0.7f + 0.6f * SprayHash(crashSeed + 12.0f);
	float crashTurn = (SprayHash(crashSeed + 14.0f) - 0.5f) * 0.7f;
	vec2 away = vec2(outward.x * cos(crashTurn) - outward.y * sin(crashTurn),
	                 outward.x * sin(crashTurn) + outward.y * cos(crashTurn));
	vec2 side = vec2(-away.y, away.x);
	float impact = (SprayHash(crashSeed + 15.0f) - 0.5f) * 6.0f;

	// How hard the wave hits: its strength, the swell under it as it reached the cliff, the tide on the cliff and the
	// storm
	float swell = SpraySwell(foot + outward * 6.0f, t - since);
	float tide = clamp((level + UW_LOWEST_TIDE) / UW_LOWEST_TIDE, 0.0f, 1.0f);
	float power = u_uwWaves.w * (0.7f + 0.3f * tide) * cliff * (strength / 0.55f) * (1.0f + 0.35f * swell);

	float seed = index * 7.13f + crashSeed;
	float h1 = SprayHash(seed + 1.0f);
	float h2 = SprayHash(seed + 2.0f);
	float h3 = SprayHash(seed + 3.0f);
	float h4 = SprayHash(seed + 4.0f);

	// Where along the cliff the particle starts: near the point of impact, and stronger there. The sheet's panels meet
	// edge to edge, and each corner takes the strength at its own place, so the sheet is one ribbon along the cliff.
	float sheetSpan = 16.0f * crashWidth;
	float spread = layer < 0.5f ? ((index + 0.5f + corner.x * 0.5f) / 8.0f - 0.5f) * sheetSpan : (h3 - 0.5f) * 14.0f;
	float alongCliff = impact * (layer < 0.5f ? 0.3f : 1.0f) + spread * (layer < 0.5f ? 1.0f : crashWidth);
	float local = power * (0.55f + 0.9f * UwNoise((foot + side * alongCliff) * 0.09f + crash)) *
	              (1.0f - 0.45f * min(abs(alongCliff - impact) / 8.0f, 1.0f));
	vec3 awayDir = vec3(away.x, 0.0f, away.y);
	vec3 sideDir = vec3(side.x, 0.0f, side.y);
	vec3 upDir = vec3(0.0f, 1.0f, 0.0f);
	vec3 base = vec3(foot.x, level, foot.y) + sideDir * alongCliff;

	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;

	vec3 world;
	vec3 placed;
	vec3 normal;
	float alpha;
	float age;
	// How far the particle has broken up, 0 to 1
	float breakup;
	if (layer < 0.5f)
	{
		// The sheet: a ribbon up the face that rises with the wave's push and falls back under its own weight
		float rise = 7.0f + 22.0f * local;
		float apex = rise / 9.8f;
		float peak = rise * rise / 19.6f;
		float life = 2.0f * apex + 0.6f;
		age = since / life;
		breakup = smoothstep(0.55f * apex, 1.7f * apex, since);
		// The sheet is the height it reached; after the top its water falls back down the face as strands
		float height = since < apex ? max(rise * since - 4.9f * since * since, 0.0f) : peak;
		float bottom = since < apex ? 0.0f : 4.9f * (since - apex) * (since - apex);
		float v = corner.y * 0.5f + 0.5f;
		float y = mix(min(bottom, height), height + 0.8f, v);
		// It curls out at the top, more as it breaks
		float curl = v * v * (0.6f + 2.2f * breakup) * (0.4f + local);
		world = base + awayDir * (0.4f + 0.6f * since);
		placed = world + upDir * y + awayDir * curl;
		normal = normalize(awayDir + upDir * (-v * 0.6f * breakup));
		alpha = since < life && height - bottom > 0.3f ? 1.0f - smoothstep(0.65f, 1.0f, age) : 0.0f;
		// The sheet seen edge on fades: the core covers it there
		alpha *= 0.95f * smoothstep(0.05f, 0.35f, abs(dot(normal, normalize(eye - world))));
		// x: across the whole ribbon, -1 to 1, so that its two ends fade
		corner.x = spread / (sheetSpan * 0.5f);
		v_texcoord1 = vec4(alongCliff * 0.07f + crashSeed * 0.013f, y * 0.05f - since * 0.12f, crashSeed * 0.031f, breakup);
	}
	else
	{
		vec3 velocity;
		float life;
		float size;
		float opacity;
		if (layer < 1.5f)
		{
			// The dense core, thrown up and out
			velocity = awayDir * (0.6f + 3.0f * h2) + sideDir * (h4 - 0.5f) * 3.0f +
			           upDir * (4.0f + 24.0f * local) * (0.5f + 0.5f * h1);
			life = 1.4f + 0.5f * h4 + storm;
			since -= h1 * 0.15f;
			size = 0.6f + 3.2f * max(since, 0.0f) * (0.6f + 0.6f * h3);
			opacity = 0.75f;
		}
		else if (layer < 2.5f)
		{
			// The large drops that arc out and fall back
			velocity = awayDir * (2.0f + 6.0f * h2) + sideDir * (h4 - 0.5f) * 5.0f +
			           upDir * (5.0f + 24.0f * local) * (0.55f + 0.45f * h1);
			life = 3.0f;
			since -= 0.05f + h1 * 0.25f;
			size = 0.07f + 0.08f * h3;
			opacity = 0.95f;
		}
		else if (layer < 3.5f)
		{
			// The fine mist: it rises from the crash as the heavy water falls, and drifts on the wind
			vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z)) * (0.8f + 2.5f * storm);
			velocity = awayDir * (0.5f + 1.5f * h2) + vec3(wind.x, 0.0f, wind.y) +
			           upDir * (2.0f + 9.0f * local) * (0.5f + 0.5f * h1);
			life = 3.5f + 2.0f * storm;
			since -= 0.2f + 0.3f * h4;
			size = 1.2f + 2.2f * max(since, 0.0f) * (0.5f + h3);
			opacity = 0.22f * smoothstep(0.05f, 0.35f, local);
		}
		else
		{
			// The haze at the foot: always there in some amount, more with bigger waves and a storm. Each puff lives
			// longer than a wave, starts at its own time and goes where the current and the wind take it.
			float cycle = 12.0f + 6.0f * SprayHash(index * 3.1f + foot.x);
			float shifted = (t + cycle * SprayHash(index * 1.7f + foot.y)) / cycle;
			float puffSeed = floor(shifted);
			since = fract(shifted) * cycle;
			float p1 = SprayHash(index * 5.3f + puffSeed * 1.9f + foot.x * 0.01f);
			float p2 = SprayHash(index * 2.9f + puffSeed * 7.1f + foot.y * 0.01f);
			base = vec3(foot.x, level + 0.6f + 1.4f * p2, foot.y) + sideDir * (p1 - 0.5f) * 22.0f +
			       vec3(outward.x, 0.0f, outward.y) * (1.0f + 6.0f * p2);
			vec2 shift;
			vec2 flow = UwCurrent(base, t, shift);
			vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z)) * (0.5f + 2.0f * storm);
			velocity = vec3(flow.x + wind.x, 0.12f, flow.y + wind.y);
			life = cycle;
			size = 7.0f + 6.0f * p1 + 0.3f * since;
			float average = u_uwWaves.w * cliff * (0.7f + 0.3f * tide);
			opacity = (0.07f + 0.18f * storm) * smoothstep(0.1f, 0.5f, average);
			h1 = p1;
		}
		world = base + velocity * since;
		if (layer < 2.5f)
		{
			world.y -= 4.9f * since * since;
		}
		age = since / life;
		alpha = since > 0.0f && since < life ? (1.0f - age) : 0.0f;
		alpha *= layer > 3.5f ? smoothstep(0.0f, 0.2f, age) : smoothstep(0.0f, 0.06f, since);
		alpha *= world.y > level - 0.5f ? opacity : 0.0f;
		size *= 0.7f + 0.6f * SprayHash(seed + 5.0f);
		breakup = layer < 1.5f ? smoothstep(0.25f, 0.9f, age) : 0.0f;

		vec3 toEye = normalize(eye - world);
		if (layer > 1.5f && layer < 2.5f)
		{
			// A fast drop is a streak along its motion
			vec3 moving = velocity + vec3(0.0f, -9.8f * since, 0.0f);
			vec3 along = normalize(moving - toEye * dot(moving, toEye) + vec3(1e-4f, 0.0f, 0.0f));
			vec3 across = normalize(cross(along, toEye));
			float stretch = 1.0f + 0.09f * length(moving);
			placed = world + along * corner.y * size * stretch * 2.2f + across * corner.x * size;
		}
		else
		{
			// The puffs turn slowly as they go
			float spin = (h2 - 0.5f) * 1.5f * since + h4 * 6.2831853f;
			vec2 turned = vec2(corner.x * cos(spin) - corner.y * sin(spin), corner.x * sin(spin) + corner.y * cos(spin));
			placed = world + (right * turned.x + up * turned.y) * size;
		}
		normal = toEye;
		// The noise moves through the puff as it grows; the haze's moves on slowly
		vec3 drift = layer > 3.5f ? vec3(t * 0.04f, -t * 0.03f, 0.0f) : vec3(0.0f, -since * 0.25f, 0.0f);
		v_texcoord1 = vec4(vec3(h1 * 7.0f, h2 * 7.0f, h3 * 7.0f) + drift, breakup);
	}

	alpha *= power > 0.02f || layer > 3.5f ? 1.0f : 0.0f;
	alpha = culled ? 0.0f : alpha;

	// The light that comes to the particle from all directions: the sky and the sea's colour. The direct light is
	// added in the fragments.
	vec3 ambient = u_uwSky.rgb * 1.15f + u_uwScatter.rgb * 0.3f + u_uwLightColour.rgb * 0.3f;
	v_texcoord0 = vec4(corner, layer, alpha);
	v_color0 = vec4(ambient, age);
	v_normal = normal;
	v_world = placed;
	gl_Position = alpha > 0.001f ? mul(u_viewProj, vec4(placed, 1.0f)) : hidden;
}
