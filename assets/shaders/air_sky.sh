#ifndef AIR_SKY_SH
#define AIR_SKY_SH

// EXPERIMENT (not for merge): the sky of the air on the screen, from the sea to space: the air's light, the stars, the
// sun's disc above the air, the glow of the high air and the aurora. The sky's dome and the haze over the scene both
// use it, so the sky has one colour where the dome stops. Include atmosphere.sh first.

#include "world_curve.sh"

// xyz: the direction towards the real sun; w: unused
uniform vec4 u_airSun;
// xyz: the direction of the magnetic pole from the planet's middle; w: how strong the aurora is tonight, 0 to 1
uniform vec4 u_aurora;
// xyz: the planet's middle in world units; w: the seconds of the game, for the aurora's movement
uniform vec4 u_airPlanet;

#define AIR_SUN_RADIUS 0.0047f
#define AIR_AURORA_BOTTOM 92.0f
#define AIR_AURORA_TOP 320.0f
#define AIR_AURORA_STEPS 40

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

// The light of the aurora along a ray from the eye, up to `farthestKm` kilometres, for the screen's pixel `pixel`. The curtains of the aurora hang in
// the high air in an oval round the magnetic pole. Their light is green from oxygen at about 110 to 200 kilometres, red
// over 200 kilometres, and purple at their sharp bottom edge from nitrogen. The curtains fold slowly, have thin upright
// rays and flicker fast. Only the night side of the planet shows them.
vec3 AirAurora(vec3 eye, vec3 dir, float farthestKm, vec2 pixel)
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
	vec3 across = normalize(cross(pole, abs(pole.y) < 0.9f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f)));
	vec3 side = cross(pole, across);
	vec3 sun = u_airSun.xyz;
	vec3 sunAcross = normalize(sun - pole * dot(sun, pole) + vec3_splat(1e-5f));
	float time = u_airPlanet.w;
	float stride = (end - start) / float(AIR_AURORA_STEPS);
	float jitter = AirHash(pixel + fract(time * 7.13f) * 97.0f);
	// The middle of the oval goes towards the equator on strong nights and on the side away from the sun
	float ovalBase = radians(67.0f - 10.0f * strength);
	vec3 light = vec3_splat(0.0f);
	for (int i = 0; i < AIR_AURORA_STEPS; ++i)
	{
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
		for (int k = 0; k < 3; ++k)
		{
			float fk = float(k);
			float fold = radians(0.5f) * sin(longitude * 9.0f + fk * 1.7f + time * 0.02f) +
			             radians(0.15f) * sin(longitude * 41.0f - fk * 2.3f + time * 0.07f) +
			             radians(0.05f) * sin(longitude * 133.0f + fk + time * 0.25f);
			float centre = oval + radians(1.1f) * (fk - 1.0f) + fold;
			float thickness = radians(0.07f + 0.05f * AirNoise(longitude * 60.0f + fk * 11.0f));
			float d = (latitude - centre) / thickness;
			curtains += exp(-d * d) * (0.55f + 0.45f * AirNoise(longitude * 25.0f + fk * 5.0f + time * 0.03f));
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
// land, for the screen's pixel `pixel`
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
	float horizonY = WorldHorizonY(altitude * ATMOS_UNITS_PER_KM);
	float aboveAir = smoothstep(15.0f, 80.0f, altitude);
	air += vec3(0.85f, 0.9f, 1.0f) * glint * dark * smoothstep(horizonY + 0.02f, horizonY + 0.2f, dir.y) * (1.0f + 1.5f * aboveAir);
	// Above the air the sun is a hard white disc
	float sunCos = dot(dir, normalize(u_airSun.xyz));
	if (aboveAir > 0.0f && sunCos > cos(AIR_SUN_RADIUS * 1.5f))
	{
		float disc = smoothstep(cos(AIR_SUN_RADIUS * 1.05f), cos(AIR_SUN_RADIUS * 0.95f), sunCos);
		air = mix(air, vec3(1.0f, 0.98f, 0.94f), disc * aboveAir);
	}
	air += AirGlow(eye, dir);
	air += AirAurora(eye, dir, 1e9f, pixel);
	return air;
}

#endif // AIR_SKY_SH
