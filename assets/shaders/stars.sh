#ifndef STARS_SH
#define STARS_SH

// EXPERIMENT (not for merge): the stars of the night sky (graphics::stars uses the same rules, and its tests check them).
// - The bright stars of the catalogue are at their true places on the sky's sphere (celestial.sh).
// - Three layers of fainter stars come from a hash, with no grid and no repeat.
// - Each star is a small spot at its exact place, to less than a pixel, so it moves smoothly.
// - The light of the sky behind a star sets the faintest star that shows. The air near the horizon dims the stars,
//   makes them redder and makes them twinkle. Above the air they do not twinkle, and more of them show.
// Include atmosphere.sh first.

#include "celestial.sh"

// The texture of the stars (stars::BuildTexture): the colour table, the index of the cells and the stars' records
SAMPLER2D(s_stars, 9);
// x: 1 when the texture of the stars is there; y: the rows of the texture; z: the moon's light for the stars, 0 to 1
// (stars::MoonLight)
uniform vec4 u_stars;
// The moon's disc: xyz the direction towards it, w the cosine of its radius, over 1.5 when there is no moon
uniform vec4 u_starsMoon;

// The layout of the texture (stars::k_TextureWidth, k_CellsAcross, k_IndexRows)
#define STARS_WIDTH 256.0f
#define STARS_CELLS 32.0f
#define STARS_INDEX_FIRST 256.0f
#define STARS_RECORD_FIRST 6400.0f
// The most records that the shader reads in one cell
#define STARS_MOST_IN_CELL 24
// The packed records (stars::PackRecord)
#define STARS_LOWEST_MAGNITUDE -2.0f
#define STARS_COLOUR_STRIDE 2048.0f
// The faintest star and the sky (stars::LimitingMagnitude)
#define STARS_DARK_LIMIT 7.5f
#define STARS_SPACE_EXTRA 1.5f
#define STARS_MOON_DROP 2.2f
#define STARS_DARK_SKY 0.012f
#define STARS_DROP_SCALE 2.0f
// The light of a star (stars::StarLight)
#define STARS_BRIGHTNESS 5.5f
#define STARS_EYE_RESPONSE 0.55f
#define STARS_FADE 1.5f
// The twinkle (stars::TwinkleStrength)
#define STARS_TWINKLE_MOST 0.55f
#define STARS_TWINKLE_TOP 0.42f
// The faint stars (stars::k_CountSlope)
#define STARS_COUNT_SLOPE 0.45f
// The angle within which a star goes into the cells round it (stars::k_CellMargin)
#define STARS_CELL_MARGIN 0.004f

// How many more faint stars show in a direction of the sky's sphere: 1 everywhere for now. The band of the Milky Way can
// give more here.
float StarDensityGain(vec3 celestialDir)
{
	return 1.0f;
}

// The texel of the texture of the stars at a place in its texels, row after row
vec4 StarTexel(float index)
{
	float row = floor(index / STARS_WIDTH);
	vec2 uv = vec2(index - row * STARS_WIDTH + 0.5f, row + 0.5f) / vec2(STARS_WIDTH, u_stars.y);
	return texture2DLod(s_stars, uv, 0.0f);
}

// The face of the cube in a direction and the point on it, -1 to 1 across and up (stars::FacePointOf)
vec3 StarFacePoint(vec3 d)
{
	vec3 a = abs(d);
	if (a.x >= a.y && a.x >= a.z)
	{
		return vec3(d.x > 0.0f ? 0.0f : 1.0f, d.y / a.x, d.z / a.x);
	}
	if (a.y >= a.z)
	{
		return vec3(d.y > 0.0f ? 2.0f : 3.0f, d.z / a.y, d.x / a.y);
	}
	return vec3(d.z > 0.0f ? 4.0f : 5.0f, d.x / a.z, d.y / a.z);
}

// The direction of a point of a face (the opposite of StarFacePoint), not normalised
vec3 StarFaceDirection(float face, vec2 p)
{
	float side = mod(face, 2.0f) < 0.5f ? 1.0f : -1.0f;
	if (face < 1.5f)
	{
		return vec3(side, p.x, p.y);
	}
	if (face < 3.5f)
	{
		return vec3(p.y, side, p.x);
	}
	return vec3(p.x, p.y, side);
}

// The warp of a face that gives the cells almost equal areas (stars::FaceWarp), and its opposite
vec2 StarWarp(vec2 p)
{
	return vec2(atan(p.x), atan(p.y)) * (4.0f / 3.14159265f);
}

vec2 StarUnwarp(vec2 w)
{
	return vec2(tan(w.x * 0.78539816f), tan(w.y * 0.78539816f));
}

// The magnitudes that the twilight takes from the faintest star that shows (stars::TwilightDrop)
float StarTwilightDrop(float sunSine)
{
	float degrees = asin(clamp(sunSine, -1.0f, 1.0f)) * 57.29578f;
	if (degrees < -15.0f)
	{
		return mix(0.0f, 0.8f, clamp((degrees + 18.0f) / 3.0f, 0.0f, 1.0f));
	}
	if (degrees < -12.0f)
	{
		return mix(0.8f, 2.0f, (degrees + 15.0f) / 3.0f);
	}
	if (degrees < -9.0f)
	{
		return mix(2.0f, 3.2f, (degrees + 12.0f) / 3.0f);
	}
	if (degrees < -6.0f)
	{
		return mix(3.2f, 4.8f, (degrees + 9.0f) / 3.0f);
	}
	if (degrees < -3.0f)
	{
		return mix(4.8f, 6.5f, (degrees + 6.0f) / 3.0f);
	}
	if (degrees < 0.0f)
	{
		return mix(6.5f, 8.5f, (degrees + 3.0f) / 3.0f);
	}
	return mix(8.5f, 12.0f, clamp(degrees / 4.0f, 0.0f, 1.0f));
}

// The faintest star that shows against a sky (stars::LimitingMagnitude). The twilight, the moon and the bright parts of
// the sky on the screen add their light.
float StarLimit(float sunSine, float moonLight, float skyLuminance, float aboveAir)
{
	float inAir = 1.0f - aboveAir;
	float twilight = exp2(inAir * StarTwilightDrop(sunSine) / STARS_DROP_SCALE * 3.3219281f) - 1.0f;
	float moon = exp2(inAir * STARS_MOON_DROP * clamp(moonLight, 0.0f, 1.0f) / STARS_DROP_SCALE * 3.3219281f) - 1.0f;
	float screen = max(skyLuminance / STARS_DARK_SKY - 1.0f, 0.0f);
	return STARS_DARK_LIMIT + aboveAir * STARS_SPACE_EXTRA - STARS_DROP_SCALE * log2(1.0f + twilight + moon + screen) * 0.30103f;
}

// The light of a star on the screen, all its pixels together (stars::StarLight)
float StarLight(float magnitude, float limit)
{
	return STARS_BRIGHTNESS * exp2(-0.4f * STARS_EYE_RESPONSE * 3.3219281f * magnitude) * smoothstep(0.0f, STARS_FADE, limit - magnitude);
}

// The colour of the table's entry for a star with a light. The eye sees little colour in a faint light, so the faint
// stars are nearly white and the bright stars show their full colour.
vec3 StarColour(float entry, float light)
{
	return mix(vec3_splat(1.0f), StarTexel(entry).rgb, 0.25f + 0.75f * smoothstep(0.05f, 1.5f, light));
}

// The spot of a star on the screen: its light spread round its place, with all its pixels together giving `light`. The
// spot of a bright star is a little wider. `pixels` is the distance from the star in pixels.
float StarSpot(float pixels, float light, float widest)
{
	float width = min(0.7f + 0.4f * smoothstep(0.3f, 4.0f, light), widest);
	return light * exp(-0.5f * pixels * pixels / (width * width)) / (6.2831853f * width * width);
}

// A slow random change from -1 to 1 over time for a star with a seed, for the twinkle
float StarFlicker(float seed, float time)
{
	return 0.5f * sin(time * 23.0f + seed * 6.2831853f) + 0.3f * sin(time * 37.0f + seed * 17.3f) +
	       0.2f * sin(time * 61.0f + seed * 31.7f);
}

// The hash of a cell of a layer: four numbers from 0 to 1
vec4 StarHash(vec3 p)
{
	vec4 q = fract(vec4(p.xyzx) * vec4(0.1031f, 0.1030f, 0.0973f, 0.1099f));
	q += dot(q, q.wzxy + 33.33f);
	return fract((q.xxyz + q.yzzw) * q.zywx);
}

// The magnitude of a faint star from a hash (stars::FaintMagnitude)
float StarFaintMagnitude(float u, float brightest, float faintest)
{
	float a = STARS_COUNT_SLOPE * 2.3025851f;
	return brightest + log(1.0f + u * (exp(a * (faintest - brightest)) - 1.0f)) / a;
}

// The colour entry of a faint star from a hash (stars::FaintTemperature and stars::ColourEntry). The table's entries go
// in equal steps of the inverse of the temperature from 22000 K to 3000 K.
float StarFaintEntry(float u)
{
	// The inverse temperatures (1e6 / K) at the shares 0, 0.08, 0.30, 0.48, 0.63, 0.94 and 1
	float mired;
	if (u < 0.08f)
	{
		mired = mix(45.4545f, 100.0f, u / 0.08f);
	}
	else if (u < 0.30f)
	{
		mired = mix(100.0f, 133.333f, (u - 0.08f) / 0.22f);
	}
	else if (u < 0.48f)
	{
		mired = mix(133.333f, 166.667f, (u - 0.30f) / 0.18f);
	}
	else if (u < 0.63f)
	{
		mired = mix(166.667f, 192.308f, (u - 0.48f) / 0.15f);
	}
	else if (u < 0.94f)
	{
		mired = mix(192.308f, 256.410f, (u - 0.63f) / 0.31f);
	}
	else
	{
		mired = mix(256.410f, 333.333f, (u - 0.94f) / 0.06f);
	}
	return floor((mired - 45.4545f) / (333.333f - 45.4545f) * 255.0f + 0.5f);
}

// One layer of faint stars: `cellsAcross` cells on each face of a turned cube, a share of them with a star
vec3 StarFaintLayer(vec3 c, mat3 turn, float layer, float cellsAcross, float share, float brightest, float faintest,
                    float limit, float pixelAngle, float widest, float time, float twinkle)
{
	vec3 p = mul(turn, c);
	vec3 facePoint = StarFacePoint(p);
	vec2 g = (StarWarp(facePoint.yz) + 1.0f) * 0.5f * cellsAcross;
	// The four cells nearest the point hold every star that is nearer than half a cell
	vec2 base = floor(g - 0.5f);
	vec3 light = vec3_splat(0.0f);
	for (int i = 0; i < 4; ++i)
	{
		vec2 cell = base + vec2(mod(float(i), 2.0f), floor(float(i) * 0.5f));
		if (cell.x < 0.0f || cell.y < 0.0f || cell.x >= cellsAcross || cell.y >= cellsAcross)
		{
			continue;
		}
		vec4 h = StarHash(vec3(cell, facePoint.x * 17.0f + layer * 131.0f));
		if (h.x > share)
		{
			continue;
		}
		vec4 h2 = StarHash(vec3(cell.yx + 71.3f, facePoint.x * 29.0f + layer * 53.0f));
		float magnitude = StarFaintMagnitude(h2.x, brightest, faintest);
		float shown = StarLight(magnitude, limit);
		if (shown <= 0.0f)
		{
			continue;
		}
		vec3 star = normalize(StarFaceDirection(facePoint.x, StarUnwarp((cell + h.yz) / cellsAcross * 2.0f - 1.0f)));
		float pixels = length(cross(star, p)) / pixelAngle;
		float spot = StarSpot(pixels, shown * (1.0f + twinkle * StarFlicker(h.w, time)), widest);
		light += StarColour(StarFaintEntry(h2.y), shown) * spot;
	}
	return light;
}

// The stars in a direction from the eye (`dir` in the frame of the camera), over a sky of colour `skyBehind`:
// - `sunSine` is the sine of the sun's height over the horizon at the land;
// - `altitude` is the eye's height over the ground in kilometres, and `aboveAir` is 0 in the air and 1 above it;
// - `up` is the air's up at the eye, and `horizonUp` is the cosine of the horizon's angle from up, under 0 high up;
// - `time` is the seconds of the game, for the twinkle.
vec3 AirStars(vec3 dir, vec3 skyBehind, float sunSine, float altitude, float aboveAir, vec3 up, float horizonUp, float time)
{
	const vec3 luma = vec3(0.2126f, 0.7152f, 0.0722f);
	float limit = StarLimit(sunSine, u_stars.z, dot(skyBehind, luma), aboveAir);
	// No star is brighter than Sirius
	if (u_stars.x < 0.5f || limit < -1.5f)
	{
		return vec3_splat(0.0f);
	}
	float mu = dot(dir, up);
	// The stars set behind the ball of the world
	float overLimb = smoothstep(horizonUp, horizonUp + 0.004f, mu);
	// The moon hides the stars behind its disc
	float behindMoon = u_starsMoon.w < 1.5f ? smoothstep(u_starsMoon.w - 2e-5f, u_starsMoon.w + 2e-5f, dot(dir, u_starsMoon.xyz)) : 0.0f;
	if (overLimb <= 0.0f || behindMoon >= 1.0f)
	{
		return vec3_splat(0.0f);
	}
	// The air near the horizon takes more of each colour and most of the blue, so the low stars are dim and red. A star
	// that the air dims to the faintest that shows does not show.
	vec3 through = mix(AtmosTransmittance(altitude, mu), vec3_splat(1.0f), aboveAir);
	float throughLuma = dot(through, luma);
	if (throughLuma < 1e-4f)
	{
		return vec3_splat(0.0f);
	}
	limit += 2.5f * log2(throughLuma) * 0.30103f;
	vec3 tint = through / throughLuma;
	float twinkle = STARS_TWINKLE_MOST * (1.0f - aboveAir) * pow(1.0f - smoothstep(0.0f, STARS_TWINKLE_TOP, mu), 2.0f);
	// The angle of a pixel on the screen; a spot does not grow past the margin round the cells
	float pixelAngle = 2.0f / (abs(u_proj[1][1]) * u_viewRect.w);
	float widest = STARS_CELL_MARGIN / (3.0f * pixelAngle);

	vec3 c = CelestialDirection(dir);
	vec3 light = vec3_splat(0.0f);

	// The stars of the catalogue in the cell of the direction, from the brightest
	vec3 facePoint = StarFacePoint(c);
	vec2 cellXY = clamp(floor((StarWarp(facePoint.yz) + 1.0f) * 0.5f * STARS_CELLS), 0.0f, STARS_CELLS - 1.0f);
	vec4 cell = StarTexel(STARS_INDEX_FIRST + facePoint.x * STARS_CELLS * STARS_CELLS + cellXY.y * STARS_CELLS + cellXY.x);
	if (cell.z < limit)
	{
		for (int i = 0; i < STARS_MOST_IN_CELL; ++i)
		{
			if (float(i) >= cell.y)
			{
				break;
			}
			vec4 record = StarTexel(STARS_RECORD_FIRST + cell.x + float(i));
			float entry = floor(record.w / STARS_COLOUR_STRIDE);
			float magnitude = (record.w - entry * STARS_COLOUR_STRIDE) / 100.0f + STARS_LOWEST_MAGNITUDE;
			if (magnitude >= limit)
			{
				break;
			}
			float pixels = length(cross(record.xyz, c)) / pixelAngle;
			if (pixels > 6.0f || dot(record.xyz, c) < 0.0f)
			{
				continue;
			}
			float seed = fract(record.w * 0.001f + record.x * 517.3f);
			float flicker = StarFlicker(seed, time);
			// Low down the colours twinkle a little apart
			vec3 apart = twinkle * vec3(flicker, StarFlicker(seed + 0.31f, time), StarFlicker(seed + 0.67f, time));
			float shown = StarLight(magnitude, limit);
			float spot = StarSpot(pixels, shown, widest);
			light += StarColour(entry, shown) * spot * max(1.0f + mix(vec3_splat(twinkle * flicker), apart, 0.4f), 0.0f);
		}
	}

	// The layers of faint stars, each on a cube turned its own way so that their faces do not line up
	float gain = StarDensityGain(c);
	if (limit > 6.5f)
	{
		mat3 turnA = mat3(0.78164f, -0.48293f, 0.39474f, 0.55012f, 0.83203f, -0.07139f, -0.29396f, 0.27296f, 0.91602f);
		light += StarFaintLayer(c, turnA, 0.0f, 72.0f, min(0.6f * gain, 1.0f), 6.5f, 7.5f, limit, pixelAngle, widest, time, twinkle);
	}
	if (limit > 7.5f)
	{
		mat3 turnB = mat3(0.68493f, -0.71061f, 0.16094f, -0.29761f, -0.07123f, 0.95203f, -0.66505f, -0.69997f, -0.26028f);
		light += StarFaintLayer(c, turnB, 1.0f, 112.0f, min(0.6f * gain, 1.0f), 7.5f, 8.5f, limit, pixelAngle, widest, time, twinkle);
	}
	if (limit > 8.5f)
	{
		mat3 turnC = mat3(-0.82406f, -0.56643f, -0.00961f, 0.34754f, -0.49208f, -0.79817f, 0.44738f, -0.66107f, 0.60236f);
		light += StarFaintLayer(c, turnC, 2.0f, 175.0f, min(0.6f * gain, 1.0f), 8.5f, 9.5f, limit, pixelAngle, widest, time, twinkle);
	}
	return light * tint * (overLimb * (1.0f - behindMoon));
}

#endif // STARS_SH
