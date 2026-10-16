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
// x: the storm's whitecaps and streaks of foam, y: the reef, z: the wet rock of the cliffs, w: the spray's mist and
// haze; 1 on, 0 off
uniform vec4 u_uwFlags2;
// The sea's currents, one field for every moving thing in the water (see underwater::CurrentAt). xy: the steady flow
// in units a second, z: how strong the swirls are, w: how strong the gusts are
uniform vec4 u_uwCurrent;

// How much of each colour the water takes out for each unit the light goes through it: the red first, then the green
// The lowest low tide under the usual height (underwater::k_LowestTide)
#define UW_LOWEST_TIDE 2.5f

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

// The height of the waves over the sea's level at a point, and the slope of the surface there. The short waves fade
// far away, where they would be smaller than a pixel: `range` is how far the point is from the eye.
float UwWaves(vec2 p, float t, float range, out vec2 slope)
{
	slope = vec2_splat(0.0f);
	vec2 sets = UwWaveSets(p, t);
	float sharp = 1.0f + 1.5f * u_uwWaves.y;
	float height = 0.0f;
	for (int i = 0; i < 14; i++)
	{
		vec4 wave = u_uwWaveTable[i];
		vec2 along = vec2(cos(wave.x), sin(wave.x));
		float k = 6.2831853f / wave.y;
		float omega = sqrt(9.8f * k) * 0.8f;
		// A wave shorter than about four pixels here fades out
		float footprint = 0.0015f * max(range, 1.0f);
		float shown = smoothstep(footprint * 4.0f, footprint * 8.0f, wave.y);
		float a = wave.z * (i < 2 ? sets.y : sets.x) * shown;
		float theta = k * dot(along, p) - omega * t + wave.w;
		float s = 0.5f + 0.5f * sin(theta);
		height += a * (2.0f * pow(max(s, 0.0f), sharp) - 1.0f);
		slope += along * (a * sharp * pow(max(s, 1e-4f), sharp - 1.0f) * cos(theta) * k);
	}
	return height;
}

// How the highest of the waves is over the sea's level, for the crests' foam
float UwWaveTop()
{
	return 0.8f * u_uwWaves.x;
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

// How much a point is at the foot of a cliff, 0 to 1
float UwShoreCliff(vec4 shore)
{
	return clamp((1.0f - length(shore.yz)) * 2.0f, 0.0f, 1.0f);
}

// The shore wave at a distance from the coast: one front that rolls in along the whole coast towards the land, its
// crests along the coast, with a steep face and a long back. It is the height over the sea's level; `face` is 1 on
// the face of the wave and 0 elsewhere. At the foot of a cliff the wave does not run up.
float UwShoreWave(float distance, vec2 xz, float t, float cliff, out float face)
{
	float storm = u_uwWaves.y;
	float k = 6.2831853f / mix(14.0f, 22.0f, storm);
	float omega = mix(1.1f, 1.5f, storm);
	float theta = k * distance + omega * t + UwNoise(xz * 0.008f) * 1.2f;
	float x = fract(theta / 6.2831853f);
	float profile = smoothstep(0.0f, 0.1f, x) * pow(max(1.0f - x, 0.0f), 1.6f);
	face = 1.0f - smoothstep(0.0f, 0.15f, x);
	float reach = 1.0f - smoothstep(8.0f, 45.0f, distance);
	// No two waves alike: each wave has its own height, the waves of a set are higher, and the breakers are larger in
	// some places along the beach than in others
	float wave = floor(theta / 6.2831853f);
	float own = 0.6f + 0.8f * UwHash(vec2(wave, 17.0f));
	float sets = 0.55f + 0.45f * sin(wave * 0.9f);
	float along = 0.6f + 0.8f * UwNoise(xz * 0.012f + vec2(3.0f, 7.0f));
	return u_uwWaves.w * 2.0f * reach * (1.0f - cliff) * own * sets * along * (profile - 0.3f);
}

// A crash of the shore wave on the foot of a cliff at a point of the coast: the same wave as UwShoreWave, that
// reaches the coast there at its own time. It gives how strong the last wave to reach the coast there was, about 0.05
// to 2 (0.55 for a usual wave): its own height, the height of its set and how big the breakers are at that place.
// `since` is the time since it reached the coast, in seconds; `crash` is the number of the wave, for the hashes that
// make each crash different; `period` is the time from one wave to the next.
float UwCliffCrash(vec2 xz, float t, out float since, out float crash, out float period)
{
	float omega = mix(1.1f, 1.5f, u_uwWaves.y);
	float theta = omega * t + UwNoise(xz * 0.008f) * 1.2f;
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
	float k = 6.2831853f / mix(14.0f, 22.0f, u_uwWaves.y);
	return cliff * u_uwWaves.w * 0.6f * sin(k * distance - 1.3f * t) * exp(-max(distance, 0.0f) / 25.0f);
}

// The height of the sea's surface at a point: the sea's level, the deep waves, faded out near the coast and where the
// water is shallow, and the shore wave that rolls in to the coast. One height for each point, so the water is one
// surface from the open sea to the edge of the run up the beach. `shore` is the shore map at the point, `range` how
// far the point is from the eye; `slope` is the surface's slope, `face` the shore wave's face.
float UwSurface(vec2 xz, float t, float range, vec4 shore, out vec2 slope, out float face)
{
	float level = u_uwParams.z;
	float depth = level - shore.w;
	float deep = smoothstep(5.0f, 40.0f, shore.x) * smoothstep(0.2f, 4.0f, depth);
	vec2 deepSlope;
	float height = UwWaves(xz, t, range, deepSlope) * deep;
	float cliff = UwShoreCliff(shore);
	float faceNear;
	float wave = UwShoreWave(shore.x, xz, t, cliff, face) + UwCliffBackwash(shore.x, t, cliff);
	// The shore wave's slope, along the direction to the coast
	float nearer = UwShoreWave(shore.x - 0.5f, xz, t, cliff, faceNear) + UwCliffBackwash(shore.x - 0.5f, t, cliff);
	float length2 = dot(shore.yz, shore.yz);
	vec2 towards = length2 > 1e-6f ? shore.yz / sqrt(length2) : vec2_splat(0.0f);
	slope = deepSlope * deep + towards * ((nearer - wave) / 0.5f);
	return level + height + wave;
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
