#ifndef AERIAL_HAZE_SH
#define AERIAL_HAZE_SH

#include "world_curve.sh"

// EXPERIMENT (not for merge): the haze between the eye and a point of the scene or a cloud, from the haze's grid that
// follows the view. Include atmosphere.sh first and declare the grid's sampler s_atmosAerial.

// The haze from the eye along `dir` over `range` world units, at the point `uv` of the screen: rgb the light that it
// adds, a the part of what is behind that shows through. `sunHeight` is the sine of the air's sun over the horizon.
vec4 AtmosHazeAlong(vec2 uv, vec3 eye, vec3 dir, float range, float sunHeight)
{
	float slice = clamp(AtmosSliceAt(range), 0.0f, ATMOS_AERIAL_SLICES - 1.0f);
	float first = floor(slice);
	float second = min(first + 1.0f, ATMOS_AERIAL_SLICES - 1.0f);
	vec4 haze = mix(texture2DLod(s_atmosAerial, AtmosAerialUv(uv, first), 0.0f),
	                texture2DLod(s_atmosAerial, AtmosAerialUv(uv, second), 0.0f), slice - first);
	// Nearer than the first slice, the haze is a share of the first slice's
	float nearShare = clamp(range / AtmosSliceDistance(0.0f), 0.0f, 1.0f);
	haze = mix(vec4(0.0f, 0.0f, 0.0f, 1.0f), haze, nearShare);
	// Past the middle of the last slice the grid does not change. From there the haze continues, and the scene fades
	// into the sky that the dome shows at the horizon in the same direction. At the horizon the far sea is all haze and
	// has the colour of the sky above it, so the line between them does not show. The air is thinner high up, so a path
	// from a high eye down to the sea holds less haze than a path along the sea.
	float sea = u_atmosParams.w;
	float fromHeight = (eye.y - sea) / ATMOS_UNITS_PER_KM;
	vec3 end = eye + dir * range;
	float toHeight = (end.y + WorldDrop(end.xz) - sea) / ATMOS_UNITS_PER_KM;
	float farther = max(range - AtmosSliceDistance(ATMOS_AERIAL_SLICES - 1.0f), 0.0f) / ATMOS_UNITS_PER_KM *
	                AtmosMeanExtinction(fromHeight, toHeight);
	float hidden = 1.0f - exp(-farther);
	// The sky over the sea's horizon in this direction: the horizon goes down under the eye's level as the eye climbs.
	// From high up a ray down to the sea goes through all of the air, and the air's light along that ray is its haze.
	float horizonY = WorldHorizonY(eye.y - sea);
	float fromAbove = smoothstep(2000.0f, 20000.0f, eye.y - sea);
	vec3 hazeDir = normalize(vec3(dir.x, mix(max(dir.y, horizonY), dir.y, fromAbove), dir.z));
	vec3 horizonSky = AtmosSkyOnScreen(hazeDir, sunHeight);
	// A thick haze has the colour of the sky at the horizon, as the dome shows it. A thin haze near the eye keeps the
	// grid's own light, with the shadows of the hills and the clouds in it.
	float opacity = 1.0f - haze.a;
	vec3 added = mix(haze.rgb * u_atmosSun.rgb, horizonSky * opacity, opacity);
	return vec4(mix(added, horizonSky, hidden), haze.a * (1.0f - hidden));
}

#endif // AERIAL_HAZE_SH
