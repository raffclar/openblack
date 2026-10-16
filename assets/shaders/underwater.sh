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
// x: how high the waves are (1 calm), y: how stormy the sea is (0 to 1), z: the wind's direction in radians from +x
// towards +z, w: how far the waves run up the beach, in height
uniform vec4 u_uwWaves;
// x: the storm's whitecaps and streaks of foam, 1 on, 0 off; y: the reef, 1 on, 0 off; zw: unused
uniform vec4 u_uwFlags2;
// The sea's currents, one field for every moving thing in the water (see underwater::CurrentAt). xy: the steady flow
// in units a second, z: how strong the swirls are, w: how strong the gusts are
uniform vec4 u_uwCurrent;

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

// One wave of the sea: its angle from the wind, its length, its height and its phase. Four long swells and two short
// chops, the same as underwater::WavesAt.
float UwWave(vec2 p, float t, float turn, float wavelength, float amplitude, float phase, float chop, inout vec2 slope)
{
	float storm = u_uwWaves.y;
	float angle = u_uwWaves.z + turn;
	vec2 along = vec2(cos(angle), sin(angle));
	float k = 6.2831853f / (wavelength * (1.0f - 0.35f * storm));
	float omega = sqrt(9.8f * k) * 0.8f;
	float sharp = 1.0f + 1.5f * storm;
	float a = amplitude * u_uwWaves.x * chop;
	float theta = k * dot(along, p) - omega * t + phase;
	float s = 0.5f + 0.5f * sin(theta);
	slope += along * (a * sharp * pow(max(s, 1e-4f), sharp - 1.0f) * cos(theta) * k);
	return a * (2.0f * pow(max(s, 0.0f), sharp) - 1.0f);
}

// The height of the waves over the sea's level at a point, and the slope of the surface there. The short chop fades
// far away, where it would be smaller than a pixel: `range` is how far the point is from the eye.
float UwWaves(vec2 p, float t, float range, out vec2 slope)
{
	slope = vec2_splat(0.0f);
	float chop = 1.0f + u_uwWaves.y * 2.5f;
	float close = 1.0f - smoothstep(60.0f, 250.0f, range);
	float height = UwWave(p, t, 0.0f, 61.0f, 0.30f, 0.0f, 1.0f, slope);
	height += UwWave(p, t, 0.5f, 41.0f, 0.18f, 1.7f, 1.0f, slope);
	height += UwWave(p, t, -0.4f, 27.0f, 0.12f, 4.1f, 1.0f, slope);
	height += UwWave(p, t, 1.1f, 18.0f, 0.08f, 2.3f, 1.0f, slope);
	vec2 chopSlope = vec2_splat(0.0f);
	height += UwWave(p, t, 0.3f, 7.0f, 0.04f, 5.2f, chop, chopSlope) * close;
	height += UwWave(p, t, -0.9f, 4.3f, 0.03f, 0.9f, chop, chopSlope) * close;
	slope += chopSlope * close;
	return height;
}

// How the highest of the waves is over the sea's level, for the crests' foam
float UwWaveTop()
{
	return (0.30f + 0.18f + 0.12f + 0.08f) * u_uwWaves.x;
}

// The normal of the sea's surface from its slope
vec3 UwSurfaceNormalOf(vec2 slope, float range)
{
	// The waves look flatter far away, where many of them fall in one pixel
	slope *= 1.0f - smoothstep(200.0f, 900.0f, range) * 0.8f;
	return normalize(vec3(-slope.x, 1.0f, -slope.y));
}

// How high the waves run up the beach now at a point, over the sea's level: in and out, a little out of step along the
// coast
float UwSwash(vec2 p, float t)
{
	float along = UwNoise(p * 0.02f) * 6.2831f;
	float cycle = 0.5f + 0.5f * sin(t * (0.9f + 0.4f * u_uwWaves.y) + along);
	return u_uwWaves.w * cycle * cycle;
}

// One swirl of the currents: the flow is the curl of a sum of these, so the water has no sinks
vec2 UwSwirl(vec2 p, float t, vec2 k, float omega, float amplitude, float phase, inout vec2 shift)
{
	float theta = dot(k, p) + omega * t + phase;
	vec2 across = vec2(k.y, -k.x) * (amplitude * u_uwCurrent.z);
	shift += across * (sin(theta) / omega);
	return across * cos(theta);
}

// How strong the gust is at a point now, 0 to 1: a front that passes along the wind every 300 units at 6 a second
float UwGust(vec2 p, float t)
{
	vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));
	float front = fract((dot(p, wind) - 6.0f * t) / 300.0f);
	return smoothstep(0.0f, 0.06f, front) * (1.0f - smoothstep(0.06f, 0.22f, front));
}

// The water's flow across the ground at a point, in units a second, and how far the swirls and the waves have moved
// the water there (`shift`), for things that drift with it. The same as underwater::CurrentAt.
vec2 UwCurrent(vec3 p, float t, out vec2 shift)
{
	shift = vec2_splat(0.0f);
	vec2 flow = u_uwCurrent.xy;
	flow += UwSwirl(p.xz, t, vec2(0.031f, 0.012f), 0.15f, 4.0f, 0.0f, shift);
	flow += UwSwirl(p.xz, t, vec2(-0.018f, 0.047f), 0.22f, 2.4f, 2.1f, shift);
	flow += UwSwirl(p.xz, t, vec2(0.062f, -0.051f), 0.31f, 1.2f, 4.4f, shift);
	vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));
	float gust = UwGust(p.xz, t) * u_uwCurrent.w;
	flow += wind * gust;
	shift += wind * gust * 1.5f;
	float k = 6.2831853f / (61.0f * (1.0f - 0.35f * u_uwWaves.y));
	float omega = sqrt(9.8f * k) * 0.8f;
	float theta = k * dot(wind, p.xz) - omega * t;
	float nearSurface = exp(min(p.y - u_uwParams.z, 0.0f) * k);
	float orbit = 0.30f * u_uwWaves.x * nearSurface;
	flow += wind * (orbit * omega * sin(theta));
	shift -= wind * (orbit * cos(theta));
	return flow;
}

#endif // UNDERWATER_SH
