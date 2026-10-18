#ifndef GALAXY_SH
#define GALAXY_SH

// EXPERIMENT (not for merge): the Milky Way and the faint nebulae on the sky's sphere. The smooth shape of the band and
// of its dust matches src/Graphics/Galaxy.cpp (galaxy::BandBrightness, galaxy::DustDepth), which the tests check.
// Noise adds the star clouds and the clumps of the dust. Include atmosphere.sh and world_curve.sh first.

// How bright the band is on the screen for a smooth brightness of 1
#define GALAXY_BRIGHTNESS 0.28f
// The sky's luminance where the band shows at full strength and where it does not show (galaxy::Visibility)
#define GALAXY_FULL_LUMINANCE 0.025f
#define GALAXY_NO_LUMINANCE 0.1f
// How much of the band a full moon high in the sky covers (galaxy::k_MoonWash)
#define GALAXY_MOON_WASH 0.85f
// The share of the island's light that a full moon gives (u_planetMoon.w at a full moon)
#define GALAXY_MOON_SHARE 0.35f
#define GALAXY_DEGREES 57.2957795f

// A direction on the sky's sphere in the galaxy's frame (galaxy::k_SkyToGalaxy): x towards the middle of the galaxy,
// y towards a galactic longitude of 90 degrees, z towards the north pole of the galaxy
vec3 GalacticDirection(vec3 sky)
{
	return vec3(dot(vec3(-0.0548755604f, -0.8734370902f, -0.4838350155f), sky),
	            dot(vec3(0.4941094279f, -0.4448296300f, 0.7469822445f), sky),
	            dot(vec3(-0.8676661490f, -0.1980763734f, 0.4559837762f), sky));
}

// The galactic longitude (-180 to 180) and latitude in degrees
vec2 GalaxyLongitudeLatitude(vec3 galactic)
{
	return vec2(atan2(galactic.y, galactic.x), asin(clamp(galactic.z, -1.0f, 1.0f))) * GALAXY_DEGREES;
}

float GalaxyBell(float x, float spread)
{
	return exp(-(x * x) / (2.0f * spread * spread));
}

float GalaxyOval(vec2 at, vec2 middle, vec2 size)
{
	vec2 d = (at - middle) / size;
	return exp(-dot(d, d));
}

// The smooth brightness of the disc and the bulge, before the dust (galaxy::BandBrightness). `bulge` gets the bulge's
// part.
float GalaxyBand(vec2 lb, out float bulge)
{
	float l = lb.x;
	float b = lb.y;
	float towardsMiddle = 0.5f + 0.5f * cos(l / GALAXY_DEGREES);
	float along = 0.22f + 0.78f * towardsMiddle * towardsMiddle + 0.30f * GalaxyBell(l - 75.0f, 12.0f) +
	              0.25f * GalaxyBell(l + 68.0f, 14.0f) + 0.25f * GalaxyBell(l - 27.0f, 5.0f) +
	              0.15f * GalaxyBell(l + 30.0f, 8.0f);
	float width = 4.0f + 3.0f * towardsMiddle;
	float disc = along * (0.7f * exp(-abs(b) / width) + 0.3f * exp(-abs(b) / (2.5f * width)));
	bulge = 1.1f * GalaxyOval(lb, vec2(0.0f, -1.0f), vec2(8.5f, 6.0f));
	return disc + bulge;
}

// The optical depth of the dark dust clouds (galaxy::DustDepth)
float GalaxyDust(vec2 lb)
{
	float l = lb.x;
	float b = lb.y;
	float lane = 1.3f * GalaxyBell(b, 1.2f) * (0.35f + 0.65f * GalaxyBell(l, 50.0f));
	float riftLatitude = 0.8f + 5.5f * (1.0f - smoothstep(0.0f, 45.0f, l));
	float rift = 1.1f * GalaxyBell(b - riftLatitude, 2.0f) * smoothstep(-8.0f, 2.0f, l) * (1.0f - smoothstep(78.0f, 92.0f, l));
	float pipe = 1.4f * GalaxyOval(lb, vec2(0.5f, 5.5f), vec2(3.5f, 1.4f));
	float rhoOphiuchi = 0.9f * GalaxyOval(lb, vec2(-6.5f, 16.5f), vec2(3.5f, 2.5f));
	float coalsack = 2.2f * GalaxyOval(lb, vec2(-58.5f, -1.0f), vec2(2.6f, 2.3f));
	float northernCoalsack = 1.0f * GalaxyOval(lb, vec2(82.0f, 1.0f), vec2(5.0f, 2.2f));
	float taurus = 0.5f * GalaxyOval(lb, vec2(172.0f, -15.0f), vec2(6.0f, 4.0f));
	return lane + rift + pipe + rhoOphiuchi + coalsack + northernCoalsack + taurus;
}

// The density of the faint stars in a direction on the sky's sphere: about 1 off the band, up to 3 to 4 on its bright
// star clouds (galaxy::StarDensity)
float GalaxyStarDensity(vec3 celestialDir)
{
	vec2 lb = GalaxyLongitudeLatitude(GalacticDirection(celestialDir));
	float bulge;
	return 1.0f + 1.5f * GalaxyBand(lb, bulge) * exp(-0.7f * GalaxyDust(lb));
}

// How much of the band shows in front of a sky of a luminance on the screen (galaxy::Visibility)
float GalaxyVisibility(float skyLuminance)
{
	return 1.0f - smoothstep(GALAXY_FULL_LUMINANCE, GALAXY_NO_LUMINANCE, skyLuminance);
}

// How much of the band the moon's light leaves, from the moon's brightness by its phase (0 new, 1 full) and the sine
// of its height over the horizon (galaxy::MoonVisibility)
float GalaxyMoonVisibility(float moonBrightness, float moonSine)
{
	return 1.0f - GALAXY_MOON_WASH * moonBrightness * smoothstep(-0.05f, 0.25f, moonSine);
}

float GalaxyHash(vec3 p)
{
	p = fract(p * 0.1031f);
	p += dot(p, p.zyx + 31.32f);
	return fract((p.x + p.y) * p.z);
}

// Smooth value noise, 0 to 1
float GalaxyNoise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0f - 2.0f * f);
	float a = mix(GalaxyHash(i), GalaxyHash(i + vec3(1.0f, 0.0f, 0.0f)), f.x);
	float b = mix(GalaxyHash(i + vec3(0.0f, 1.0f, 0.0f)), GalaxyHash(i + vec3(1.0f, 1.0f, 0.0f)), f.x);
	float c = mix(GalaxyHash(i + vec3(0.0f, 0.0f, 1.0f)), GalaxyHash(i + vec3(1.0f, 0.0f, 1.0f)), f.x);
	float d = mix(GalaxyHash(i + vec3(0.0f, 1.0f, 1.0f)), GalaxyHash(i + vec3(1.0f, 1.0f, 1.0f)), f.x);
	return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}

// A soft oval patch round `middle`; `u` and `v` are its two axes, each divided by its half size in radians
float GalaxyPatch(vec3 sky, vec3 middle, vec3 u, vec3 v)
{
	vec3 d = sky - middle;
	float x = dot(d, u);
	float y = dot(d, v);
	// Only the half of the sphere round the patch: the opposite point gives the same offsets along the axes
	return exp(-(x * x + y * y)) * smoothstep(0.0f, 0.3f, dot(sky, middle));
}

// The light of the nebulae, the Magellanic clouds and the Andromeda galaxy, on the scale of the band's brightness
vec3 GalaxyNebulae(vec3 sky)
{
	vec3 light = vec3_splat(0.0f);
	// Orion Nebula
	light += GalaxyPatch(sky, vec3(0.107176f, 0.989793f, -0.093935f), vec3(1.053437f, 9.728684f, 103.713526f), vec3(-126.584024f, 13.706720f, 0.000000f)) * vec3(0.750000f, 0.375000f, 0.465000f);
	// Lambda Orionis ring
	light += GalaxyPatch(sky, vec3(0.105527f, 0.979350f, 0.172445f), vec3(-0.311324f, -2.889265f, 16.599247f), vec3(-18.988678f, 2.046066f, 0.000000f)) * vec3(0.060000f, 0.018000f, 0.021600f);
	// Rosette
	light += GalaxyPatch(sky, vec3(-0.138310f, 0.986623f, 0.086286f), vec3(1.055910f, -7.532264f, 87.818597f), vec3(-94.568268f, -13.257042f, 0.000000f)) * vec3(0.120000f, 0.036000f, 0.043200f);
	// California
	light += GalaxyPatch(sky, vec3(0.400011f, 0.698459f, 0.593419f), vec3(-22.168403f, 28.748547f, -18.894025f), vec3(105.898214f, 19.590643f, -94.442123f)) * vec3(0.080000f, 0.024000f, 0.028800f);
	// Pleiades
	light += GalaxyPatch(sky, vec3(0.500423f, 0.763271f, 0.408649f), vec3(-18.339521f, -27.972419f, 74.704823f), vec3(-87.119395f, 57.117978f, 0.000000f)) * vec3(0.088000f, 0.115200f, 0.160000f);
	// North America
	light += GalaxyPatch(sky, vec3(0.503414f, -0.508714f, 0.698415f), vec3(-10.433904f, 33.760096f, 32.110999f), vec3(53.218509f, 31.269786f, -15.583264f)) * vec3(0.180000f, 0.054000f, 0.064800f);
	// Pelican
	light += GalaxyPatch(sky, vec3(0.485399f, -0.525102f, 0.699040f), vec3(-49.431526f, 53.474724f, 74.493158f), vec3(93.496846f, 86.427595f, 0.000000f)) * vec3(0.080000f, 0.024000f, 0.028800f);
	// Gamma Cygni
	light += GalaxyPatch(sky, vec3(0.441799f, -0.621672f, 0.646790f), vec3(6.863340f, 17.241884f, 11.884201f), vec3(32.135916f, 1.406245f, -20.599282f)) * vec3(0.070000f, 0.021000f, 0.025200f);
	// Lagoon
	light += GalaxyPatch(sky, vec3(0.014625f, -0.910710f, -0.412787f), vec3(88.135990f, 1.415324f, 0.000000f), vec3(-1.084992f, 67.565361f, -149.104540f)) * vec3(0.550000f, 0.220000f, 0.275000f);
	// Trifid
	light += GalaxyPatch(sky, vec3(0.009637f, -0.920250f, -0.391213f), vec3(0.938894f, -89.654511f, 210.917256f), vec3(229.170552f, 2.399956f, 0.000000f)) * vec3(0.297500f, 0.175000f, 0.262500f);
	// Eagle
	light += GalaxyPatch(sky, vec3(0.079573f, -0.967869f, -0.238533f), vec3(3.199572f, -38.917153f, 158.976845f), vec3(190.343720f, 15.649102f, 0.000000f)) * vec3(0.140000f, 0.042000f, 0.050400f);
	// Omega
	light += GalaxyPatch(sky, vec3(0.087034f, -0.956341f, -0.278991f), vec3(4.829210f, -53.064083f, 183.402584f), vec3(228.239892f, 20.771457f, 0.000000f)) * vec3(0.180000f, 0.054000f, 0.064800f);
	// Rho Ophiuchi
	light += GalaxyPatch(sky, vec3(-0.359884f, -0.839672f, -0.406737f), vec3(17.090358f, -12.607142f, 10.904646f), vec3(26.370667f, 5.588066f, -34.869047f)) * vec3(0.056000f, 0.062400f, 0.080000f);
	// Antares glow
	light += GalaxyPatch(sky, vec3(-0.344850f, -0.826415f, -0.445104f), vec3(-8.928243f, -21.396079f, 46.642870f), vec3(58.752016f, -24.516280f, 0.000000f)) * vec3(0.080000f, 0.057600f, 0.033600f);
	// Carina
	light += GalaxyPatch(sky, vec3(-0.475381f, 0.161185f, -0.864889f), vec3(-39.108484f, 13.260319f, 23.966999f), vec3(-19.366511f, -57.117395f, 0.000000f)) * vec3(0.600000f, 0.216000f, 0.252000f);
	// Running Chicken
	light += GalaxyPatch(sky, vec3(-0.451666f, 0.045879f, -0.891007f), vec3(-72.556527f, 7.370023f, 37.159628f), vec3(-11.580199f, -114.004931f, 0.000000f)) * vec3(0.120000f, 0.036000f, 0.043200f);
	// Large Magellanic Cloud
	light += GalaxyPatch(sky, vec3(0.054775f, 0.341590f, -0.938252f), vec3(-3.957827f, -11.021325f, -4.243593f), vec3(15.065440f, -5.041958f, -0.956110f)) * vec3(0.473000f, 0.484000f, 0.550000f);
	// Tarantula
	light += GalaxyPatch(sky, vec3(0.033138f, 0.355196f, -0.934204f), vec3(11.049181f, 118.432306f, 45.421293f), vec3(-142.620110f, 13.305790f, 0.000000f)) * vec3(0.450000f, 0.189000f, 0.234000f);
	// Small Magellanic Cloud
	light += GalaxyPatch(sky, vec3(0.287420f, 0.067361f, -0.955433f), vec3(11.851200f, 20.115830f, 4.983383f), vec3(-33.522880f, 21.866317f, -8.542942f)) * vec3(0.326800f, 0.334400f, 0.380000f);
	// Andromeda
	light += GalaxyPatch(sky, vec3(0.738590f, 0.139291f, 0.659608f), vec3(-24.341425f, 17.704653f, 23.517341f), vec3(28.007936f, 111.418247f, -54.890065f)) * vec3(0.160000f, 0.144000f, 0.121600f);
	// Heart
	light += GalaxyPatch(sky, vec3(0.375581f, 0.295553f, 0.878400f), vec3(-49.438880f, -38.904563f, 34.228892f), vec3(-50.617416f, 64.323262f, 0.000000f)) * vec3(0.060000f, 0.018000f, 0.021600f);
	// Soul
	light += GalaxyPatch(sky, vec3(0.362420f, 0.335605f, 0.869495f), vec3(-45.691550f, -42.310840f, 35.375980f), vec3(-64.881865f, 70.066041f, 0.000000f)) * vec3(0.050000f, 0.015000f, 0.018000f);
	// IC 1396
	light += GalaxyPatch(sky, vec3(0.438510f, -0.310483f, 0.843391f), vec3(-26.292020f, 18.615788f, 20.523333f), vec3(23.649145f, 33.400885f, 0.000000f)) * vec3(0.060000f, 0.018000f, 0.021600f);
	// Gum
	light += GalaxyPatch(sky, vec3(-0.450266f, 0.576315f, -0.681998f), vec3(-2.411618f, 0.501626f, 2.016082f), vec3(-1.933721f, -3.281777f, -1.496553f)) * vec3(0.030000f, 0.009000f, 0.010800f);
	// Barnard's Loop: a thin red arc on the east side of Orion
	float loopAngle = acos(clamp(dot(sky, vec3(0.087036f, 0.994829f, -0.052336f)), -1.0f, 1.0f)) * GALAXY_DEGREES;
	float east = dot(sky, vec3(-0.996195f, 0.087156f, 0.0f));
	light += vec3(1.0f, 0.30f, 0.36f) * (0.10f * GalaxyBell(loopAngle - 7.0f, 0.6f) * smoothstep(-0.02f, 0.08f, east));
	return light;
}

// How much of the light from the sky's sphere gets through the air in a direction, for an eye at `eyeKm` from the
// middle of the world's ball: none behind the ball, the air's own loss low down, all of it from over the air
vec3 GalaxyAirLoss(vec3 eyeKm, vec3 dir)
{
	float radius = length(eyeKm);
	// From over the air, the light goes through the air from where the ray goes into it
	float b = dot(eyeKm, dir);
	float disc = b * b - (radius * radius - ATMOS_TOP_RADIUS * ATMOS_TOP_RADIUS);
	bool inAir = radius <= ATMOS_TOP_RADIUS;
	bool missesAir = !inAir && (disc < 0.0f || b > 0.0f);
	vec3 entry = inAir ? eyeKm : eyeKm + dir * (-b - sqrt(max(disc, 0.0f)));
	float entryRadius = length(entry);
	vec3 loss = AtmosTransmittance(min(entryRadius, ATMOS_TOP_RADIUS) - ATMOS_GROUND_RADIUS, dot(dir, entry / entryRadius));
	return missesAir ? vec3_splat(1.0f) : loss;
}

// The light of the Milky Way and the nebulae in the direction `dir`, which is `sky` on the sky's sphere, in front of
// the sky's colour `skyBehind`, for an eye at `eyeKm` from the middle of the world's ball. They show only in a dark sky.
vec3 AirGalaxy(vec3 sky, vec3 dir, vec3 skyBehind, vec3 eyeKm)
{
	const vec3 luma = vec3(0.2126f, 0.7152f, 0.0722f);
	float aboveAir = smoothstep(15.0f, 80.0f, length(eyeKm) - ATMOS_GROUND_RADIUS);
	// Over the air the moon does not light the sky
	float moonSine = dot(normalize(eyeKm), u_planetMoon.xyz);
	float moon = GalaxyMoonVisibility(u_planetMoon.w / GALAXY_MOON_SHARE * (1.0f - aboveAir), moonSine);
	float shown = GalaxyVisibility(dot(skyBehind, luma)) * moon;
	if (shown <= 0.0f)
	{
		return vec3_splat(0.0f);
	}
	vec3 loss = GalaxyAirLoss(eyeKm, dir);
	if (dot(loss, luma) <= 1e-3f)
	{
		return vec3_splat(0.0f);
	}
	vec3 galactic = GalacticDirection(sky);
	vec2 lb = GalaxyLongitudeLatitude(galactic);
	float bulge;
	float band = GalaxyBand(lb, bulge);
	vec3 light = vec3_splat(0.0f);
	if (band > 0.02f)
	{
		// The large star clouds and the clumps of dust
		float clouds = 0.5f * GalaxyNoise(galactic * 9.0f) + 0.25f * GalaxyNoise(galactic * 19.0f) +
		               0.15f * GalaxyNoise(galactic * 41.0f) + 0.10f * GalaxyNoise(galactic * 85.0f);
		float clumps = GalaxyNoise(galactic * 23.0f + 7.3f) * 0.6f + GalaxyNoise(galactic * 57.0f + 3.1f) * 0.4f;
		// The fine grain of far stars too faint to see one by one
		float grain = 0.5f * GalaxyNoise(galactic * 130.0f) + 0.5f * GalaxyNoise(galactic * 270.0f);
		float stars = band * (0.25f + 1.6f * clouds * clouds) * (0.5f + 1.0f * grain * grain);
		float dust = GalaxyDust(lb) * (0.3f + 1.4f * clumps);
		// The bulge is yellow-orange and the disc blue-white. The dust takes more blue than red.
		vec3 colour = mix(vec3(0.78f, 0.86f, 1.0f), vec3(1.0f, 0.8f, 0.58f), bulge / max(band, 1e-4f));
		light = colour * stars * exp(-dust * vec3(0.8f, 1.0f, 1.3f));
	}
	light += GalaxyNebulae(sky) * (0.6f + 0.8f * GalaxyNoise(galactic * 60.0f));
	// Above the air the band is brighter
	return light * loss * (GALAXY_BRIGHTNESS * shown * (1.0f + 0.6f * aboveAir));
}

#endif // GALAXY_SH
