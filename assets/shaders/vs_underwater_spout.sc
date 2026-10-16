$input a_position, i_data0
$output v_texcoord0, v_texcoord1, v_color0, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): a waterspout over the open sea in a storm. Each instance is one waterspout (i_data0: xy
// where its foot is, z how strong it is now, w its own number) with 900 particles: 0 to 699 are the column of spray
// that turns fast near the sea and slower up to the cloud, bends with the wind and rises; 700 to 899 are the ring of
// spray thrown out and up round its foot.

// rgb: the colour of the sky
uniform vec4 u_uwSky;

float SpoutHash(float n)
{
	return fract(sin(n * 12.9898f) * 43758.5453f);
}

void main()
{
	vec2 foot = i_data0.xy;
	float strength = i_data0.z;
	float own = i_data0.w;
	vec2 corner = a_position.xy;
	float index = a_position.z;
	float t = u_uwParams.y;
	float level = u_uwParams.z;
	float seed = index * 3.17f + own * 41.3f;
	float h1 = SpoutHash(seed + 1.0f);
	float h2 = SpoutHash(seed + 2.0f);
	float h3 = SpoutHash(seed + 3.0f);
	float h4 = SpoutHash(seed + 4.0f);
	vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));

	vec3 world;
	float size;
	float alpha;
	if (index < 700.0f)
	{
		// The column: each particle goes up it and round it, faster where the column is narrow
		float height = 260.0f * (0.7f + 0.3f * strength);
		float f = fract(h1 + t * (0.05f + 0.04f * h2));
		float radius = mix(3.0f, 45.0f, pow(f, 2.2f)) * (0.6f + 0.4f * strength) * (0.7f + 0.6f * h3);
		float angle = h4 * 6.2831853f + t * 3.0f / (1.0f + radius * 0.12f);
		// The column bends with the wind and sways
		vec2 bend = wind * f * f * 60.0f + vec2(sin(f * 3.0f + t * 0.3f + own), cos(f * 2.3f + t * 0.25f + own)) * f * 25.0f;
		world = vec3(foot.x + bend.x + cos(angle) * radius, level + f * height, foot.y + bend.y + sin(angle) * radius);
		size = mix(2.5f, 16.0f, f) * (0.7f + 0.6f * h2);
		alpha = 0.22f * strength * smoothstep(0.0f, 0.04f, f) * (1.0f - smoothstep(0.8f, 1.0f, f));
	}
	else
	{
		// The ring at the foot: spray thrown out and up, falling back under its weight
		float life = 2.5f;
		float since = fract(h1 + t / life) * life;
		float angle = h2 * 6.2831853f + t * 1.5f;
		vec2 out2 = vec2(cos(angle), sin(angle));
		float speed = 6.0f + 9.0f * h3;
		world = vec3(foot.x, level, foot.y) + vec3(out2.x, 0.0f, out2.y) * (4.0f + speed * since) +
		        vec3(0.0f, (4.0f + 7.0f * h4) * since - 4.9f * since * since, 0.0f) +
		        vec3(wind.x, 0.0f, wind.y) * since * 3.0f;
		size = (1.5f + 2.5f * h4) * (0.6f + 0.8f * since / life);
		alpha = world.y > level - 0.3f ? 0.5f * strength * (1.0f - since / life) * smoothstep(0.0f, 0.1f, since) : 0.0f;
	}

	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 placed = world + (right * corner.x + up * corner.y) * size;
	vec3 toEye = normalize(u_uwEye.xyz - world);
	float lit = 0.55f + 0.45f * max(dot(normalize(right * corner.x + up * corner.y + toEye), u_uwAbove.xyz), 0.0f);
	// Water drops scatter the light forward: the column is brighter with the sun or the moon behind it
	float through = pow(max(dot(-toEye, u_uwAbove.xyz), 0.0f), 4.0f) * 0.8f;
	vec3 colour = mix(vec3_splat(0.8f), u_uwSky.rgb * 1.6f + u_uwLightColour.rgb * 0.6f, 0.5f) * lit +
	              u_uwLightColour.rgb * through;

	v_texcoord0 = vec4(corner, size, alpha);
	v_texcoord1 = vec4(h1 * 6.0f, h2 * 6.0f, t * 0.05f + h3 * 6.0f, 0.0f);
	v_color0 = vec4(colour, 1.0f);
	v_world = placed;
	gl_Position = alpha > 0.002f ? mul(u_viewProj, vec4(placed, 1.0f)) : vec4(0.0f, 0.0f, 2.0f, 1.0f);
}
