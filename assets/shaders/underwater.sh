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
// x: the storm's whitecaps and streaks of foam, 1 on, 0 off; y: the reef, 1 on, 0 off; z: 1 for the motes' pass of
// debris; w: 1 to draw the water as a flat colour where it shows, to compare it with the original sea
uniform vec4 u_uwFlags2;
// x: the wet rock of the cliffs, y: the spray's mist and haze; 1 on, 0 off; zw: unused
uniform vec4 u_uwFlags3;
// The sea's currents, one field for every moving thing in the water (see underwater::CurrentAt). xy: the steady flow
// in units a second, z: how strong the swirls are, w: how strong the gusts are
uniform vec4 u_uwCurrent;
// x: the shore wave's phase in radians, which grows each frame (see underwater::AdvanceShorePhase); y: the height of
// the open sea's waves that come in to the coast, in units; zw: unused
uniform vec4 u_uwPhase;

// The world's scale: units for each metre, from the height of an adult male villager (underwater::k_UnitsPerMetre)
#define UW_UNITS_PER_METRE 0.949143f
// The acceleration of gravity, in units a second squared (underwater::k_Gravity)
#define UW_GRAVITY (9.81f * UW_UNITS_PER_METRE)

// How much of each colour the water takes out for each unit the light goes through it: the red first, then the green
// The lowest low tide under the usual height (underwater::k_LowestTide)
#define UW_LOWEST_TIDE 2.5f

#define UW_ABSORPTION vec3(0.085f, 0.032f, 0.018f)
// The least distance of water that the light goes through where the original game shows its sea, in units
#define UW_SHALLOWS_TINT 12.0f
// The alpha that the land writes into the scene when the experiment's water is on. Other draws do not usually write it.
#define UW_LAND_MARK (128.0f / 255.0f)
// The highest land, in units, that can show the original sea through its coast alpha (altitude 4)
#define UW_FILM_TOP 2.7f

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

// Noise for foam: three layers turned against each other, so no square cells of the noise show
float UwFoamNoise(vec2 p)
{
	vec2 a = vec2(p.x * 0.8f - p.y * 0.6f, p.x * 0.6f + p.y * 0.8f);
	vec2 b = vec2(p.x * 0.28f + p.y * 0.96f, p.y * 0.28f - p.x * 0.96f) * 2.1f + 5.3f;
	vec2 c = vec2(p.x * -0.5f + p.y * 0.87f, p.x * 0.87f + p.y * 0.5f) * 4.3f + 1.7f;
	return UwNoise(a) * 0.55f + UwNoise(b) * 0.3f + UwNoise(c) * 0.15f;
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

// The size of a cell of the caustics on the sea bed: 1 m, so their lines are some tens of centimetres apart
#define UW_CAUSTIC_FREQUENCY (1.0f / (1.0f * UW_UNITS_PER_METRE))

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

// The waves of the sea, the same table as underwater::WaveTable: x the direction, y the length, z the height, w the
// phase. The first two are the swell, the rest the wind sea.
uniform vec4 u_uwWaveTable[14];

// How high the waves are here and now against their mean: x for the wind sea, y for the swell (underwater::WaveSets)
vec2 UwWaveSets(vec2 p, float t)
{
	vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));
	float kWind = 6.2831853f / 440.0f;
	float speedWind = 0.5f * sqrt(9.8f * 55.0f / 6.2831853f) * 0.8f;
	float sets = 0.5f + 0.5f * sin(kWind * dot(wind, p) - kWind * speedWind * t);
	float patches = 0.75f + 0.25f * sin(p.x * 0.004f + p.y * 0.0017f + t * 0.05f) * sin(p.y * 0.0031f - p.x * 0.0012f - t * 0.04f);
	vec2 swell = vec2(cos(2.4f), sin(2.4f));
	float kSwell = 6.2831853f / 840.0f;
	float speedSwell = 0.5f * sqrt(9.8f * 120.0f / 6.2831853f) * 0.8f;
	float swellSets = 0.5f + 0.5f * sin(kSwell * dot(swell, p) - kSwell * speedSwell * t + 1.3f);
	return vec2((0.45f + 1.1f * sets * sets) * patches, 0.5f + 1.0f * swellSets);
}

// How sharp the crests are on a calm day (underwater::k_CrestSharpness)
#define UW_CREST_SHARPNESS 1.25f
// How far the warp moves the wind sea's pattern at most, in metres (underwater::k_WarpMetres)
#define UW_WARP_METRES 6.0f
// The range of the wind sea's groups (underwater::k_GroupRange)
#define UW_GROUP_RANGE 1.2f
// The steepest that one wave of the table can be, as its height times its wave number, on a calm day and in the worst
// storm (underwater::k_SteepestWaveCalm and k_SteepestWaveStorm)
#define UW_STEEPEST_WAVE_CALM 0.12f
#define UW_STEEPEST_WAVE_STORM 0.16f
// How long the foam of a white horse stays after its crest, in seconds (underwater::k_WhitecapSeconds)
#define UW_WHITECAP_SECONDS 1.5f

// How much of the water at a point is a white horse, 0 to 1, from the crest envelope of UwWavesInDepth: the same as
// underwater::WhitecapShare. A stronger wind breaks lower crests.
float UwWhitecap(float envelope)
{
	float start = 0.9f - 0.08f * u_uwWaves.y;
	return smoothstep(start, start + 0.12f, envelope);
}

// The share of the open sea that white horses cover, for a pixel that is too large to show them one by one: the same
// as underwater::WhitecapCover
float UwWhitecapCover()
{
	return 0.005f + 0.2f * pow(max(u_uwWaves.y, 0.0f), 1.6f);
}

// How the wind sea varies over the sea, the same as underwater::WaveVariationAt. warp: how far the wind sea's pattern
// moves; warpSlope: the change of the warp along x (xy) and z (zw); groups: how big the even (x) and odd (y) waves are
// against their mean; groupSlope: the change of the groups along x (xy) and z (zw).
void UwWaveVariation(vec2 p, float t, out vec2 warp, out vec4 warpSlope, out vec2 groups, out vec4 groupSlope)
{
	float amplitude = UW_WARP_METRES * UW_UNITS_PER_METRE;
	float f1 = 6.2831853f / (110.0f * UW_UNITS_PER_METRE);
	float f2 = 6.2831853f / (130.0f * UW_UNITS_PER_METRE);
	float f3 = 6.2831853f / (70.0f * UW_UNITS_PER_METRE);
	float f4 = 6.2831853f / (85.0f * UW_UNITS_PER_METRE);
	vec2 d1 = vec2(0.8f, 0.6f);
	vec2 d2 = vec2(-0.6f, 0.8f);
	vec2 d3 = vec2(0.28f, -0.96f);
	vec2 d4 = vec2(0.96f, 0.28f);
	float a1 = f1 * dot(d1, p) + 0.11f * t;
	float a2 = f2 * dot(d2, p) - 0.09f * t;
	float a3 = f3 * dot(d3, p) - 0.07f * t + 1.3f;
	float a4 = f4 * dot(d4, p) + 0.05f * t + 2.9f;
	warp = amplitude * vec2(sin(a1) + 0.5f * sin(a3), sin(a2) + 0.5f * sin(a4));
	vec2 dx = amplitude * (vec2(f1 * d1.x * cos(a1), f2 * d2.x * cos(a2)) + 0.5f * vec2(f3 * d3.x * cos(a3), f4 * d4.x * cos(a4)));
	vec2 dz = amplitude * (vec2(f1 * d1.y * cos(a1), f2 * d2.y * cos(a2)) + 0.5f * vec2(f3 * d3.y * cos(a3), f4 * d4.y * cos(a4)));
	warpSlope = vec4(dx, dz);
	float g1 = 6.2831853f / (90.0f * UW_UNITS_PER_METRE);
	float g2 = 6.2831853f / (60.0f * UW_UNITS_PER_METRE);
	vec2 e1 = vec2(0.92f, 0.39f);
	vec2 e2 = vec2(-0.31f, 0.95f);
	float b1 = g1 * dot(e1, p) + 0.04f * t;
	float b2 = g2 * dot(e2, p) - 0.03f * t;
	float b3 = g2 * dot(e1, p) - 0.05f * t + 2.1f;
	float b4 = g1 * dot(e2, p) + 0.035f * t + 0.7f;
	float range = 0.5f * UW_GROUP_RANGE;
	groups = vec2(1.0f + range * sin(b1) * sin(b2), 1.0f + range * sin(b3) * sin(b4));
	vec2 even = range * (cos(b1) * sin(b2) * g1 * e1 + sin(b1) * cos(b2) * g2 * e2);
	vec2 odd = range * (cos(b3) * sin(b4) * g2 * e1 + sin(b3) * cos(b4) * g1 * e2);
	groupSlope = vec4(even.x, odd.x, even.y, odd.y);
}

// The tilt of the wind sea's lengths, the same as underwater::WaveTiltAt: x the tilt (-1 to 1), yz its change along x
// and z. Where it is over 0 the long waves of the wind sea are higher and the short ones lower.
#define UW_TILT_STRENGTH 0.7f
#define UW_TILT_METRES 10.0f
vec3 UwWaveTilt(vec2 p, float t)
{
	float h1 = 6.2831853f / (260.0f * UW_UNITS_PER_METRE);
	float h2 = 6.2831853f / (190.0f * UW_UNITS_PER_METRE);
	vec2 q1 = vec2(0.6f, -0.8f);
	vec2 q2 = vec2(0.95f, 0.31f);
	float c1 = h1 * dot(q1, p) + 0.012f * t + 0.4f;
	float c2 = h2 * dot(q2, p) - 0.009f * t + 1.9f;
	return vec3(sin(c1) * sin(c2), cos(c1) * sin(c2) * h1 * q1 + sin(c1) * cos(c2) * h2 * q2);
}

// The far sea. x: 1 while the waves are filtered for each pixel, y: 1 while the long waves show, z: the angle that one
// pixel takes up on the screen (radians), w: 1 while the water reflects the cube of the far scene
uniform vec4 u_uwSea;
// The long waves of the open sea, the same table as underwater::FarWaveTable, as u_uwWaveTable keeps them
uniform vec4 u_uwFarWaves[3];

// The size of a pixel on the water, for the waves that it can show. x: the size across the view; yz: the direction of
// the view along the ground; w: how many times longer the pixel is along the view than across it. This gives a round
// pixel of the old size for a point at a distance from the eye.
vec4 UwFootprint(float range)
{
	return vec4(0.0015f * max(range, 1.0f), 1.0f, 0.0f, 1.0f);
}

// The size of a pixel on the water along a direction
float UwFootprintAlong(vec4 footprint, vec2 along)
{
	float lengthwise = dot(along, footprint.yz) * footprint.w;
	float across = along.x * footprint.z - along.y * footprint.y;
	return footprint.x * sqrt(lengthwise * lengthwise + across * across);
}

// The share of a wave that a pixel shows, the same as underwater::FilterWave. x: the share of its height, y: the share
// of its slope. A wave shorter than about four pixels loses its height, and one shorter than about three pixels loses
// its slope too. Without the filter both fade as the height does.
vec2 UwWaveShown(float length, float pixel)
{
	float height = smoothstep(pixel * 4.0f, pixel * 8.0f, length);
	float slope = u_uwSea.x > 0.5f ? smoothstep(pixel * 2.0f, pixel * 4.0f, length) : height;
	return vec2(height, slope);
}

// The height of the waves over the sea's level at a point, and the slope of the surface there, as a pixel of the
// size `footprint` (see UwFootprint) shows them. `variance` is the variance of the slope that the pixel cannot show,
// which makes the surface rougher.
// How much a wave of wave number k (deep water) grows as it comes into water of a depth: the same as
// underwater::Shoaling
float UwShoaling(float k, float depth)
{
	if (depth <= 0.0f)
	{
		return 0.0f;
	}
	float deep = k * depth;
	if (deep > 6.0f)
	{
		return 1.0f;
	}
	// GLSL 1.20 has no hyperbolic functions: they come from exp here
	float e1 = exp(-2.0f * deep);
	float tanhDeep = (1.0f - e1) / (1.0f + e1);
	float kh = deep / sqrt(max(tanhDeep, 1e-6f));
	float e2 = exp(-2.0f * kh);
	float tanhKh = (1.0f - e2) / (1.0f + e2);
	float sinh2Kh = 0.5f * (exp(2.0f * kh) - exp(-2.0f * kh));
	float group = tanhKh * (1.0f + 2.0f * kh / max(sinh2Kh, 1e-6f));
	return 1.0f / sqrt(max(group, 1e-4f));
}

// The chop of the wind: short waves with pointed crests over the long waves of the sea. Each octave is a grid of
// crests that noise bends, with a round trough and a pointed crest. Two copies of each octave move in opposite
// directions. The next octave turns, is 3.8 times shorter, 0.22 times as high and less pointed. The same as
// underwater::ChopAt: change them together.
// The height of the first octave on a calm day, and its length, in metres (underwater::k_ChopMetres and
// k_ChopLengthMetres)
#define UW_CHOP_METRES 0.25f
#define UW_CHOP_LENGTH_METRES 30.0f
// The octaves for the height of the surface, and the octaves for its normal
#define UW_CHOP_GEOMETRY 3
#define UW_CHOP_SHADING 5

// One octave of the chop at a point of its grid, 0 in the trough to 1 on the crest. `pointed` makes the crests
// narrower and the troughs broader.
float UwChopOctave(vec2 q, float pointed)
{
	q += vec2_splat(UwNoise(q) * 2.0f - 1.0f);
	vec2 edge = 1.0f - abs(sin(q));
	vec2 trough = mix(edge, abs(cos(q)), edge);
	return pow(max(1.0f - pow(max(trough.x * trough.y, 0.0f), 0.6f), 0.0f), pointed);
}

// The height of the chop over its mean at a point. `octaves` is how many octaves to add, and `pixel` is the size of a
// pixel on the water there: an octave that the pixel cannot show fades out.
float UwChop(vec2 p, float t, int octaves, float pixel)
{
	float amplitude = UW_CHOP_METRES * UW_UNITS_PER_METRE * (1.0f + 2.0f * u_uwWaves.y) * min(u_uwWaves.x, 1.0f);
	if (amplitude <= 0.0f)
	{
		return 0.0f;
	}
	vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));
	float k = 6.2831853f / (UW_CHOP_LENGTH_METRES * UW_UNITS_PER_METRE);
	vec2 q = vec2(dot(p, wind), wind.x * p.y - wind.y * p.x) * k;
	float pointed = 4.0f;
	float height = 0.0f;
	for (int i = 0; i < UW_CHOP_SHADING; i++)
	{
		if (i >= octaves)
		{
			break;
		}
		float shown = UwWaveShown(6.2831853f / k, pixel).x;
		float drift = sqrt(UW_GRAVITY * k) * t;
		float pair = UwChopOctave(q + vec2(drift, 0.0f), pointed) + UwChopOctave(q - vec2(drift, 0.0f), pointed);
		height += (pair - 2.0f * (0.42f + 0.028f * float(i))) * amplitude * shown;
		q = vec2(1.6f * q.x + 1.2f * q.y, 1.6f * q.y - 1.2f * q.x) * 1.9f;
		k *= 3.8f;
		amplitude *= 0.22f;
		pointed = mix(pointed, 1.0f, 0.2f);
	}
	return height;
}

// How big the waves are at a point against the usual, from the distance to the coast: the same as
// underwater::OpenSeaSize
float UwOpenSeaSize(vec2 p, float t, float shoreDistance)
{
	float far = smoothstep(60.0f * UW_UNITS_PER_METRE, 500.0f * UW_UNITS_PER_METRE, shoreDistance);
	float patches = 0.5f + 0.5f * sin(p.x * 0.0021f + p.y * 0.0013f + t * 0.01f) * sin(p.y * 0.0017f - p.x * 0.0009f - t * 0.008f);
	return mix(0.85f, 0.7f + 0.9f * patches, far) * (1.0f + 0.35f * far);
}

// The height of the waves over the sea's level at a point in water of a depth, at a distance from the coast, and the
// slope there, as a pixel of the size `footprint` shows them. Each wave grows as the water gets shallow, and all of
// them break together where they are too high for the depth (the same as underwater::WavesInDepth). `breaking` is
// how much of their height the breaking takes away, 0 to 1. `whitecap` is the crest envelope for the white horses (see
// underwater::WaveSample::whitecap). The chop of the wind (UwChop) adds its octaves for the height, but not its slope:
// `chopShare` is the share of the chop that the depth keeps, for the normal of the surface.
float UwWavesInDepth(vec2 p, float t, vec4 footprint, float shoreDistance, float depth, out vec2 slope, out float variance,
                     out float breaking, out float whitecap, out float chopShare)
{
	slope = vec2_splat(0.0f);
	variance = 0.0f;
	whitecap = 0.0f;
	vec2 sets = UwWaveSets(p, t);
	float open = smoothstep(60.0f * UW_UNITS_PER_METRE, 500.0f * UW_UNITS_PER_METRE, shoreDistance);
	sets = pow(max(sets, vec2_splat(1e-3f)), vec2_splat(1.0f + 0.8f * open));
	float size = UwOpenSeaSize(p, t, shoreDistance);
	// Each wave's growth in this depth, found once
	float grown[14];
	float grownFar[3];
	float total = 0.0f;
	for (int n = 0; n < 14; n++)
	{
		vec4 wave = u_uwWaveTable[n];
		grown[n] = UwShoaling(6.2831853f / max(wave.y, 1.0f), depth);
		total += wave.z * (n < 2 ? sets.y : sets.x) * grown[n];
	}
	for (int m = 0; m < 3; m++)
	{
		vec4 wave = u_uwFarWaves[m];
		grownFar[m] = UwShoaling(6.2831853f / max(wave.y, 1.0f), depth);
		total += wave.z * sets.y * grownFar[m];
	}
	total *= size;
	float kept = total > 0.0f ? min(1.0f, 0.39f * max(depth, 0.0f) / total) : 0.0f;
	breaking = 1.0f - kept;
	float sharp = UW_CREST_SHARPNESS + 1.5f * u_uwWaves.y;
	float height = 0.0f;
	// The wind sea is drawn at a warped point, with its waves in groups (see UwWaveVariation)
	vec2 warp;
	vec4 warpSlope;
	vec2 groups;
	vec4 groupSlope;
	UwWaveVariation(p, t, warp, warpSlope, groups, groupSlope);
	vec3 tilt = UwWaveTilt(p, t);
	vec2 warped = p + warp;
	vec2 windSlope = vec2_splat(0.0f);
	for (int i = 0; i < 14; i++)
	{
		vec4 wave = u_uwWaveTable[i];
		bool wind = i >= 2;
		bool even = mod(float(i), 2.0f) < 0.5f;
		vec2 along = vec2(cos(wave.x), sin(wave.x));
		float k = 6.2831853f / wave.y;
		float omega = sqrt(UW_GRAVITY * k);
		float pixel = u_uwSea.x > 0.5f ? UwFootprintAlong(footprint, along) : footprint.x;
		// A sharp crest holds shorter waves than the wave's own length: the filter takes the length of the shortest
		vec2 shown = UwWaveShown(u_uwSea.x > 0.5f ? wave.y / sharp : wave.y, pixel);
		float plain = wave.z * (i < 2 ? sets.y : sets.x) * size * grown[i] * kept;
		float tiltLog = UW_TILT_STRENGTH * log(max(wave.y, 1e-3f) / (UW_TILT_METRES * UW_UNITS_PER_METRE));
		float tiltScale = wind ? exp(tilt.x * tiltLog) : 1.0f;
		float plainGroup = even ? groups.x : groups.y;
		float group = wind ? plainGroup * tiltScale : 1.0f;
		// The sets, groups, patches and tilt all make a wave higher. A soft cap keeps each wave under the steepness
		// that the sea holds, so a wave never gets a sharp peak (see underwater::CapSteepness).
		float limit = mix(UW_STEEPEST_WAVE_CALM, UW_STEEPEST_WAVE_STORM, u_uwWaves.y) / k;
		float overLimit = plain * group / limit;
		float capRate = 1.0f / (1.0f + overLimit * overLimit);
		float a = plain * group * sqrt(capRate);
		capRate *= sqrt(capRate);
		float theta = k * dot(along, wind ? warped : p) - omega * t + wave.w;
		float s = 0.5f + 0.5f * sin(theta);
		float shape = 2.0f * pow(max(s, 0.0f), sharp) - 1.0f;
		height += a * shown.x * shape;
		vec2 rise = along * (a * shown.y * sharp * pow(max(s, 1e-4f), sharp - 1.0f) * cos(theta) * k);
		if (wind)
		{
			// The crest envelope: the steepness of the wave on its crest, and for a short time after its crest. Where the
			// pixel cannot show the wave, the mean of the envelope over a wave takes its place, so far away it does not
			// flicker.
			float age = mod(1.5707963f - theta, 6.2831853f) / omega;
			float onCrest = max(pow(max(s, 0.0f), sharp), exp(-age / UW_WHITECAP_SECONDS));
			whitecap += a * k * mix(0.5f, onCrest, shown.y);
			windSlope += rise;
			slope += plain * capRate * shown.x * shape *
			         ((even ? groupSlope.xz : groupSlope.yw) * tiltScale + plainGroup * tiltScale * tiltLog * tilt.yz);
		}
		else
		{
			slope += rise;
		}
		variance += (1.0f - shown.y * shown.y) * 0.5f * (a * k) * (a * k);
	}
	// The slope at the warped point, turned back to the slope at the point itself
	slope += vec2(windSlope.x * (1.0f + warpSlope.x) + windSlope.y * warpSlope.y,
	              windSlope.x * warpSlope.z + windSlope.y * (1.0f + warpSlope.w));
	// The long waves of the open sea: plain sine waves that rise and fall with the swell's sets
	for (int j = 0; j < 3; j++)
	{
		vec4 wave = u_uwFarWaves[j];
		vec2 along = vec2(cos(wave.x), sin(wave.x));
		float k = 6.2831853f / max(wave.y, 1.0f);
		float omega = sqrt(UW_GRAVITY * k);
		float pixel = u_uwSea.x > 0.5f ? UwFootprintAlong(footprint, along) : footprint.x;
		vec2 shown = UwWaveShown(wave.y, pixel);
		float a = wave.z * sets.y * size * grownFar[j] * kept;
		float theta = k * dot(along, p) - omega * t + wave.w;
		height += a * shown.x * sin(theta);
		slope += along * (a * shown.y * k * cos(theta));
		variance += (1.0f - shown.y * shown.y) * 0.5f * (a * k) * (a * k);
	}
	// The chop of the wind, cut by the depth as the other waves are
	chopShare = kept;
	height += UwChop(p, t, UW_CHOP_GEOMETRY, footprint.x) * kept;
	return height;
}

// The caustics on the bed under a point of the surface, from the waves themselves: the surface focuses the light that
// it bends where it curves up, as a lens, and the bed at `depth` takes about 1 / |1 - depth (n - 1) / n c| of it, where
// c is the curvature of the surface and (n - 1) / n is 0.25 for water. The waves set the caustics' strength (the depth
// times the curvature that they give), their size (the length of the waves that bring the light to a focus near this
// depth: the ripples in the shallows, the longer waves deeper down) and their speed
// (those waves' speed); the cells of UwCaustics give the shape of the lines. The short ripples on the waves are the
// short end of the same spectrum (0.23 to 1.2 m, at the wind sea's steepness). Under small chop the caustics are fine
// and fast; under a big swell, broad and slow; with no waves, none. 1 is the even light.
float UwCausticsFromWaves(vec2 p, float t, float depth)
{
	vec2 sets = UwWaveSets(p, t);
	float curvature = 0.0f;
	float weight = 0.0f;
	float weightedLength = 0.0f;
	for (int i = 0; i < 14; i++)
	{
		vec4 wave = u_uwWaveTable[i];
		float k = 6.2831853f / wave.y;
		float c = wave.z * (i < 2 ? sets.y : sets.x) * k * k;
		// A wave shows most in the caustics at the depth where it brings the light to a focus
		float near = 0.25f * c * depth - 1.0f;
		float w = c / (1.0f + near * near);
		curvature += c;
		weight += w;
		weightedLength += w * wave.y;
	}
	float on = u_uwWaves.x > 0.0f ? 1.0f : 0.0f;
	float steepness = (0.012f + 0.013f * u_uwWaves.y) * on;
	for (int r = 0; r < 6; r++)
	{
		float rippleLength = 1.2f * UW_UNITS_PER_METRE * pow(0.72f, float(r));
		float k = 6.2831853f / rippleLength;
		float c = steepness * rippleLength * sets.x * k * k;
		float near = 0.25f * c * depth - 1.0f;
		float w = c / (1.0f + near * near);
		curvature += c;
		weight += w;
		weightedLength += w * rippleLength;
	}
	if (curvature <= 1e-5f || weight <= 1e-6f)
	{
		return 1.0f;
	}
	// The waves that focus the light at this depth, and how strongly the waves focus it there
	float focusLength = weightedLength / weight;
	float strength = clamp(depth * 0.25f * curvature * 0.35f, 0.0f, 1.0f);
	float k = 6.2831853f / focusLength;
	float speed = sqrt(UW_GRAVITY * k) / 6.2831853f;
	float lines = UwCaustics(p / (focusLength * 0.8f), t * speed * 1.5f, 1.0f) / 0.55f;
	return mix(1.0f, clamp(lines, 0.2f, 3.0f), strength);
}

// The height of the waves of the open sea, far from the coast, as a pixel of the size `footprint` shows them
float UwWavesFiltered(vec2 p, float t, vec4 footprint, out vec2 slope, out float variance)
{
	float breaking;
	float whitecap;
	float chopShare;
	return UwWavesInDepth(p, t, footprint, 1e6f, 1e6f, slope, variance, breaking, whitecap, chopShare);
}

// The height of the waves over the sea's level at a point, and the slope of the surface there. The short waves fade
// far away, where they would be smaller than a pixel: `range` is how far the point is from the eye.
float UwWaves(vec2 p, float t, float range, out vec2 slope)
{
	float variance;
	return UwWavesFiltered(p, t, UwFootprint(range), slope, variance);
}

// The roughness (GGX alpha) of the water at a pixel, from the variance of the slopes that the pixel cannot show, the
// same as underwater::FilteredRoughness
float UwRoughness(float variance)
{
	return min(sqrt(0.1f * 0.1f + max(variance, 0.0f)), 1.0f);
}

// How the highest of the waves is over the sea's level, for the crests' foam: half the sum of their heights
float UwWaveTop()
{
	float sum = 0.0f;
	for (int i = 0; i < 14; i++)
	{
		sum += u_uwWaveTable[i].z;
	}
	for (int j = 0; j < 3; j++)
	{
		sum += u_uwFarWaves[j].z;
	}
	return max(0.5f * sum, 0.05f);
}

// The ripples of a still pond, its height over the level and its slope: broad soft swells that bend the reflections,
// short low ripples on them, and the fine ripples that the wind makes, which run with the wind. All are slow.
float UwPondRipples(vec2 xz, float t, out vec2 slope)
{
	vec2 q = xz * 1.7f;
	float a = 0.008f;
	float height = sin(q.x + t * 0.9f + sin(q.y * 0.6f + t * 0.3f)) * sin(q.y * 1.3f - t * 0.7f + sin(q.x * 0.5f)) * a;
	slope = vec2(cos(q.x + t * 0.9f) * 1.7f, cos(q.y * 1.3f - t * 0.7f) * 2.2f) * a * 0.5f;
	vec2 r = xz * 0.45f;
	float b = 0.025f;
	height += sin(r.x + t * 0.4f) * sin(r.y * 0.8f - t * 0.33f) * b;
	slope += vec2(cos(r.x + t * 0.4f) * sin(r.y * 0.8f - t * 0.33f) * 0.45f,
	              sin(r.x + t * 0.4f) * cos(r.y * 0.8f - t * 0.33f) * 0.36f) *
	         b;
	// The wind's fine ripples: three trains near the wind's direction, 0.5 to 0.9 m long
	vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));
	for (int i = 0; i < 3; i++)
	{
		float turn = (float(i) - 1.0f) * 0.45f;
		vec2 along = vec2(wind.x * cos(turn) - wind.y * sin(turn), wind.x * sin(turn) + wind.y * cos(turn));
		float length_ = (0.5f + 0.2f * float(i)) * UW_UNITS_PER_METRE;
		float k = 6.2831853f / length_;
		float speed = sqrt(UW_GRAVITY * k) * 0.35f;
		float amplitude = 0.0035f * length_ * (0.6f + 0.4f * sin(dot(xz, vec2(0.11f, 0.07f)) + float(i) * 2.1f + t * 0.2f));
		float phase = k * dot(xz, along) - speed * t + float(i) * 1.7f;
		height += sin(phase) * amplitude;
		slope += along * (cos(phase) * amplitude * k);
	}
	return height;
}

// The caustics on the bed of a pond under a point of its surface, 1 the even light: the ripples bend the light from
// above by about a quarter of their slope, so the light from a point of the surface falls on the bed at the point plus
// a quarter of the depth times the slope. Where the ripples curve up, as a lens, the light from a patch of the surface
// falls on a smaller patch of the bed, and the bed is brighter by the ratio of the two areas.
float UwPondCaustics(vec2 p, float t, float depth)
{
	float apart = 0.08f * UW_UNITS_PER_METRE;
	vec2 here;
	vec2 alongX;
	vec2 alongZ;
	UwPondRipples(p, t, here);
	UwPondRipples(p + vec2(apart, 0.0f), t, alongX);
	UwPondRipples(p + vec2(0.0f, apart), t, alongZ);
	float bend = 0.25f * depth;
	float xx = 1.0f + bend * (alongX.x - here.x) / apart;
	float zz = 1.0f + bend * (alongZ.y - here.y) / apart;
	float xz = bend * (alongZ.x - here.x) / apart;
	float zx = bend * (alongX.y - here.y) / apart;
	float area = abs(xx * zz - xz * zx);
	return clamp(1.0f / max(area, 0.25f), 0.3f, 3.0f);
}

// How much a point is in a still pond that does not join the sea, 0 to 1. The shore map keeps it in the length of
// the direction to the coast, over 1 (see underwater::ShoreTexel)
float UwShorePond(vec4 shore)
{
	return clamp((length(shore.yz) - 1.0f) * 2.0f, 0.0f, 1.0f);
}

// The normal of the sea's surface from its slope
vec3 UwSurfaceNormalOf(vec2 slope, float range)
{
	// The waves look flatter far away, where many of them fall in one pixel
	slope *= 1.0f - smoothstep(200.0f, 900.0f, range) * 0.8f;
	return normalize(vec3(-slope.x, 1.0f, -slope.y));
}

// The shore map (see seabed::Seabed::shore): read by each shader from its own sampler, then taken apart here.
// x: the distance to the coast in units, out to sea positive; yz: the direction towards the coast, shorter at the foot
// of a cliff; w: the height of the ground.
vec2 UwShoreUv(vec2 xz)
{
	return (xz / 10.0f + 0.5f) / 513.0f;
}

// The sides of the map in units: the shore map covers it, and holds its edge past it
#define UW_MAP_UNITS 5120.0f

// The shore map past the map's edge: the open, deep sea. The coast is further away by the distance past the edge, no
// cliff is there, and the ground eases down to the far floor (160 under the sea's level). On the map it changes nothing.
vec4 UwOpenSea(vec4 shore, vec2 xz)
{
	vec2 past = max(max(-xz, xz - UW_MAP_UNITS), vec2_splat(0.0f));
	float beyond = length(past);
	if (beyond > 0.0f)
	{
		shore.x = max(shore.x, 0.0f) + beyond;
		shore.yz = -sign(xz - UW_MAP_UNITS * 0.5f) * past / beyond;
		shore.w = mix(shore.w, -160.0f, smoothstep(0.0f, 160.0f, beyond));
	}
	return shore;
}

// How much a point is at the foot of a cliff, 0 to 1
float UwShoreCliff(vec4 shore)
{
	return clamp((1.0f - length(shore.yz)) * 2.0f, 0.0f, 1.0f);
}

// The shore wave's wave number in deep water, radians a unit: 100 m long (underwater::k_ShoreWaveNumber)
#define UW_SHORE_K (6.2831853f / (100.0f * UW_UNITS_PER_METRE))

// How much of the shore wave there is at a depth: it shows where the water is shallow for the height of the waves
// that come in, where they grow, steepen and break
float UwShoreReach(float depth)
{
	return 1.0f - smoothstep(1.2f, 3.0f, max(depth, 0.0f) / max(u_uwPhase.y, 0.05f));
}

// The slope of the sea bed that gives the shore wave its lengths (height for each unit across). The crests follow
// the lines of equal distance to the coast, so the phase comes from the distance only. A phase from the local depth
// makes rings round each hill of the sea bed.
#define UW_SHORE_BED_SLOPE 0.02f
// The distance that keeps the shore wave at least about 6 units long at the edge of the water, in units
#define UW_SHORE_EDGE_DISTANCE 3.0f
// How far inland the phase still changes, in units: the run up the beach moves as one sheet
#define UW_SHORE_INLAND 4.0f

// The shore wave's phase at a distance from the coast, in radians. A shallow water wave moves at the root of gravity
// times the depth. On a bed that goes down evenly, the waves get shorter near the coast. Past the point where they are
// as long as in deep water, they keep that length. Inland the phase stops changing smoothly.
float UwShorePhase(float distance)
{
	float omega = sqrt(UW_GRAVITY * UW_SHORE_K);
	float grade = UW_GRAVITY * UW_SHORE_BED_SLOPE;
	float deepFrom = omega * omega / (grade * UW_SHORE_K * UW_SHORE_K) - UW_SHORE_EDGE_DISTANCE;
	float edgeK = omega / sqrt(grade * UW_SHORE_EDGE_DISTANCE);
	if (distance < 0.0f)
	{
		return -edgeK * UW_SHORE_INLAND * (1.0f - exp(distance / UW_SHORE_INLAND));
	}
	float near = min(distance, deepFrom);
	float shallow = 2.0f * omega / sqrt(grade) * (sqrt(near + UW_SHORE_EDGE_DISTANCE) - sqrt(UW_SHORE_EDGE_DISTANCE));
	return shallow + UW_SHORE_K * max(distance - deepFrom, 0.0f);
}

// The shore wave's wave number at a distance from the coast (radians a unit): the change of UwShorePhase
float UwShoreWaveNumber(float distance)
{
	float omega = sqrt(UW_GRAVITY * UW_SHORE_K);
	float grade = UW_GRAVITY * UW_SHORE_BED_SLOPE;
	return max(UW_SHORE_K, omega / sqrt(grade * (max(distance, 0.0f) + UW_SHORE_EDGE_DISTANCE)));
}

// The smaller of two heights, with a soft change from one to the other, so that no crease shows where they cross
float UwSoftMin(float a, float b)
{
	float a2 = a * a;
	float b2 = b * b;
	return a * b / max(sqrt(sqrt(a2 * a2 + b2 * b2)), 1e-6f);
}

// The larger of two heights, with a soft change from one to the other
float UwSoftMax(float a, float b)
{
	float a2 = a * a;
	float b2 = b * b;
	return sqrt(sqrt(a2 * a2 + b2 * b2));
}

// How big one shore wave is against the usual: its own height, and the waves of a set are higher
float UwShoreWaveSize(float wave)
{
	return (0.6f + 0.8f * UwHash(vec2(wave, 17.0f))) * (0.55f + 0.45f * sin(wave * 0.9f));
}

// The shore wave at a distance from the coast, in water of a depth: one front that rolls in along the whole coast
// towards the land, its crests along the coast. The result is the height over the sea's level. At the foot of a cliff
// and in a pond there is no shore wave.
// - In deep water the wave is a low, round swell.
// - As the water gets shallow, the wave gets shorter and higher, and its face gets steeper.
// - Where the depth cannot hold the wave, it breaks. Then it is a lower front of white water with a steep face.
// - At the edge of the water it runs up the beach fast as a thin sheet and goes back slowly.
// `face` is how much the point is on the face of a wave that breaks, 0 to 1.
float UwShoreWave(float distance, float depth, vec2 xz, float cliff, float pond, out float face)
{
	float theta = UwShorePhase(distance) + u_uwPhase.x + UwNoise(xz * 0.008f) * 1.2f;
	float turns = theta / 6.2831853f;
	float wave = floor(turns);
	// x: 0 at the trough in front of the crest (on the side of the coast), to 1 at the trough behind it
	float x = turns - wave;
	// How near the wave is to the height that the depth holds: under 1 it shoals, over 1 it breaks
	float coming = u_uwPhase.y * UwShoaling(UW_SHORE_K, depth);
	float holds = 0.78f * max(depth, 0.0f);
	float ratio = coming / max(holds, 1e-3f);
	float steep = smoothstep(0.3f, 1.0f, ratio);
	// The face gets steeper and the back longer: the phase moves faster on the front half of the wave. This change
	// of the phase is smooth and always goes forwards, so the surface does not fold.
	float u = 6.2831853f * x;
	float lean = 1.7f * steep;
	float phi = u + 0.5f * lean * (1.0f - cos(u));
	float rise = 0.5f - 0.5f * cos(phi);
	// The crest gets a little narrower as the wave nears its break
	float profile = pow(rise, 1.0f + 0.5f * steep) - 0.45f;
	// The face of the wave that breaks: from the trough in front up to just past the crest
	face = smoothstep(0.02f, 0.12f, x) * (1.0f - smoothstep(0.3f, 0.42f, x)) * smoothstep(0.75f, 1.1f, ratio);
	// How high the wave is, crest to trough: the open sea's waves as they shoal, no higher than the depth holds. At
	// the edge of the water: the run up the beach.
	float grown = UwSoftMin(coming, holds);
	float runUp = u_uwWaves.w * 2.0f * (1.0f - smoothstep(0.0f, 6.0f, distance));
	float height = UwSoftMax(grown * UwShoreReach(depth), runUp);
	// No two waves alike. The size of a wave changes to the size of the next one in the trough behind it, so the
	// surface has no step between two waves. The breakers are also larger at some places along the beach.
	float size = mix(UwShoreWaveSize(wave), UwShoreWaveSize(wave + 1.0f), smoothstep(0.55f, 1.0f, x));
	float along = 0.6f + 0.8f * UwNoise(xz * 0.012f + vec2(3.0f, 7.0f));
	return height * (1.0f - cliff) * (1.0f - pond) * size * along * profile;
}

// A crash of the shore wave on the foot of a cliff at a point of the coast: the same wave as UwShoreWave, that
// reaches the coast there at its own time. It gives how strong the last wave to reach the coast there was, about 0.05
// to 2 (0.55 for a usual wave): its own height, the height of its set and how big the breakers are at that place.
// `since` is the time since it reached the coast, in seconds; `crash` is the number of the wave, for the hashes that
// make each crash different; `period` is the time from one wave to the next.
float UwCliffCrash(vec2 xz, float t, out float since, out float crash, out float period)
{
	// The shore wave's angular speed (underwater::ShoreOmega) and its phase, which grows each frame
	float omega = 6.2831853f / (8.0f + 3.0f * u_uwWaves.y);
	float theta = u_uwPhase.x + UwNoise(xz * 0.008f) * 1.2f;
	period = 6.2831853f / omega;
	crash = floor(theta / 6.2831853f);
	since = fract(theta / 6.2831853f) * period;
	float own = 0.6f + 0.8f * UwHash(vec2(crash, 17.0f));
	float sets = 0.55f + 0.45f * sin(crash * 0.9f);
	float along = 0.6f + 0.8f * UwNoise(xz * 0.012f + vec2(3.0f, 7.0f));
	return own * sets * along;
}

// The waves that go back out to sea from the foot of a cliff, in rings
float UwCliffBackwash(float distance, float t, float cliff)
{
	float k = 6.2831853f / (18.0f * UW_UNITS_PER_METRE);
	return cliff * u_uwWaves.w * 0.6f * sin(k * distance - 1.3f * t) * exp(-max(distance, 0.0f) / 25.0f);
}

#include "underwater_wake.sh"

// The rogue wave of a strong storm (see underwater::RogueWave). [0] xy: the middle of its crest line, z: its direction
// in radians, w: its crest over the mean level (0 for no wave); [1] x: its length, y: half the length of its crest
// line, z: how much its crest breaks, w: how far the ends of the crest line stay behind
uniform vec4 u_uwRogue[2];

// The lightning on the sea (see underwater::SeaStrike). [0] xyz: where it struck, on the sea's level, w: how bright its
// bolt is now, 0 to 1; [1] x: how bright the flash of the sky over the camera is, 0 to 1, y: the time since it struck,
// in seconds, z: how strong the strike was where it hit the sea (0 where it did not), w: 1 while it shows; [2] xyz: the
// middle of the brightest flash inside the clouds, w: its light now (see cloud_lightning::LightFor)
uniform vec4 u_uwStrike[3];
// The colour of the light of the lightning
#define UW_FLASH_COLOUR vec3(0.82f, 0.88f, 1.0f)

// One bump of the rogue wave's profile: x its value, y its rise along the profile
vec2 UwRogueBump(float u, float middle, float width, float amount)
{
	float s = (u - middle) / width;
	float value = amount * exp(-s * s);
	return vec2(value, value * (-2.0f * s / width));
}

// The rogue wave's part of the sea's height at a point, and its slope; `crestShare` is how much of its crest is at the
// point (0 to 1), for its foam. The same as underwater::RogueWaveHeight: change them together.
float UwRogueWave(vec2 xz, float shoreDistance, float depth, out vec2 slope, out float crestShare)
{
	slope = vec2_splat(0.0f);
	crestShare = 0.0f;
	vec4 a = u_uwRogue[0];
	vec4 b = u_uwRogue[1];
	if (a.w <= 0.0f)
	{
		return 0.0f;
	}
	vec2 direction = vec2(cos(a.z), sin(a.z));
	vec2 across = vec2(-direction.y, direction.x);
	vec2 offset = xz - a.xy;
	float side = dot(offset, across) / b.y;
	if (abs(side) > 2.0f)
	{
		return 0.0f;
	}
	// The crest line bends back towards its ends
	float u = (dot(offset, direction) + b.w * b.x * side * side) / b.x;
	if (u < -1.6f || u > 0.9f)
	{
		return 0.0f;
	}
	// The crest, with a steep face in front and a long back, the deep trough in front of it, the trough behind it and
	// a smaller wave behind that
	vec2 crest = UwRogueBump(u, 0.0f, u >= 0.0f ? 0.08f : 0.2f, 1.0f);
	vec2 profile = crest + UwRogueBump(u, 0.3f, 0.14f, -0.5f) + UwRogueBump(u, -0.55f, 0.2f, -0.35f) +
	               UwRogueBump(u, -1.0f, 0.14f, 0.3f);
	float ends = exp(-(side * side * side * side));
	float endsRise = ends * (-4.0f * side * side * side / b.y);
	float fade = smoothstep(60.0f, 200.0f, shoreDistance) * smoothstep(3.0f, 15.0f, depth);
	vec2 uRise = (direction + across * (b.w * 2.0f * side / b.y * b.x)) / b.x;
	slope = a.w * fade * (uRise * (profile.y * ends) + across * (profile.x * endsRise));
	crestShare = crest.x * ends * fade;
	return a.w * fade * profile.x * ends;
}

// The height of the sea's surface at a point: the sea's level, the deep waves, faded out near the coast and where the
// water is shallow, and the shore wave that rolls in to the coast. One height for each point, so the water is one
// surface from the open sea to the edge of the run up the beach. `shore` is the shore map at the point, `footprint`
// the size of the pixel on the water (see UwFootprint); `slope` is the surface's slope, `face` the shore wave's face
// and `variance` the variance of the slope that the pixel cannot show.
float UwSurfaceSheltered(vec2 xz, float t, vec4 footprint, vec4 shore, float shelter, out vec2 slope, out float face,
                         out float variance, out float whitecap, out float chopShare);

float UwSurfaceFiltered(vec2 xz, float t, vec4 footprint, vec4 shore, out vec2 slope, out float face, out float variance)
{
	float whitecap;
	float chopShare;
	return UwSurfaceSheltered(xz, t, footprint, shore, 1.0f, slope, face, variance, whitecap, chopShare);
}

// The same, with `shelter` (0 to 1) the share of the open sea's waves that reach the point past the land upwind of it:
// a bay behind a headland has less swell. `whitecap` is the crest envelope of the sea's waves (UwWhitecap), and
// `chopShare` the share of the chop of the wind that is in the height (its slope is not in `slope`).
float UwSurfaceSheltered(vec2 xz, float t, vec4 footprint, vec4 shore, float shelter, out vec2 slope, out float face,
                         out float variance, out float whitecap, out float chopShare)
{
	float level = u_uwParams.z;
	float depth = level - shore.w;
	float pond = UwShorePond(shore);
	// The waves of the sea, grown and broken by the depth; where the shore wave rolls in, they give way to it. A pond
	// has none: only small ripples that the wind makes.
	float breaking;
	vec2 deepSlope;
	float height = UwWavesInDepth(xz, t, footprint, shore.x, depth, deepSlope, variance, breaking, whitecap, chopShare);
	float deep = smoothstep(0.05f, 0.6f, depth) * (1.0f - 0.7f * UwShoreReach(depth)) * (1.0f - pond) * shelter;
	whitecap *= deep;
	chopShare *= deep;
	height *= deep;
	variance *= deep * deep;
	float cliff = UwShoreCliff(shore);
	float faceNear;
	// The shore wave fades where a pixel is too large to show it, as the sea's waves do
	float shoreShown = UwWaveShown(6.2831853f / UwShoreWaveNumber(shore.x), footprint.x).x;
	float wave = UwShoreWave(shore.x, depth, xz, cliff, pond, face) * shoreShown + UwCliffBackwash(shore.x, t, cliff);
	// The shore wave's slope, along the direction to the coast
	float nearer = UwShoreWave(shore.x - 0.5f, depth, xz, cliff, pond, faceNear) * shoreShown +
	               UwCliffBackwash(shore.x - 0.5f, t, cliff);
	face *= shoreShown;
	float length2 = dot(shore.yz, shore.yz);
	vec2 towards = length2 > 1e-6f ? shore.yz / sqrt(length2) : vec2_splat(0.0f);
	slope = deepSlope * deep + towards * ((nearer - wave) / 0.5f);
	// The ripples of a pond (UwPondRipples)
	if (pond > 0.0f)
	{
		vec2 pondSlope;
		height += UwPondRipples(xz, t, pondSlope) * pond;
		slope += pondSlope * pond;
	}
	// The wakes and the rings of the things in the water, and the hulls that push the water down
	vec2 wakeSlope;
	float wake = UwWake(xz, wakeSlope);
	slope += wakeSlope;
	// The rogue wave of a strong storm, which dies before the coast
	vec2 rogueSlope;
	float rogueCrest;
	float rogue = UwRogueWave(xz, shore.x, depth, rogueSlope, rogueCrest);
	slope += rogueSlope;
	return level + height + wave + wake + rogue;
}

// The height of the sea's surface at a point, as UwSurfaceFiltered gives it for a round pixel at `range` from the eye
float UwSurface(vec2 xz, float t, float range, vec4 shore, out vec2 slope, out float face)
{
	float variance;
	return UwSurfaceFiltered(xz, t, UwFootprint(range), shore, slope, face, variance);
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
