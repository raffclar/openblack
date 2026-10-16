#ifndef UNDERWATER_SH
#define UNDERWATER_SH

// EXPERIMENT (not for merge): what the view under the sea shares between its shaders. The original game has no view
// under the sea.

// x: 1 while the view is under the sea, else 0; y: the time in seconds; z: the height of the sea's surface; w: how far
// under the surface the eye is
uniform vec4 u_uwParams;
// xyz: the direction towards the light under the surface, bent by the water; w: how much of the light is the moon's
uniform vec4 u_uwLight;
// rgb: the light's colour and strength; w: how much the light shows at all (0 at night with no moon)
uniform vec4 u_uwLightColour;
// rgb: the colour of the water far away at the eye's depth; w: unused
uniform vec4 u_uwScatter;
// xyz: where the eye is; w: 1 when the eye is under the surface
uniform vec4 u_uwEye;
// x: the reflections, y: the foam, z: the refraction, w: the shafts of light; 1 on, 0 off (to measure what each costs)
uniform vec4 u_uwFlags;
// xyz: the direction towards the light above the surface; w: the caustics, 1 on, 0 off
uniform vec4 u_uwAbove;

// How much of each colour the water takes out for each unit the light goes through it: the red first, then the green
#define UW_ABSORPTION vec3(0.085f, 0.032f, 0.018f)

// The part of each colour that is left after the light goes this far through the water
vec3 UwTransmittance(float range)
{
	return exp(-UW_ABSORPTION * max(range, 0.0f));
}

// A number from 0 to 1 for a point of a lattice
float UwHash(vec2 p)
{
	vec3 q = fract(vec3(p.x, p.y, p.x) * 0.1031f);
	q += dot(q, q.yzx + 33.33f);
	return fract((q.x + q.y) * q.z);
}

vec2 UwHash2(vec2 p)
{
	vec3 q = fract(vec3(p.x, p.y, p.x) * vec3(0.1031f, 0.1030f, 0.0973f));
	q += dot(q, q.yzx + 33.33f);
	return fract((q.xx + q.yz) * q.zy);
}

// Smooth noise from 0 to 1
float UwNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0f - 2.0f * f);
	float a = UwHash(i);
	float b = UwHash(i + vec2(1.0f, 0.0f));
	float c = UwHash(i + vec2(0.0f, 1.0f));
	float d = UwHash(i + vec2(1.0f, 1.0f));
	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// One layer of caustics: the bright lines where the waves of the surface focus the light. Points that move in time make
// cells, and the light is brightest along the edges between two cells.
float UwCausticLayer(vec2 p, float t)
{
	vec2 cell = floor(p);
	vec2 f = fract(p);
	float d1 = 8.0f;
	float d2 = 8.0f;
	for (int y = -1; y <= 1; y++)
	{
		for (int x = -1; x <= 1; x++)
		{
			vec2 o = vec2(float(x), float(y));
			vec2 h = UwHash2(cell + o);
			vec2 site = o + 0.5f + 0.42f * sin(t * vec2(0.83f, 1.21f) + 6.2831f * h) - f;
			float d = dot(site, site);
			if (d < d1)
			{
				d2 = d1;
				d1 = d;
			}
			else if (d < d2)
			{
				d2 = d;
			}
		}
	}
	float edge = sqrt(d2) - sqrt(d1);
	return pow(clamp(1.0f - edge * 1.5f, 0.0f, 1.0f), 4.0f);
}

// The caustics at a point of the surface: two layers that move different ways, so that the lines join and part.
// sharpness goes from 1 (sharp, near the surface) to 0 (soft, deep down).
float UwCaustics(vec2 p, float t, float sharpness)
{
	vec2 warp = vec2(sin(p.y * 1.7f + t * 0.9f), cos(p.x * 1.5f - t * 0.7f)) * 0.18f;
	float a = UwCausticLayer(p + warp, t);
	float b = UwCausticLayer(p * 1.31f + vec2(5.2f, 1.3f) - warp, t * 1.17f + 2.0f);
	float lines = a + b;
	return mix(0.45f + 0.25f * lines, lines * 1.1f, sharpness);
}

// How strongly the surface lets the light through at a point of it, for the shafts of light: bright and dark patches
// that drift as the waves move
float UwSurfaceLight(vec2 p, float t)
{
	float n = UwNoise(p * 0.11f + vec2(t * 0.05f, t * 0.03f)) * 0.65f;
	n += UwNoise(p * 0.29f - vec2(t * 0.08f, -t * 0.06f)) * 0.35f;
	return smoothstep(0.42f, 0.78f, n);
}

// The normal of the sea's surface at a point, from its waves: four long waves that go different ways and two short
// ones. They fade to a flat surface far away, where they would be smaller than a pixel.
vec3 UwWaveNormal(vec2 p, float t, float range)
{
	vec2 slope = vec2_splat(0.0f);
	vec2 d1 = vec2(0.80f, 0.60f);
	vec2 d2 = vec2(-0.39f, 0.92f);
	vec2 d3 = vec2(0.96f, -0.28f);
	vec2 d4 = vec2(-0.70f, -0.71f);
	slope += d1 * cos(dot(p, d1) * 0.31f + t * 0.90f) * 0.060f;
	slope += d2 * cos(dot(p, d2) * 0.53f + t * 1.25f) * 0.045f;
	slope += d3 * cos(dot(p, d3) * 0.97f + t * 1.70f) * 0.035f;
	slope += d4 * cos(dot(p, d4) * 1.71f + t * 2.30f) * 0.025f;
	float close = 1.0f - smoothstep(30.0f, 120.0f, range);
	vec2 d5 = vec2(0.17f, 0.98f);
	vec2 d6 = vec2(0.99f, 0.12f);
	slope += d5 * cos(dot(p, d5) * 3.70f + t * 3.4f) * 0.020f * close;
	slope += d6 * cos(dot(p, d6) * 5.30f - t * 3.9f) * 0.014f * close;
	slope *= 1.0f - smoothstep(150.0f, 600.0f, range);
	return normalize(vec3(-slope.x, 1.0f, -slope.y));
}

#endif // UNDERWATER_SH
