#ifndef UNDERWATER_FOAM_SHADE_SH
#define UNDERWATER_FOAM_SHADE_SH

// EXPERIMENT (not for merge): how the post pass draws the foam with a thickness. The foam field (see
// underwater_foam.sh) gives how thick the foam is at each point. The foam stands on the water as high as it is thick,
// broken into lumps that the current carries. The light goes through its thin edges, its thick parts are in shadow,
// and the bubbles show in it near the eye. From under the water, its underside shows where it covers the surface.

#include "underwater_foam.sh"

SAMPLER2D(s_foam, 9);

// The highest that the foam stands on the water, for a thickness of 1, in units (underwater::k_FoamFullHeight)
#define UW_FOAM_TALLEST 0.45f
// How much of its height the foam of the shore keeps (underwater::k_ShoreFoamHeightScale): change them together
#define UW_SHORE_FOAM_HEIGHT_SCALE 0.6666667f

// The foam field at a point: x how thick the foam is, y how old, z how much air it holds, w how much of the foam is
// shore foam (0 to 1)
vec4 FoamFieldAt(vec2 xz)
{
	vec2 uv = UwFoamUv(xz);
	float inside = UwFoamInside(uv);
	if (inside <= 0.0f)
	{
		return vec4_splat(0.0f);
	}
	vec4 field = texture2DLod(s_foam, uv, 0.0f);
	return vec4(field.x * inside, field.y, field.z * inside, field.w * inside);
}

// The lace of the foam, 0 to 1.5: lumps with gaps between them, from the noise `lumps`. More of the lace closes where
// the foam is thick. Where a pixel is large on the water, the lace is softer, so that it does not shimmer.
float FoamLace(float field, float lumps, float pixel)
{
	float soft = clamp(pixel * 0.8f, 0.0f, 0.3f);
	return 1.5f * smoothstep(0.55f - 0.08f * min(field, 1.0f) - soft, 0.75f + soft, lumps);
}

// How thick the foam is at a point, 0 to about 3: the foam field, broken into its lace. `carried` is how far the current
// moved the water there, so the lumps drift with it.
float FoamThicknessAt(vec2 xz, vec2 carried, float pixel)
{
	float field = FoamFieldAt(xz).x;
	return field * FoamLace(field, UwFoamNoise((xz - carried) * 1.8f), pixel);
}

// How high the foam stands for a thickness of 1 at a point, in units: the shore foam stands lower
float FoamTallestAt(vec2 xz)
{
	return UW_FOAM_TALLEST * mix(1.0f, UW_SHORE_FOAM_HEIGHT_SCALE, FoamFieldAt(xz).w);
}

// The bubbles in the foam, as cells: x the distance to the middle of the nearest cell, y to the middle of the second
// nearest, in cells
vec2 FoamCells(vec2 p)
{
	vec2 cell = floor(p);
	vec2 f = p - cell;
	float nearest = 8.0f;
	float second = 8.0f;
	for (int j = -1; j <= 1; j++)
	{
		for (int i = -1; i <= 1; i++)
		{
			vec2 o = vec2(float(i), float(j));
			vec2 middle = o + 0.15f + 0.7f * UwHash2(cell + o);
			float d = length(middle - f);
			second = d < nearest ? nearest : min(second, d);
			nearest = min(nearest, d);
		}
	}
	return vec2(nearest, second);
}

// The bubbles that show in the foam at a point: x how much a bubble's rim and the gap between bubbles darken it, y the
// highlight of the light on the bubbles. Fresh foam with much air has large bubbles; old foam has small ones. They
// fade out where a pixel is larger than a bubble.
vec2 FoamBubbles(vec2 xz, vec3 field, float pixel)
{
	float size = mix(0.16f, 0.05f, smoothstep(0.0f, 12.0f, field.y)) * (0.7f + 0.3f * min(field.z, 1.0f));
	float shows = 1.0f - smoothstep(0.25f * size, 0.8f * size, pixel);
	if (shows <= 0.0f)
	{
		return vec2_splat(0.0f);
	}
	vec2 cells = FoamCells(xz / size);
	// The gap between two bubbles is where the two nearest middles are almost as far
	float gap = 1.0f - smoothstep(0.0f, 0.12f, cells.y - cells.x);
	float dome = 1.0f - smoothstep(0.0f, 0.55f, cells.x);
	float highlight = pow(dome, 6.0f);
	return vec2((gap * 0.3f + (1.0f - dome) * 0.1f) * shows, highlight * shows * (0.4f + 0.6f * min(field.z, 1.0f)));
}

// The water with the foam on it, seen from above along `direction`. `water` is the colour of the water there,
// `surfacePoint` where the view meets the water, `surface` the thin surface foam (0 to 1) and `thinColour` its colour,
// `pixel` the size of the pixel on the water, and `carried` how far the current moved the water there. The thin
// surface foam shows as before; the foam of the field stands on it.
vec3 VolumeFoam(vec3 water, vec3 thinColour, vec3 surfacePoint, vec3 direction, float surface, float pixel, vec2 carried,
                vec3 sky, vec3 scatter, float jitter)
{
	water = mix(water, thinColour, surface * 0.85f);
	float tallest = UW_FOAM_TALLEST * 1.5f;
	float down = max(-direction.y, 0.06f);
	// Where the view goes in at the top of the highest foam
	vec3 top = surfacePoint - direction * (tallest / down);
	float here = FoamFieldAt(surfacePoint.xz).x;
	float ahead = FoamFieldAt(top.xz).x;
	if (here < 0.02f && ahead < 0.02f)
	{
		return water;
	}
	// Go down the view from the top of the foam until it meets the foam: foam that stands in front of the point hides it
	vec3 hit = surfacePoint;
	float thickness = FoamThicknessAt(surfacePoint.xz, carried, pixel);
	// Eight steps, which start at a different place for each pixel, so that no bands show
	for (int i = 0; i < 8; i++)
	{
		vec3 at = mix(top, surfacePoint, min((float(i) + jitter) / 7.5f, 1.0f));
		float standing = FoamThicknessAt(at.xz, carried, pixel);
		if (at.y - surfacePoint.y <= min(standing, 1.5f) * FoamTallestAt(at.xz))
		{
			hit = at;
			thickness = standing;
			break;
		}
	}
	if (thickness < 0.01f)
	{
		return water;
	}
	vec3 field = FoamFieldAt(hit.xz).xyz;

	// The slope of the top of the foam
	float e = 0.4f;
	float hitTallest = FoamTallestAt(hit.xz);
	float dx = FoamThicknessAt(hit.xz + vec2(e, 0.0f), carried, pixel) - thickness;
	float dz = FoamThicknessAt(hit.xz + vec2(0.0f, e), carried, pixel) - thickness;
	vec3 normal = normalize(vec3(-dx * hitTallest / e, 1.0f, -dz * hitTallest / e));

	vec3 towards = u_uwAbove.xyz;
	vec3 light = u_uwLightColour.rgb * u_uwLightColour.w;
	// Thicker foam towards the light throws a shadow on this point; the deep parts of thick foam get less light
	float lean = hitTallest * thickness * 1.5f / max(towards.y, 0.15f);
	float blocker = FoamThicknessAt(hit.xz + towards.xz * lean, carried, pixel);
	float shadow = mix(0.6f, 1.0f, exp(-max(blocker - thickness, 0.0f) * 2.0f));
	float hollow = mix(1.0f, 0.75f, smoothstep(0.6f, 1.8f, thickness));
	vec2 bubbles = FoamBubbles(hit.xz, field, pixel);
	float shade = hollow * (1.0f - bubbles.x);
	float lit = 0.35f + 0.65f * clamp(dot(normal, towards) * 0.6f + 0.4f, 0.0f, 1.0f);
	vec3 colour = 0.92f * (light * lit * shadow * 0.9f + sky * 0.5f + scatter * 0.25f) * shade;
	// The light goes through the thin edges of the foam towards the eye, coloured by the water under it
	float thin = exp(-thickness * 3.0f);
	float forward = pow(clamp(dot(direction, towards) * 0.5f + 0.5f, 0.0f, 1.0f), 4.0f);
	vec3 tint = scatter / max(max(scatter.r, max(scatter.g, scatter.b)), 1e-3f);
	colour += light * mix(vec3_splat(1.0f), tint, 0.5f) * thin * (0.1f + 0.6f * forward);
	// The highlight of the light on the bubbles
	colour += light * bubbles.y * 0.6f * shadow;
	// Thin foam lets the water show through it
	float cover = smoothstep(0.02f, 0.45f, thickness) * 0.95f;
	return mix(water, min(colour, vec3_splat(1.2f)), cover);
}

// The underside of the foam seen from under the water, where it covers the surface: thin foam glows with the light
// that comes through it, thick foam is dark, and the bubbles show as bright rings. `surface` is the colour of the
// surface there without the foam.
vec3 FoamUnderside(vec3 surface, vec3 surfacePoint, float pixel, vec3 scatter)
{
	vec4 field = FoamFieldAt(surfacePoint.xz);
	if (field.x < 0.01f)
	{
		return surface;
	}
	float thickness = field.x * FoamLace(field.x, UwFoamNoise(surfacePoint.xz * 0.9f), pixel);
	vec3 light = u_uwLightColour.rgb * u_uwLightColour.w;
	vec2 bubbles = FoamBubbles(surfacePoint.xz, field.xyz, pixel);
	vec3 under = light * exp(-thickness * 2.2f) * (1.1f + 0.6f * bubbles.y) + scatter * (0.3f + 0.4f * exp(-thickness));
	under *= 1.0f - bubbles.x * 0.5f;
	float cover = 1.0f - exp(-thickness * 5.0f);
	return mix(surface, under, cover);
}

#endif // UNDERWATER_FOAM_SHADE_SH
