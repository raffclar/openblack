$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): the map of the sun's visibility over the land, from the height map. Each texel follows
// the way towards the sun or the moon over the land and finds the hills in the way. It also finds how much of the
// sky the ground sees, from the highest hill in eight directions.
// r: how much of the light reaches the ground, soft at the edges; g: how much of the sky the ground sees; b: the
// height under which a point over the ground is in the hills' shadow.

// The shore map: w is the height of the ground at each corner of the land
SAMPLER2D(s_shore, 0);

// xyz: the direction towards the sun or the moon; w: the sea's level
uniform vec4 u_sunLight;

float HeightAt(vec2 xz)
{
	return texture2DLod(s_shore, (xz / 10.0f + 0.5f) / 513.0f, 0.0f).w;
}

void main()
{
	vec2 corner = floor(gl_FragCoord.xy);
	vec2 here = corner * 10.0f;
	float sea = u_sunLight.w;
	// The water's surface takes the light where the ground is under it
	float ground = max(HeightAt(here), sea);
	vec3 towards = u_sunLight.xyz;
	float visibility = 1.0f;
	float ceiling = -1000.0f;
	vec2 across = towards.xz;
	float level = length(across);
	if (towards.y <= 0.0f)
	{
		visibility = 0.0f;
		ceiling = 10000.0f;
	}
	else if (level > 1e-4f)
	{
		vec2 direction = across / level;
		float rise = towards.y / level;
		// The steps grow, so that the far hills count with few samples
		float t = 8.0f;
		for (int i = 0; i < 40; ++i)
		{
			if (t > 3500.0f)
			{
				break;
			}
			float height = HeightAt(here + direction * t);
			float above = height - (ground + t * rise);
			float penumbra = 0.5f + t * 0.02f;
			visibility = min(visibility, clamp(0.5f - above / penumbra, 0.0f, 1.0f));
			ceiling = max(ceiling, height - t * rise);
			t *= 1.2f;
		}
	}

	// The sky that the ground sees: the highest hill along eight directions
	float open = 0.0f;
	for (int d = 0; d < 8; ++d)
	{
		float angle = float(d) * 0.785398f;
		vec2 direction = vec2(cos(angle), sin(angle));
		float highest = 0.0f;
		float t = 10.0f;
		for (int i = 0; i < 10; ++i)
		{
			float height = max(HeightAt(here + direction * t), sea);
			highest = max(highest, (height - ground) / t);
			t *= 1.6f;
		}
		// The share of the sky over the horizon in this direction
		open += 1.0f - highest / sqrt(1.0f + highest * highest);
	}
	gl_FragColor = vec4(visibility, open / 8.0f, ceiling, 1.0f);
}
