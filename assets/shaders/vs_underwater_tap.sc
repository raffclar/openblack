$input a_position, i_data0, i_data1, i_data2
$output v_texcoord0, v_texcoord1, v_color0, v_normal, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the splash where the hand taps the water. Each instance is one tap (i_data0: xyz where it
// is on the water's surface, w when it started; i_data1: x how high its sheet goes, y the sheet's radius, z how fast its
// drops leave the water, w how many drops it has; i_data2: xyz its own numbers for the drops, w how dense its spray is).
// Its particles, lit and shaped as the spray at the cliffs (fs_underwater_spray.sc):
// - 0 to 31: a sheet of water that rises round the hand, leans out, breaks into strands and falls back,
// - 32 to 111: drops that fly up and out and fall back under gravity, stretched along their motion. They are the same
//   drops as underwater::tap::DropOf, so the rings start where they land.
// - 112 to 143: a fine spray round the rim that drifts on the wind,
// - 144 to 191: bits of foam on the water that spread out with the first ring and fade.

// rgb: the colour of the sky
uniform vec4 u_uwSky;
// x: the gravity in units a second a second; y: game units in a metre
uniform vec4 u_tapParams;

float TapHash(float n)
{
	return fract(sin(n * 12.9898f) * 43758.5453f);
}

void main()
{
	vec3 centre = i_data0.xyz;
	float since = u_uwParams.y - i_data0.w;
	float sheetHeight = i_data1.x;
	float sheetRadius = i_data1.y;
	float dropSpeed = i_data1.z;
	float drops = i_data1.w;
	vec3 seed = i_data2.xyz;
	float spray = i_data2.w;
	float g = u_tapParams.x;
	float metre = u_tapParams.y;
	vec2 corner = a_position.xy;
	float index = a_position.z;
	float level = centre.y;
	vec4 hidden = vec4(0.0f, 0.0f, 2.0f, 1.0f);

	vec3 eye = u_uwEye.xyz;
	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 upDir = vec3(0.0f, 1.0f, 0.0f);
	float rise = sqrt(2.0f * g * max(sheetHeight, 0.01f));
	float apex = rise / g;
	float h0 = TapHash(index * 3.17f + seed.x * 91.0f);

	vec3 placed;
	vec3 normal;
	float alpha;
	float age;
	float breakup;
	float layer;
	if (index < 32.0f)
	{
		// The sheet: panels edge to edge round the hand, each with a ragged top
		layer = 0.0f;
		float angle = (index + 0.5f + corner.x * 0.5f) / 32.0f * 6.2831853f;
		vec3 out_ = vec3(cos(angle), 0.0f, sin(angle));
		float life = 2.0f * apex + 0.35f;
		age = since / life;
		breakup = smoothstep(0.45f * apex, 1.4f * apex, since);
		float top = since < apex ? max(rise * since - 0.5f * g * since * since, 0.0f) : sheetHeight;
		float bottom = since < apex ? 0.0f : 0.5f * g * (since - apex) * (since - apex);
		float v = corner.y * 0.5f + 0.5f;
		float y = mix(min(bottom, top), top, v) * (0.75f + 0.5f * TapHash(floor(index) + seed.y * 37.0f));
		// The sheet opens out as it rises, and its top leans out more as it breaks
		float radius = sheetRadius * (0.45f + 0.75f * (1.0f - exp(-since * 6.0f))) + v * v * sheetHeight * (0.1f + 0.3f * breakup);
		vec3 world = centre + out_ * radius;
		placed = world + upDir * y;
		normal = normalize(out_ + upDir * (0.35f - v * 0.7f));
		alpha = since > 0.0f && since < life && top - bottom > 0.02f ? (1.0f - smoothstep(0.55f, 1.0f, age)) * 0.6f : 0.0f;
		v_texcoord1 = vec4(angle * sheetRadius * 0.15f + seed.z * 7.0f, y * 0.2f / metre - since * 0.2f, h0 * 0.3f, breakup);
		corner.x = 0.0f;
	}
	else if (index < 112.0f)
	{
		// A drop, as underwater::tap::DropOf makes it: four sequences that spread evenly give its direction and speed
		layer = 2.0f;
		float i = index - 32.0f;
		float h1 = fract(i * 0.6180340f + seed.x);
		float h2 = fract(i * 0.7548777f + seed.y);
		float h3 = fract(i * 0.5698403f + seed.z);
		float h4 = fract(i * 0.4142136f + seed.x + seed.y);
		float angle = h1 * 6.2831853f;
		vec3 out_ = vec3(cos(angle), 0.0f, sin(angle));
		vec3 start = centre + out_ * sheetRadius * (0.7f + 0.5f * h3);
		vec3 velocity = out_ * dropSpeed * (0.15f + 0.35f * h4) + upDir * dropSpeed * (0.5f + 0.5f * h2);
		float flight = since - 0.12f * h2 * apex;
		float life = 2.0f * velocity.y / g;
		float size = (0.025f + 0.045f * h3) * metre;
		vec3 world = start + velocity * flight - upDir * 0.5f * g * flight * flight;
		age = flight / life;
		alpha = flight > 0.0f && flight < life && i < drops ? 0.95f : 0.0f;
		breakup = 0.0f;
		// A fast drop is a streak along its motion
		vec3 toEye = normalize(eye - world);
		vec3 moving = velocity - upDir * g * flight;
		vec3 alongMotion = normalize(moving - toEye * dot(moving, toEye) + vec3(1e-4f, 0.0f, 0.0f));
		vec3 acrossMotion = normalize(cross(alongMotion, toEye));
		float stretch = 1.0f + 0.08f * length(moving) / metre;
		placed = world + alongMotion * corner.y * size * stretch * 2.2f + acrossMotion * corner.x * size;
		normal = toEye;
		v_texcoord1 = vec4(h1 * 7.0f, h2 * 7.0f - since * 0.25f, h3 * 7.0f, 0.0f);
	}
	else if (index < 144.0f)
	{
		// The fine spray round the rim, which drifts on the wind and grows
		layer = 3.0f;
		float h1 = TapHash(index * 1.31f + seed.y * 53.0f);
		float h2 = TapHash(index * 2.71f + seed.z * 29.0f);
		float angle = h1 * 6.2831853f;
		vec3 out_ = vec3(cos(angle), 0.0f, sin(angle));
		vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z)) * (0.5f + 2.0f * u_uwWaves.y) * metre;
		float start = 0.05f + 0.15f * h2;
		float drift = since - start;
		vec3 world = centre + out_ * (sheetRadius * (0.9f + 0.4f * h2) + 0.6f * metre * drift) +
		             vec3(wind.x, 0.0f, wind.y) * drift + upDir * (rise * 0.35f * h1 * drift / (1.0f + drift));
		float life = 1.4f + 0.9f * h2;
		float size = sheetRadius * 0.25f + 0.45f * metre * max(drift, 0.0f);
		age = drift / life;
		alpha = drift > 0.0f && drift < life ? (1.0f - age) * smoothstep(0.0f, 0.1f, drift) * 0.08f * spray : 0.0f;
		breakup = 0.0f;
		float spin = (h2 - 0.5f) * 1.2f * drift + h1 * 6.2831853f;
		vec2 turned = vec2(corner.x * cos(spin) - corner.y * sin(spin), corner.x * sin(spin) + corner.y * cos(spin));
		placed = world + (right * turned.x + up * turned.y) * size;
		normal = normalize(eye - world);
		v_texcoord1 = vec4(h1 * 7.0f, h2 * 7.0f - since * 0.2f, h0 * 7.0f, 0.0f);
	}
	else
	{
		// A bit of foam that lies on the water: it moves out with the first ring, slows, and fades
		layer = 1.0f;
		float h1 = TapHash(index * 4.13f + seed.x * 17.0f);
		float h2 = TapHash(index * 5.77f + seed.y * 23.0f);
		float angle = h1 * 6.2831853f;
		vec3 out_ = vec3(cos(angle), 0.0f, sin(angle));
		float start = apex * 0.6f;
		float drift = since - start;
		float ring = sheetRadius * (0.6f + 0.6f * h2) + 1.2f * metre * (1.0f - exp(-max(drift, 0.0f) * 1.5f));
		vec3 world = centre + out_ * ring + upDir * 0.06f;
		float life = 2.5f + 1.5f * h2;
		float size = (0.08f + 0.1f * h2) * metre * (1.0f + 0.4f * max(drift, 0.0f));
		age = drift / life;
		alpha = drift > 0.0f && drift < life ? (1.0f - age) * smoothstep(0.0f, 0.2f, drift) * 0.8f : 0.0f;
		breakup = smoothstep(0.3f, 1.0f, age);
		// It lies flat, turned its own way
		float spin = h2 * 6.2831853f + drift * (h1 - 0.5f) * 0.5f;
		vec3 xAxis = vec3(cos(spin), 0.0f, sin(spin));
		vec3 zAxis = vec3(-xAxis.z, 0.0f, xAxis.x);
		placed = world + (xAxis * corner.x + zAxis * corner.y * 0.7f) * size;
		normal = upDir;
		v_texcoord1 = vec4(h1 * 7.0f, h2 * 7.0f, h0 * 7.0f, breakup);
	}

	vec3 ambient = u_uwSky.rgb * 1.5f + u_uwScatter.rgb * 0.25f + u_uwLightColour.rgb * 0.3f;
	v_texcoord0 = vec4(corner, layer, alpha);
	// w: the height of the tap's water, which the particles fade near
	v_color0 = vec4(ambient, level);
	v_normal = normal;
	v_world = placed;
	gl_Position = alpha > 0.001f ? mul(u_viewProj, vec4(placed, 1.0f)) : hidden;
}
