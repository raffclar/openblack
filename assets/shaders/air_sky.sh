#ifndef AIR_SKY_SH
#define AIR_SKY_SH

// EXPERIMENT (not for merge): the sky of the air on the screen, from the sea to space: the air's light, the stars, the
// sun's disc above the air, the glow of the high air and the aurora. The sky's dome and the haze over the scene both
// use it, so the sky has one colour where the dome stops. Include atmosphere.sh first.

#include "world_curve.sh"

// xyz: the direction towards the real sun; w: how strong the noctilucent clouds are tonight, 0 to 1
uniform vec4 u_airSun;
// xyz: the direction of the magnetic pole from the planet's middle; w: how strong the aurora is tonight, 0 to 1
uniform vec4 u_aurora;
// xyz: the planet's middle in world units; w: the seconds of the game, for the aurora's movement
uniform vec4 u_airPlanet;

#define AIR_SUN_RADIUS 0.0047f
#define AIR_AURORA_BOTTOM 92.0f
#define AIR_AURORA_TOP 320.0f
// The steps of the aurora's full march (atmosphere::k_AuroraSteps)
#define AIR_AURORA_STEPS 40
// The noctilucent clouds (atmosphere::k_NoctilucentHeight): their height in kilometres and their brightness on the screen
#define AIR_NLC_HEIGHT 83.0f
#define AIR_NLC_BRIGHTNESS 0.28f

// The aurora's pass at half the screen's size (see AirAuroraOnScreen)
SAMPLER2D(s_airAurora, 12);

float AirHash(vec2 p)
{
	vec3 q = fract(vec3(p.xyx) * 0.1031f);
	q += dot(q, q.yzx + 33.33f);
	return fract((q.x + q.y) * q.z);
}

float AirNoise(float x)
{
	float i = floor(x);
	float f = x - i;
	float a = AirHash(vec2(i, 17.0f));
	float b = AirHash(vec2(i + 1.0f, 17.0f));
	return mix(a, b, f * f * (3.0f - 2.0f * f));
}

// Where a ray is inside a ball round the planet's middle with a radius in kilometres: x in, y out. None when x >= y.
vec2 AirBallSpan(vec3 eyeKm, vec3 dir, float radiusKm)
{
	float b = dot(eyeKm, dir);
	float c = dot(eyeKm, eyeKm) - radiusKm * radiusKm;
	float disc = b * b - c;
	if (disc < 0.0f)
	{
		return vec2(1.0f, -1.0f);
	}
	float s = sqrt(disc);
	return vec2(-b - s, -b + s);
}

// The magnetic latitudes round the middle of the oval that can hold the curtains: the oval moves 3 degrees with the
// time of night, the three curtains are 1.1 degrees apart, and their folds and thickness add less than 1.2 degrees
#define AIR_AURORA_BAND radians(6.0f)
// The samples along a ray that find the part of it near the curtains, before the march
#define AIR_AURORA_PROBES 6

// The light of the aurora along a ray from the eye, up to `farthestKm` kilometres, for the screen's pixel `pixel`, in
// `steps` steps at most. The curtains of the aurora hang in the high air in an oval round the magnetic pole. Their light
// is green from oxygen at about 110 to 200 kilometres, red over 200 kilometres, and purple at their sharp bottom edge
// from nitrogen. The curtains fold slowly, have thin upright rays and flicker fast. Only the night side of the planet
// shows them.
//
// With `probes`, a few probes along the ray first find the part of it that is near the oval's band of latitude and on
// the night side. The march then takes only the steps of the full march that fall in that part. Thus it gives the same
// light for fewer steps, and no steps for a ray far from the oval or on the day side.
vec3 AirAurora(vec3 eye, vec3 dir, float farthestKm, vec2 pixel, int steps, bool probes)
{
	float strength = u_aurora.w;
	if (strength <= 0.01f)
	{
		return vec3_splat(0.0f);
	}
	vec3 eyeKm = (eye - u_airPlanet.xyz) / ATMOS_UNITS_PER_KM;
	vec2 outer = AirBallSpan(eyeKm, dir, ATMOS_GROUND_RADIUS + AIR_AURORA_TOP);
	if (outer.x >= outer.y || outer.y <= 0.0f)
	{
		return vec3_splat(0.0f);
	}
	float start = max(outer.x, 0.0f);
	float end = min(outer.y, farthestKm);
	// From under the curtains, the ray starts where it comes up through their bottom
	vec2 inner = AirBallSpan(eyeKm, dir, ATMOS_GROUND_RADIUS + AIR_AURORA_BOTTOM);
	if (inner.x < inner.y)
	{
		if (inner.x <= 0.0f && inner.y > 0.0f)
		{
			start = max(start, inner.y);
		}
		else if (inner.x > 0.0f)
		{
			end = min(end, inner.x);
		}
	}
	// The planet hides the curtains behind it
	vec2 ground = AirBallSpan(eyeKm, dir, ATMOS_GROUND_RADIUS);
	if (ground.x < ground.y && ground.x > 0.0f)
	{
		end = min(end, ground.x);
	}
	if (end <= start)
	{
		return vec3_splat(0.0f);
	}
	vec3 pole = u_aurora.xyz;
	vec3 sun = u_airSun.xyz;
	// The middle of the oval goes towards the equator on strong nights and on the side away from the sun
	float ovalBase = radians(67.0f - 10.0f * strength);
	float bandLow = sin(ovalBase - AIR_AURORA_BAND);
	float bandHigh = sin(min(ovalBase + AIR_AURORA_BAND, radians(90.0f)));
	// Along the ray the sine of the latitude and the height of the sun each follow a sine wave in the angle round the
	// planet's middle. Between two probes such a curve goes away from the line between them by at most an eighth of the
	// square of the angle between them.
	float probeStride = (end - start) / float(AIR_AURORA_PROBES - 1);
	float angle = probeStride / (ATMOS_GROUND_RADIUS + AIR_AURORA_BOTTOM);
	float bend = 0.125f * angle * angle + 1e-4f;
	float first = probes ? 1e9f : start;
	float last = probes ? -1e9f : end;
	vec2 before = vec2_splat(0.0f);
	if (probes)
	{
		for (int probeIndex = 0; probeIndex < AIR_AURORA_PROBES; ++probeIndex)
		{
			vec3 probeUp = normalize(eyeKm + dir * (start + float(probeIndex) * probeStride));
			vec2 probe = vec2(dot(probeUp, pole), dot(probeUp, sun));
			if (probeIndex > 0)
			{
				bool nearBand = min(before.x, probe.x) - bend <= bandHigh && max(before.x, probe.x) + bend >= bandLow;
				bool dark = min(before.y, probe.y) - bend < 0.05f;
				if (nearBand && dark)
				{
					first = min(first, start + float(probeIndex - 1) * probeStride);
					last = start + float(probeIndex) * probeStride;
				}
			}
			before = probe;
		}
	}
	if (last < first)
	{
		return vec3_splat(0.0f);
	}
	vec3 across = normalize(cross(pole, abs(pole.y) < 0.9f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f)));
	vec3 side = cross(pole, across);
	vec3 sunAcross = normalize(sun - pole * dot(sun, pole) + vec3_splat(1e-5f));
	float time = u_airPlanet.w;
	float stride = (end - start) / float(steps);
	float jitter = AirHash(pixel + fract(time * 7.13f) * 97.0f);
	// The steps of the full march from `start` that fall in the part near the curtains
	int firstStep = int(max(floor((first - start) / stride - jitter), 0.0f));
	int lastStep = int(min(ceil((last - start) / stride), float(steps - 1)));
	vec3 light = vec3_splat(0.0f);
	for (int marchIndex = 0; marchIndex < AIR_AURORA_STEPS; ++marchIndex)
	{
		int i = firstStep + marchIndex;
		if (i > lastStep)
		{
			break;
		}
		vec3 p = eyeKm + dir * (start + (float(i) + jitter) * stride);
		float r = length(p);
		vec3 n = p / r;
		float altitude = r - ATMOS_GROUND_RADIUS;
		if (altitude < AIR_AURORA_BOTTOM - 4.0f)
		{
			continue;
		}
		float dark = smoothstep(0.05f, -0.2f, dot(n, sun));
		if (dark <= 0.0f)
		{
			continue;
		}
		float latitude = asin(clamp(dot(n, pole), -1.0f, 1.0f));
		vec3 level = n - pole * dot(n, pole);
		float longitude = atan2(dot(level, side), dot(level, across));
		float midnight = -dot(normalize(level + vec3_splat(1e-5f)), sunAcross);
		float oval = ovalBase - radians(3.0f) * midnight;
		// Three curtains, each folded slowly along its length
		float curtains = 0.0f;
		for (int c = 0; c < 3; ++c)
		{
			float fc = float(c);
			float fold = radians(0.5f) * sin(longitude * 9.0f + fc * 1.7f + time * 0.02f) +
			             radians(0.15f) * sin(longitude * 41.0f - fc * 2.3f + time * 0.07f) +
			             radians(0.05f) * sin(longitude * 133.0f + fc + time * 0.25f);
			float centre = oval + radians(1.1f) * (fc - 1.0f) + fold;
			float thickness = radians(0.07f + 0.05f * AirNoise(longitude * 60.0f + fc * 11.0f));
			float d = (latitude - centre) / thickness;
			curtains += exp(-d * d) * (0.55f + 0.45f * AirNoise(longitude * 25.0f + fc * 5.0f + time * 0.03f));
		}
		if (curtains < 0.002f)
		{
			continue;
		}
		// The thin upright rays, and their fast flicker
		float rays = 0.45f + 0.55f * AirNoise(longitude * 2400.0f + time * 0.4f);
		rays *= 0.85f + 0.15f * sin(time * 11.0f + longitude * 700.0f);
		// The colours by height: green oxygen from a sharp bottom, red oxygen high up, purple nitrogen at the edge
		float green = smoothstep(AIR_AURORA_BOTTOM, AIR_AURORA_BOTTOM + 12.0f, altitude) * exp(-max(altitude - 110.0f, 0.0f) / 45.0f);
		float red = smoothstep(160.0f, 230.0f, altitude) * (1.0f - smoothstep(270.0f, AIR_AURORA_TOP, altitude)) * 0.18f;
		float edge = (altitude - (AIR_AURORA_BOTTOM + 3.0f)) / 4.0f;
		float purple = exp(-edge * edge) * 0.45f;
		vec3 colour = green * vec3(0.22f, 1.0f, 0.38f) + red * vec3(1.0f, 0.1f, 0.14f) + purple * vec3(0.75f, 0.22f, 0.95f);
		light += colour * (curtains * rays * dark * stride);
	}
	// From under the air, the air dims the aurora on its way to the eye
	float eyeAltitude = length(eyeKm) - ATMOS_GROUND_RADIUS;
	vec3 through = eyeAltitude < 100.0f ? AtmosTransmittance(eyeAltitude, dot(normalize(eyeKm), dir)) : vec3_splat(1.0f);
	return light * through * (0.02f * (0.5f + strength));
}

// The aurora that the aurora's pass drew at half the screen's size, at a point of the screen (0 to 1)
vec3 AirAuroraOnScreen(vec2 uv)
{
	return texture2DLod(s_airAurora, uv, 0.0f).rgb;
}

// Whether the scene can have the aurora in front of it: only when the eye is over the bottom of the curtains, as from
// space. From under them, a ray to the scene never comes up to the curtains.
bool AirAuroraOverScene()
{
	return u_atmosParams.z > AIR_AURORA_BOTTOM - 4.0f;
}

float AirNoise2(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = p - i;
	vec2 u = f * f * (3.0f - 2.0f * f);
	float a = AirHash(i);
	float b = AirHash(i + vec2(1.0f, 0.0f));
	float c = AirHash(i + vec2(0.0f, 1.0f));
	float d = AirHash(i + vec2(1.0f, 1.0f));
	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// The noctilucent clouds: a thin sheet of ice at the top of the mesosphere. The sheet is one surface, so each pixel
// reads it once where its ray goes through. The sun under the horizon lights the sheet through the air at the limb,
// while the low air is in the earth's shadow. Thus the sheet shows in deep twilight, low in the sky on the side of the
// sun, as silver-blue bands and billows that drift slowly. From space it is a thin bright line over the limb at the
// line between day and night.
vec3 AirNoctilucent(vec3 eye, vec3 dir)
{
	float strength = u_airSun.w;
	if (strength <= 0.01f)
	{
		return vec3_splat(0.0f);
	}
	vec3 eyeKm = (eye - u_airPlanet.xyz) / ATMOS_UNITS_PER_KM;
	float eyeAltitude = length(eyeKm) - ATMOS_GROUND_RADIUS;
	vec2 sheet = AirBallSpan(eyeKm, dir, ATMOS_GROUND_RADIUS + AIR_NLC_HEIGHT);
	if (sheet.x >= sheet.y || sheet.y <= 0.0f)
	{
		return vec3_splat(0.0f);
	}
	// Under the sheet the ray meets it where it goes out of the ball, and over the sheet where it comes in
	float t = sheet.x > 0.0f ? sheet.x : sheet.y;
	vec2 ground = AirBallSpan(eyeKm, dir, ATMOS_GROUND_RADIUS);
	if (ground.x < ground.y && ground.x > 0.0f && ground.x < t)
	{
		return vec3_splat(0.0f);
	}
	vec3 p = eyeKm + dir * t;
	vec3 n = normalize(p);
	vec3 sun = u_airSun.xyz;
	// The sun's light that reaches the sheet through the air. It is none in the earth's shadow.
	vec3 lit = AtmosTransmittance(AIR_NLC_HEIGHT, dot(n, sun));
	if (max(lit.r, max(lit.g, lit.b)) < 1e-4f)
	{
		return vec3_splat(0.0f);
	}
	// The sky behind the sheet must be dark: the air over the eye from the ground, and the air under the sheet from space
	vec3 place = normalize(mix(normalize(eyeKm), n, smoothstep(50.0f, 90.0f, eyeAltitude)));
	float dark = 1.0f - smoothstep(-0.139f, -0.105f, dot(place, sun));
	if (dark <= 0.0f)
	{
		return vec3_splat(0.0f);
	}
	// The pattern on the sheet in kilometres: wide patches, long bands 20 to 40 km apart, short waves across them and
	// small billows. The wind of the mesosphere moves them all slowly.
	float time = u_airPlanet.w;
	vec2 q = p.xz + vec2(0.045f, 0.012f) * time;
	// A strong night covers more of the sky
	float patches = smoothstep(0.62f - 0.4f * strength, 0.9f - 0.35f * strength,
	                           0.6f * AirNoise2(q / 380.0f) + 0.4f * AirNoise2(q / 140.0f + 7.3f));
	if (patches <= 0.0f)
	{
		return vec3_splat(0.0f);
	}
	// The size of a pixel on the sheet. The fine parts go to their mean when they are smaller than a pixel, so they do
	// not shimmer far away.
	float along = min(1.0f / max(abs(dot(dir, n)), 1e-3f), 40.0f);
	float pixel = t * 0.0012f * along;
	// The sheet's own frame: x along the bands and y across them. The bands bend slowly across the sheet.
	q += 45.0f * vec2(AirNoise2(q / 220.0f), AirNoise2(q / 220.0f + 5.7f));
	vec2 b = vec2(dot(q, vec2(0.8f, 0.6f)), dot(q, vec2(-0.6f, 0.8f)));
	// Long thin streaks along the bands, in groups 20 to 40 km apart
	float wide = 0.7f * AirNoise2(vec2(b.x / 90.0f, b.y / 12.0f)) + 0.3f * AirNoise2(q / 40.0f + 9.2f);
	float thin = AirNoise2(vec2(b.x / 40.0f, b.y / 4.0f) + 3.1f);
	float streaks = mix(wide, 0.75f * wide + 0.25f * thin, 1.0f - smoothstep(1.0f, 3.0f, pixel));
	float groups = 0.5f + 0.5f * sin(b.y * (6.2831853f / 28.0f) + 3.0f * AirNoise2(q / 90.0f));
	// Short waves across the bands, a few kilometres long, and small billows
	float waves = 0.5f + 0.5f * sin(b.x * (6.2831853f / 7.0f) + 4.0f * AirNoise2(q / 12.0f));
	float fine = 1.0f - smoothstep(1.0f, 4.0f, pixel);
	float billows = mix(0.5f, 0.6f * waves + 0.4f * AirNoise2(q / 2.0f), fine);
	float density = patches * pow(max(streaks, 0.0f), 1.3f) * (0.7f + 0.3f * groups) * (0.85f + 0.3f * billows);
	// The ray goes through more of the thin sheet when it goes along it. The small ice scatters more forward.
	float forward = 0.6f + 1.4f * pow(max(dot(dir, sun), 0.0f), 4.0f);
	// From under the sheet the air between dims the light on its way to the eye, most near the horizon
	// From over the sheet the dark ground is behind it, and it shows less bright than against the sky.
	vec3 toEye = eyeAltitude < AIR_NLC_HEIGHT ? AtmosTransmittance(eyeAltitude, dot(normalize(eyeKm), dir)) : vec3_splat(0.6f);
	// The ice is silver-blue. The light at the limb keeps only some of its own colour.
	vec3 tint = mix(vec3_splat(dot(lit, vec3(0.2126f, 0.7152f, 0.0722f))), lit, 0.05f) * vec3(0.7f, 0.86f, 1.0f);
	vec3 light = tint * toEye * (density * min(along, 15.0f) * forward * dark * strength * AIR_NLC_BRIGHTNESS);
	// The brightest parts ease towards white and do not go past it
	return vec3_splat(1.0f) - exp(-light);
}

// The faint glow of the high air at about 95 kilometres: a thin green line along the limb, seen from above the air
vec3 AirGlow(vec3 eye, vec3 dir)
{
	vec3 eyeKm = (eye - u_airPlanet.xyz) / ATMOS_UNITS_PER_KM;
	float eyeAltitude = length(eyeKm) - ATMOS_GROUND_RADIUS;
	float seen = smoothstep(60.0f, 140.0f, eyeAltitude);
	if (seen <= 0.0f)
	{
		return vec3_splat(0.0f);
	}
	vec2 outer = AirBallSpan(eyeKm, dir, ATMOS_GROUND_RADIUS + 100.0f);
	vec2 inner = AirBallSpan(eyeKm, dir, ATMOS_GROUND_RADIUS + 90.0f);
	vec2 ground = AirBallSpan(eyeKm, dir, ATMOS_GROUND_RADIUS);
	float end = ground.x < ground.y && ground.x > 0.0f ? ground.x : outer.y;
	float path = max(min(outer.y, end) - max(outer.x, 0.0f), 0.0f);
	if (inner.x < inner.y)
	{
		path -= max(min(inner.y, end) - max(inner.x, 0.0f), 0.0f);
	}
	// Only the night side glows so that it shows
	vec3 n = normalize(eyeKm + dir * max(outer.x, 0.0f));
	float dark = smoothstep(0.1f, -0.15f, dot(n, u_airSun.xyz));
	return vec3(0.12f, 0.45f, 0.18f) * (max(path, 0.0f) * 0.00012f * dark * seen);
}

// The sky in a direction from the eye on the screen, with `sunHeight` the sine of the air's sun over the horizon at the
// land, for the screen's pixel `pixel`. The aurora is not in it: the caller adds the aurora's pass or AirAurora.
vec3 AirSkyColour(vec3 eye, vec3 dir, float sunHeight, vec2 pixel)
{
	vec3 air = AtmosSkyOnScreen(dir, sunHeight);
	const vec3 luma = vec3(0.2126f, 0.7152f, 0.0722f);
	float altitude = u_atmosParams.z;
	// Where the air is dark, small stars show. Above the air they are brighter, and they show to the limb.
	float dark = 1.0f - smoothstep(0.02f, 0.12f, dot(air, luma));
	vec3 cell = floor(dir * 260.0f);
	vec3 q = fract(cell * vec3(0.1031f, 0.1030f, 0.0973f));
	q += dot(q, q.yzx + 33.33f);
	vec3 h = fract((q.xxy + q.yzz) * q.zyx);
	vec3 star = cell + 0.2f + 0.6f * h - dir * 260.0f;
	float glint = h.x > 0.985f ? smoothstep(0.06f, 0.0f, dot(star, star)) * (0.4f + 0.6f * h.y) : 0.0f;
	// The stars show over the limb of the world's ball, in the air's own frame
	float ballRadius = ATMOS_GROUND_RADIUS + max(altitude, 0.0f);
	float horizonUp = -sqrt(max(altitude, 0.0f) * (2.0f * ATMOS_GROUND_RADIUS + max(altitude, 0.0f))) / ballRadius;
	float aboveAir = smoothstep(15.0f, 80.0f, altitude);
	air += vec3(0.85f, 0.9f, 1.0f) * glint * dark * smoothstep(horizonUp + 0.02f, horizonUp + 0.2f, dot(dir, AtmosUp())) *
	       (1.0f + 1.5f * aboveAir);
	// Above the air the sun is a hard white disc
	float sunCos = dot(dir, normalize(u_airSun.xyz));
	if (aboveAir > 0.0f && sunCos > cos(AIR_SUN_RADIUS * 1.5f))
	{
		float disc = smoothstep(cos(AIR_SUN_RADIUS * 1.05f), cos(AIR_SUN_RADIUS * 0.95f), sunCos);
		air = mix(air, vec3(1.0f, 0.98f, 0.94f), disc * aboveAir);
	}
	air += AirGlow(eye, dir);
	air += AirNoctilucent(eye, dir);
	// A little noise of about one step of the screen's colours, so the dim sky at dusk shows no bands
	air += vec3_splat((AirHash(pixel) + AirHash(pixel + 31.7f) - 1.0f) / 255.0f);
	return max(air, vec3_splat(0.0f));
}

#endif // AIR_SKY_SH
