$input v_texcoord0

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): the moon that looks like the real moon (graphics::real_moon). The square holds a ball
// with the map of the moon's surface, lit by the sun from the true direction, and a soft halo round it. The colour
// is premultiplied: the alpha is how much the moon hides the sky behind it.

// The map of the moon's surface: the middle column is the point that faces the eye, the top row is the north
SAMPLER2D(s_moonMap, 0);
// xyz: the moon's east across its face (on the west side of the sky); w: unused
uniform vec4 u_moonEast;
// xyz: the moon's north across its face; w: the size of the square in numbers of the moon's radius
uniform vec4 u_moonNorth;
// xyz: from the moon to the eye; w: the light of the earth on the dark side
uniform vec4 u_moonToEye;
// xyz: the direction towards the sun; w: the brightness of the map's light
uniform vec4 u_moonSun;
// rgb: the part of each colour that comes through the air and the clouds; w: how much the moon hides the sky
uniform vec4 u_moonTint;
// rgb: the colour of the halo; w: its strength
uniform vec4 u_moonHalo;

#define REAL_MOON_PI 3.14159265f

// The map's colour at a point of the moon's face, with four samples round it, so that the small moon does not
// shimmer
vec3 MapAt(vec2 p, vec2 spread)
{
	vec3 sum = vec3_splat(0.0f);
	for (int i = 0; i < 4; ++i)
	{
		vec2 offset = vec2(i == 1 || i == 3 ? 0.25f : -0.25f, i >= 2 ? 0.25f : -0.25f) * spread;
		vec2 q = p + offset;
		float r2 = min(dot(q, q), 0.999f);
		float z = sqrt(1.0f - r2);
		float longitude = atan2(q.x, z);
		float latitude = asin(clamp(q.y, -1.0f, 1.0f));
		sum += texture2DLod(s_moonMap, vec2(0.5f + longitude / (2.0f * REAL_MOON_PI), 0.5f - latitude / REAL_MOON_PI), 0.0f).rgb;
	}
	return sum * 0.25f;
}

void main()
{
	vec2 p = v_texcoord0.xy;
	float r = length(p);
	float pixel = max(fwidth(r), 1e-4f);
	// The moon's edge, smooth over one pixel
	float disc = 1.0f - smoothstep(1.0f - pixel, 1.0f, r);

	vec3 colour = vec3_splat(0.0f);
	if (r < 1.0f)
	{
		float z = sqrt(max(1.0f - r * r, 0.0f));
		vec3 normal = u_moonEast.xyz * p.x + u_moonNorth.xyz * p.y + u_moonToEye.xyz * z;
		vec3 albedo = MapAt(p, vec2(fwidth(p.x), fwidth(p.y)));
		// The sun's light: the moon's dust sends it back almost the same at all angles (Lommel and Seeliger)
		float towardsSun = dot(normal, u_moonSun.xyz);
		float sunLight = towardsSun > 0.0f ? 2.0f * towardsSun / (towardsSun + max(z, 1e-3f)) : 0.0f;
		// The earth's light comes from behind the eye, onto the whole face
		float earthLight = u_moonToEye.w * z;
		colour = albedo * (sunLight + earthLight) * u_moonSun.w * u_moonTint.rgb;
	}
	// The halo: a bright ring close to the moon and a wide, faint glow, both gone at the edge of the square
	float out1 = max(r - 1.0f, 0.0f);
	float fade = 1.0f - smoothstep(0.6f * u_moonNorth.w, u_moonNorth.w, r);
	float halo = u_moonHalo.w * (0.6f * exp(-out1 * 1.6f) + 0.4f * exp(-out1 * 0.35f)) * fade;
	gl_FragColor = vec4(colour * disc + u_moonHalo.rgb * halo * (1.0f - disc), disc * u_moonTint.w);
}
