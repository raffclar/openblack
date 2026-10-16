$input a_position, i_data0, i_data1
$output v_texcoord0, v_texcoord1, v_color0, v_normal, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the splash where a thing falls into the water. Each instance is one splash (i_data0: xyz
// where it is, w when it started; i_data1: x how high its crown goes, y how wide the crown is, z how fast its drops
// leave the water, w how many drops it has). It has 160 particles in four layers, lit and shaped as the spray at the
// cliffs (fs_underwater_spray.sc):
// - 0 to 23: the crown, a ring of panels of white water that rises round the hole, opens out and breaks into strands,
// - 24 to 47: the column of water that comes up from the middle as the hole closes,
// - 48 to 127: the drops thrown up and out from the rim of the crown, stretched along their motion,
// - 128 to 159: a fine mist round the rim.

// rgb: the colour of the sky
uniform vec4 u_uwSky;

float SplashHash(float n)
{
	return fract(sin(n * 12.9898f) * 43758.5453f);
}

void main()
{
	vec3 centre = i_data0.xyz;
	float since = u_uwParams.y - i_data0.w;
	float crownHeight = i_data1.x;
	float crownRadius = i_data1.y;
	float dropSpeed = i_data1.z;
	float drops = i_data1.w;
	vec2 corner = a_position.xy;
	float index = a_position.z;
	float level = centre.y;
	vec4 hidden = vec4(0.0f, 0.0f, 2.0f, 1.0f);
	float layer = index < 24.0f ? 0.0f : (index < 48.0f ? 1.0f : (index < 128.0f ? 2.0f : 3.0f));
	float seed = index * 7.13f + i_data0.w * 3.1f + dot(centre.xz, vec2(0.0173f, 0.0291f));
	float h1 = SplashHash(seed + 1.0f);
	float h2 = SplashHash(seed + 2.0f);
	float h3 = SplashHash(seed + 3.0f);
	float h4 = SplashHash(seed + 4.0f);

	vec3 eye = u_uwEye.xyz;
	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 upDir = vec3(0.0f, 1.0f, 0.0f);
	// The crown rises at the speed that takes it to its height, and falls back under its weight
	float rise = sqrt(19.6f * max(crownHeight, 0.05f));
	float apex = rise / 9.8f;

	vec3 placed;
	vec3 world;
	vec3 normal;
	float alpha;
	float age;
	float breakup;
	if (layer < 0.5f)
	{
		// The crown: panels that meet edge to edge round the hole, each corner at its own angle
		float angle = (index + 0.5f + corner.x * 0.5f) / 24.0f * 6.2831853f;
		vec3 out_ = vec3(cos(angle), 0.0f, sin(angle));
		float life = 2.0f * apex + 0.4f;
		age = since / life;
		breakup = smoothstep(0.5f * apex, 1.5f * apex, since);
		float height = since < apex ? max(rise * since - 4.9f * since * since, 0.0f) : crownHeight;
		float bottom = since < apex ? 0.0f : 4.9f * (since - apex) * (since - apex);
		float v = corner.y * 0.5f + 0.5f;
		float y = mix(min(bottom, height), height, v);
		// The ring opens out as it rises, and its top leans out more
		float radius = crownRadius * (0.6f + 0.7f * (1.0f - exp(-since * 5.0f))) + v * v * crownHeight * (0.25f + 0.5f * breakup);
		// Each panel has a ragged height
		y *= 0.8f + 0.4f * SplashHash(floor(index) + i_data0.w);
		world = centre + out_ * radius;
		placed = world + upDir * y;
		normal = normalize(out_ + upDir * (0.3f - v * 0.6f));
		alpha = since > 0.0f && since < life && height - bottom > 0.05f ? 1.0f - smoothstep(0.6f, 1.0f, age) : 0.0f;
		alpha *= 0.9f;
		v_texcoord1 = vec4(angle * crownRadius * 0.1f + i_data0.w * 0.013f, y * 0.12f - since * 0.15f, h1 * 0.3f, breakup);
		corner.x = 0.0f;
	}
	else
	{
		vec3 velocity;
		vec3 base;
		float life;
		float size;
		float opacity;
		float angle = h1 * 6.2831853f;
		vec3 out_ = vec3(cos(angle), 0.0f, sin(angle));
		if (layer < 1.5f)
		{
			// The column from the middle, after the crown has gone up
			float start = apex * 1.4f + 0.05f * h2;
			since -= start;
			base = centre + out_ * crownRadius * 0.15f * h3;
			velocity = upDir * rise * (0.55f + 0.45f * h2) + out_ * 0.4f * h4;
			life = 2.0f * rise * 0.8f / 9.8f + 0.3f;
			size = crownRadius * (0.08f + 0.08f * h3) + 0.1f * max(since, 0.0f);
			opacity = 0.6f;
		}
		else if (layer < 2.5f)
		{
			// The drops from the rim: only as many as the splash has
			base = centre + out_ * crownRadius * (0.8f + 0.4f * h3);
			since -= 0.15f * h2 * apex;
			velocity = out_ * dropSpeed * (0.25f + 0.45f * h4) + upDir * dropSpeed * (0.55f + 0.45f * h2);
			life = 3.0f;
			size = 0.05f + 0.07f * h3;
			opacity = index - 48.0f < drops ? 0.95f : 0.0f;
		}
		else
		{
			// The mist round the rim, which drifts on the wind
			vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z)) * (0.6f + 2.0f * u_uwWaves.y);
			base = centre + out_ * crownRadius * (0.9f + 0.5f * h3);
			since -= 0.1f * h2;
			velocity = out_ * (0.5f + dropSpeed * 0.15f * h4) + vec3(wind.x, 0.0f, wind.y) + upDir * rise * 0.3f * h2;
			life = 1.6f + 0.8f * h4;
			size = crownRadius * 0.2f + 0.5f * max(since, 0.0f);
			opacity = 0.06f * clamp(crownHeight, 0.0f, 1.0f);
		}
		world = base + velocity * since;
		if (layer < 2.5f)
		{
			world.y -= 4.9f * since * since;
		}
		age = since / life;
		alpha = since > 0.0f && since < life ? 1.0f - age : 0.0f;
		alpha *= smoothstep(0.0f, 0.05f, since);
		alpha *= world.y > level - 0.3f ? opacity : 0.0f;
		breakup = layer < 1.5f ? smoothstep(0.3f, 0.9f, age) : 0.0f;
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
			float spin = (h2 - 0.5f) * 1.5f * since + h4 * 6.2831853f;
			vec2 turned = vec2(corner.x * cos(spin) - corner.y * sin(spin), corner.x * sin(spin) + corner.y * cos(spin));
			placed = world + (right * turned.x + up * turned.y) * size;
		}
		normal = toEye;
		v_texcoord1 = vec4(vec3(h1 * 7.0f, h2 * 7.0f, h3 * 7.0f) + vec3(0.0f, -since * 0.25f, 0.0f), breakup);
	}

	vec3 ambient = u_uwSky.rgb * 1.5f + u_uwScatter.rgb * 0.25f + u_uwLightColour.rgb * 0.3f;
	v_texcoord0 = vec4(corner, layer, alpha);
	v_color0 = vec4(ambient, age);
	v_normal = normal;
	v_world = placed;
	gl_Position = alpha > 0.001f ? mul(u_viewProj, vec4(placed, 1.0f)) : hidden;
}
