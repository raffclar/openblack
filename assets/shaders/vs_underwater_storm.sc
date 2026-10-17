$input a_position, i_data0
$output v_texcoord0, v_texcoord1, v_color0, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the spray of the storm on the sea, 900 particles for each instance. i_data0.x is the
// kind of the instance:
// - 0: the rogue wave (u_uwRogue). Particles 0 to 599 are the spray that the wind blows off its crest; 600 to 899 are
//   the white water that falls down its face as it breaks.
// - 1: a strike of lightning on the sea (u_uwStrike). Particles 0 to 499 are the steam that rises from the point;
//   500 to 899 are the spray that the strike throws out and up.

// rgb: the colour of the sky
uniform vec4 u_uwSky;

SAMPLER2D(s_shore, 2);

float StormHash(float n)
{
	return fract(sin(n * 12.9898f) * 43758.5453f);
}

void main()
{
	vec2 corner = a_position.xy;
	float index = a_position.z;
	float kind = i_data0.x;
	float t = u_uwParams.y;
	float level = u_uwParams.z;
	float seed = index * 3.17f + kind * 57.1f;
	float h1 = StormHash(seed + 1.0f);
	float h2 = StormHash(seed + 2.0f);
	float h3 = StormHash(seed + 3.0f);
	float h4 = StormHash(seed + 4.0f);
	vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));

	vec3 world = vec3_splat(0.0f);
	float size = 1.0f;
	float alpha = 0.0f;
	float grey = 0.85f;
	if (kind < 0.5f)
	{
		vec4 a = u_uwRogue[0];
		vec4 b = u_uwRogue[1];
		vec2 along = vec2(cos(a.z), sin(a.z));
		vec2 across = vec2(-along.y, along.x);
		// A point on the crest line, where the crest bends back towards its ends
		float side = (h1 * 2.0f - 1.0f) * 1.5f;
		float ends = exp(-(side * side * side * side));
		vec2 crest = a.xy + across * (side * b.y) - along * (b.w * b.x * side * side);
		vec4 shore = UwOpenSea(texture2DLod(s_shore, UwShoreUv(crest), 0.0f), crest);
		float fade = smoothstep(60.0f, 200.0f, shore.x) * smoothstep(3.0f, 15.0f, level - shore.w);
		float top = level + a.w * fade * ends;
		float strength = b.z * ends * fade;
		if (index < 600.0f)
		{
			// The spray goes up off the crest and the wind blows it ahead of the wave and to the side
			float life = 1.4f + 0.8f * h3;
			float f = fract(h2 + t / life);
			float age = f * life;
			vec2 drift = along * (2.0f * age) + wind * (5.0f * age) + across * ((h4 - 0.5f) * 5.0f * age);
			world = vec3(crest.x + drift.x, top + (2.5f + 4.0f * h4) * age - 1.6f * age * age, crest.y + drift.y);
			size = mix(2.0f, 9.0f, f) * (0.7f + 0.6f * h3);
			alpha = 0.45f * strength * smoothstep(0.0f, 0.08f, f) * (1.0f - f);
		}
		else
		{
			// The white water falls from the crest down the steep face to the trough in front
			float life = 1.0f + 0.4f * h3;
			float f = fract(h2 + t / life);
			float age = f * life;
			vec2 ahead = along * (2.0f + 9.0f * age) + across * ((h4 - 0.5f) * 3.0f);
			world = vec3(crest.x + ahead.x, top - 7.0f * age * age, crest.y + ahead.y);
			size = (2.0f + 3.0f * h4) * (0.8f + 0.6f * f);
			alpha = world.y > level - 0.5f ? 0.55f * strength * smoothstep(0.0f, 0.1f, f) * (1.0f - f) : 0.0f;
			grey = 0.95f;
		}
	}
	else
	{
		vec3 base = u_uwStrike[0].xyz;
		float since = u_uwStrike[1].y;
		float strength = u_uwStrike[1].z;
		if (index < 500.0f)
		{
			// The steam: each puff starts a little after the strike, rises, spreads and drifts with the wind
			float age = since - h1 * 0.4f;
			float angle = h4 * 6.2831853f;
			float radius = (1.5f + 4.0f * h3) * (0.5f + age);
			float rise = (5.0f + 7.0f * h2) * pow(max(age, 0.0f), 0.7f);
			world = base + vec3(cos(angle) * radius + wind.x * 2.5f * age, rise, sin(angle) * radius + wind.y * 2.5f * age);
			size = (2.0f + 3.0f * h3) * (1.0f + 1.2f * max(age, 0.0f));
			alpha = age > 0.0f ? 0.28f * strength * smoothstep(0.0f, 0.15f, age) * (1.0f - smoothstep(1.5f, 4.0f, age)) : 0.0f;
			grey = 0.78f;
		}
		else
		{
			// The spray: thrown out and up from the point, it falls back under its weight
			float age = since - h1 * 0.08f;
			float life = 1.6f;
			float angle = h4 * 6.2831853f;
			vec2 out2 = vec2(cos(angle), sin(angle)) * (6.0f + 14.0f * h2);
			world = base + vec3(out2.x * age, (8.0f + 10.0f * h3) * age - 4.9f * age * age, out2.y * age);
			size = 0.8f + 1.5f * h4;
			alpha = age > 0.0f && age < life && world.y > level - 0.3f ? 0.5f * strength * (1.0f - age / life) : 0.0f;
			grey = 0.95f;
		}
	}

	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 placed = world + (right * corner.x + up * corner.y) * size;
	vec3 toEye = normalize(u_uwEye.xyz - world);
	float lit = 0.55f + 0.45f * max(dot(normalize(right * corner.x + up * corner.y + toEye), u_uwAbove.xyz), 0.0f);
	// Water drops scatter the light forward: the spray is brighter with the sun or the moon behind it
	float through = pow(max(dot(-toEye, u_uwAbove.xyz), 0.0f), 4.0f) * 0.8f;
	vec3 colour = mix(vec3_splat(grey), u_uwSky.rgb * 1.6f + u_uwLightColour.rgb * 0.6f, 0.5f) * lit +
	              u_uwLightColour.rgb * through;
	// The lightning lights the spray and the steam
	colour += UW_FLASH_COLOUR * (u_uwStrike[1].x * 0.8f + u_uwStrike[0].w * 0.6f) * u_uwStrike[1].w;

	v_texcoord0 = vec4(corner, size, alpha);
	v_texcoord1 = vec4(h1 * 6.0f, h2 * 6.0f, t * 0.05f + h3 * 6.0f, 0.0f);
	v_color0 = vec4(colour, 1.0f);
	v_world = placed;
	gl_Position = alpha > 0.002f ? mul(u_viewProj, vec4(placed, 1.0f)) : vec4(0.0f, 0.0f, 2.0f, 1.0f);
}
