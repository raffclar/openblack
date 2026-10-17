#ifndef ATMOSPHERE_SH
#define ATMOSPHERE_SH

// EXPERIMENT (not for merge): the air's light, for the sky near the horizon and the haze over the land and the sea. The
// formulas match src/Graphics/Atmosphere.cpp, which the tests check. Distances are in kilometres unless a name says
// otherwise. The textures of the air are at sampler stages 13 (the light that gets through to the top) and 14 (the sky
// round the eye).

// x: how much dust the air holds (1 for clean air); y: how much denser the haze is than the real air; z: the eye's
// height over the sea in kilometres; w: the sea's level in world units
uniform vec4 u_atmosParams;
// xyz: the direction towards the sun or the moon; w: 1 while the haze shows
uniform vec4 u_atmosLight;
// rgb: what the air's light is scaled by on the screen (the light's colour and the exposure); w: unused
uniform vec4 u_atmosSun;

#define ATMOS_PI 3.14159265f
#define ATMOS_GROUND_RADIUS 6360.0f
#define ATMOS_TOP_RADIUS 6460.0f
#define ATMOS_RAYLEIGH_SCATTERING vec3(5.802e-3f, 13.558e-3f, 33.1e-3f)
#define ATMOS_RAYLEIGH_HEIGHT 8.0f
#define ATMOS_MIE_SCATTERING 3.996e-3f
#define ATMOS_MIE_EXTINCTION 4.440e-3f
#define ATMOS_MIE_HEIGHT 1.2f
#define ATMOS_MIE_FORWARD 0.8f
#define ATMOS_OZONE_ABSORPTION vec3(0.650e-3f, 1.881e-3f, 0.085e-3f)
#define ATMOS_OZONE_MIDDLE 25.0f
#define ATMOS_OZONE_HALF_WIDTH 15.0f
#define ATMOS_MULTIPLE_SHARE 0.3f
#define ATMOS_UNITS_PER_KM 949.0f
#define ATMOS_HAZE_LAYER_HEIGHT 2.5f
// The haze's grid (atmosphere::k_Aerial* in Atmosphere.h)
#define ATMOS_AERIAL_CELLS 32.0f
#define ATMOS_AERIAL_SLICES 32.0f
#define ATMOS_AERIAL_TILES_ACROSS 8.0f
#define ATMOS_AERIAL_NEAREST 8.0f
#define ATMOS_AERIAL_FARTHEST 24000.0f

SAMPLER2D(s_atmosTransmittance, 13);
SAMPLER2D(s_atmosSkyView, 14);

float AtmosSafeSqrt(float x)
{
	return sqrt(max(x, 0.0f));
}

// rgb: what the molecules scatter; a: what the dust scatters
vec4 AtmosScattering(float altitude)
{
	float h = max(altitude, 0.0f);
	return vec4(ATMOS_RAYLEIGH_SCATTERING * exp(-h / ATMOS_RAYLEIGH_HEIGHT),
	            ATMOS_MIE_SCATTERING * exp(-h / ATMOS_MIE_HEIGHT) * u_atmosParams.x);
}

vec3 AtmosExtinction(float altitude)
{
	float h = max(altitude, 0.0f);
	float ozone = max(0.0f, 1.0f - abs(h - ATMOS_OZONE_MIDDLE) / ATMOS_OZONE_HALF_WIDTH);
	return ATMOS_RAYLEIGH_SCATTERING * exp(-h / ATMOS_RAYLEIGH_HEIGHT) +
	       vec3_splat(ATMOS_MIE_EXTINCTION * exp(-h / ATMOS_MIE_HEIGHT) * u_atmosParams.x) + ATMOS_OZONE_ABSORPTION * ozone;
}

float AtmosRayleighPhase(float cosAngle)
{
	return 3.0f / (16.0f * ATMOS_PI) * (1.0f + cosAngle * cosAngle);
}

float AtmosMiePhase(float cosAngle)
{
	float g = ATMOS_MIE_FORWARD;
	float g2 = g * g;
	float d = 1.0f + g2 - 2.0f * g * cosAngle;
	return 3.0f / (8.0f * ATMOS_PI) * (1.0f - g2) * (1.0f + cosAngle * cosAngle) / ((2.0f + g2) * d * sqrt(d));
}

float AtmosDistanceToTop(float radius, float mu)
{
	return -radius * mu + AtmosSafeSqrt(radius * radius * (mu * mu - 1.0f) + ATMOS_TOP_RADIUS * ATMOS_TOP_RADIUS);
}

float AtmosDistanceToGround(float radius, float mu)
{
	return -radius * mu - AtmosSafeSqrt(radius * radius * (mu * mu - 1.0f) + ATMOS_GROUND_RADIUS * ATMOS_GROUND_RADIUS);
}

bool AtmosHitsGround(float radius, float mu)
{
	return mu < 0.0f && radius * radius * (mu * mu - 1.0f) + ATMOS_GROUND_RADIUS * ATMOS_GROUND_RADIUS >= 0.0f;
}

// Where a height and an angle are in the texture of the light that gets through (Bruneton's mapping)
vec2 AtmosTransmittanceUv(float altitude, float mu)
{
	float radius = ATMOS_GROUND_RADIUS + clamp(altitude, 0.0f, ATMOS_TOP_RADIUS - ATMOS_GROUND_RADIUS);
	float horizon = sqrt(ATMOS_TOP_RADIUS * ATMOS_TOP_RADIUS - ATMOS_GROUND_RADIUS * ATMOS_GROUND_RADIUS);
	float rho = AtmosSafeSqrt(radius * radius - ATMOS_GROUND_RADIUS * ATMOS_GROUND_RADIUS);
	float d = AtmosDistanceToTop(radius, mu);
	float dMin = ATMOS_TOP_RADIUS - radius;
	float dMax = rho + horizon;
	return vec2((d - dMin) / (dMax - dMin), rho / horizon);
}

// The part of each colour that gets through from a height to the top along a ray at mu from straight up; none where the
// ray hits the ground
vec3 AtmosTransmittance(float altitude, float mu)
{
	float radius = ATMOS_GROUND_RADIUS + max(altitude, 0.0f);
	if (AtmosHitsGround(radius, mu))
	{
		return vec3_splat(0.0f);
	}
	return texture2DLod(s_atmosTransmittance, AtmosTransmittanceUv(altitude, mu), 0.0f).rgb;
}

// The light that the air scatters more than once, for each steradian, at a height with the light at mu
vec3 AtmosMultipleScattering(float altitude, float mu)
{
	return ATMOS_MULTIPLE_SHARE * AtmosTransmittance(altitude, max(mu, 0.05f)) * smoothstep(-0.25f, 0.05f, mu) /
	       (4.0f * ATMOS_PI);
}

// The light that the air at a height scatters towards the eye for each kilometre, with the light at lightMu and the
// angle between the ray and the light at cosAngle, and `lit` of the direct light reaching it
vec3 AtmosScatterAt(float altitude, float lightMu, float cosAngle, float lit)
{
	vec4 scattering = AtmosScattering(altitude);
	vec3 single = scattering.rgb * AtmosRayleighPhase(cosAngle) + vec3_splat(scattering.a * AtmosMiePhase(cosAngle));
	vec3 sun = AtmosTransmittance(altitude, lightMu);
	return single * sun * lit + (scattering.rgb + vec3_splat(scattering.a)) * AtmosMultipleScattering(altitude, lightMu);
}

// Where a direction is in the texture of the sky round the eye (Hillaire's mapping)
vec2 AtmosSkyViewUv(float viewMu, float lightCos, float altitude)
{
	float radius = ATMOS_GROUND_RADIUS + max(altitude, 1e-3f);
	float beta = acos(clamp(sqrt(radius * radius - ATMOS_GROUND_RADIUS * ATMOS_GROUND_RADIUS) / radius, -1.0f, 1.0f));
	float zenithHorizon = ATMOS_PI - beta;
	float viewAngle = acos(clamp(viewMu, -1.0f, 1.0f));
	float v = viewAngle < zenithHorizon ? (1.0f - AtmosSafeSqrt(1.0f - viewAngle / zenithHorizon)) * 0.5f
	                                    : AtmosSafeSqrt((viewAngle - zenithHorizon) / beta) * 0.5f + 0.5f;
	float u = AtmosSafeSqrt(-clamp(lightCos, -1.0f, 1.0f) * 0.5f + 0.5f);
	return vec2(u, v);
}

// The sky's light from a world direction, for a light of strength 1
vec3 AtmosSky(vec3 dir)
{
	vec2 across = dir.xz;
	vec2 light = u_atmosLight.xz;
	float lightCos = dot(across, across) > 1e-8f && dot(light, light) > 1e-8f ? dot(normalize(across), normalize(light)) : 1.0f;
	return texture2DLod(s_atmosSkyView, AtmosSkyViewUv(dir.y, lightCos, u_atmosParams.z), 0.0f).rgb;
}

// A colour on the screen with its brightest parts eased under 1 (the glow round a low sun), unchanged under 0.8
vec3 AtmosSoftClip(vec3 colour)
{
	vec3 over = max(colour - 0.8f, vec3_splat(0.0f));
	return min(colour, vec3_splat(0.8f)) + 0.2f * (vec3_splat(1.0f) - exp(-over / 0.2f));
}

// How much denser the haze is than the real air at a height (atmosphere::HazeScaleAt): u_atmosParams.y at the sea's
// level, and near 1 high up
float AtmosHazeScaleAt(float altitude)
{
	return 1.0f + (u_atmosParams.y - 1.0f) * exp(-max(altitude, 0.0f) / ATMOS_HAZE_LAYER_HEIGHT);
}

// The mean of exp(-h / scaleHeight) along a straight path from the height low to the height high
float AtmosMeanFalloff(float low, float high, float scaleHeight)
{
	float rise = high - low;
	return rise > 1e-3f ? scaleHeight * (exp(-low / scaleHeight) - exp(-high / scaleHeight)) / rise : exp(-low / scaleHeight);
}

// The mean of the haze's dimming for each kilometre, as the eye sees it, along a straight path from one height to
// another (in kilometres). The air thins with the height, the dust faster than the molecules, and the extra haze of
// the low air faster than both.
float AtmosMeanExtinction(float fromHeight, float toHeight)
{
	float low = max(min(fromHeight, toHeight), 0.0f);
	float high = max(max(fromHeight, toHeight), 0.0f);
	float extra = u_atmosParams.y - 1.0f;
	float rayleighLayer = 1.0f / (1.0f / ATMOS_RAYLEIGH_HEIGHT + 1.0f / ATMOS_HAZE_LAYER_HEIGHT);
	float mieLayer = 1.0f / (1.0f / ATMOS_MIE_HEIGHT + 1.0f / ATMOS_HAZE_LAYER_HEIGHT);
	float rayleigh = AtmosMeanFalloff(low, high, ATMOS_RAYLEIGH_HEIGHT) + extra * AtmosMeanFalloff(low, high, rayleighLayer);
	float mie = AtmosMeanFalloff(low, high, ATMOS_MIE_HEIGHT) + extra * AtmosMeanFalloff(low, high, mieLayer);
	const vec3 luma = vec3(0.2126f, 0.7152f, 0.0722f);
	return dot(ATMOS_RAYLEIGH_SCATTERING, luma) * rayleigh + ATMOS_MIE_EXTINCTION * u_atmosParams.x * mie;
}

// The sky as the dome shows it on the screen in a direction, with `sunHeight` the sine of the air's sun over the
// horizon. The haze over the far land and the far sea goes to this colour, so the land and the sea fade into the sky.
vec3 AtmosSkyOnScreen(vec3 dir, float sunHeight)
{
	vec3 air = AtmosSky(dir) * u_atmosSun.rgb;
	// By day the air's sky high up is lighter than the plain model gives, as the original dome is, so the sky does not
	// go to a deep blue over the eye. The sky at dusk and at night keeps the model's own brightness.
	const vec3 luma = vec3(0.2126f, 0.7152f, 0.0722f);
	float lift = 1.0f + 0.8f * smoothstep(0.05f, 0.6f, dir.y) * smoothstep(-0.02f, 0.2f, u_atmosLight.y);
	air = AtmosSoftClip(air * lift);
	// After sunset and in the moon's light the model gives a deep, strong blue. The eye sees the dim sky with less
	// colour, so the sky goes part of the way to grey while the air's sun is low.
	float grey = dot(air, luma);
	return mix(vec3_splat(grey), air, mix(0.25f, 1.0f, smoothstep(0.0f, 0.35f, sunHeight)));
}

// The distance of a slice of the haze's grid from the eye in world units, and the slice at a distance
float AtmosSliceDistance(float slice)
{
	return ATMOS_AERIAL_NEAREST * exp(log(ATMOS_AERIAL_FARTHEST / ATMOS_AERIAL_NEAREST) * (slice + 0.5f) / ATMOS_AERIAL_SLICES);
}

float AtmosSliceAt(float distance)
{
	return log(max(distance, 1e-3f) / ATMOS_AERIAL_NEAREST) / log(ATMOS_AERIAL_FARTHEST / ATMOS_AERIAL_NEAREST) *
	           ATMOS_AERIAL_SLICES -
	       0.5f;
}

// Where a point of the screen (0 to 1) is in a slice's tile of the haze's texture, kept half a texel inside the tile
vec2 AtmosAerialUv(vec2 screenUv, float slice)
{
	float rows = ATMOS_AERIAL_SLICES / ATMOS_AERIAL_TILES_ACROSS;
	vec2 tile = vec2(mod(slice, ATMOS_AERIAL_TILES_ACROSS), floor(slice / ATMOS_AERIAL_TILES_ACROSS));
	vec2 inside = clamp(screenUv, vec2_splat(0.5f / ATMOS_AERIAL_CELLS), vec2_splat(1.0f - 0.5f / ATMOS_AERIAL_CELLS));
	return (tile + inside) / vec2(ATMOS_AERIAL_TILES_ACROSS, rows);
}

#endif // ATMOSPHERE_SH
