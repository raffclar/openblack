#ifndef WORLD_CURVE_SH
#define WORLD_CURVE_SH

// EXPERIMENT (not for merge): the curve of the world far away (atmosphere::WorldDrop and the functions near it). Inside
// the flat reach round the eye the world is flat, so the map and the coast do not change. Past it the world goes down
// with the square of the distance over twice the planet's radius, so the far sea goes down under the horizon.

// xy: the eye's x and z; z: the flat reach; w: the world's radius, 0 for a flat world
uniform vec4 u_worldCurve;

// How far the world goes down at a point across the ground
float WorldDrop(vec2 xz)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return 0.0f;
	}
	float past = max(length(xz - u_worldCurve.xy) - u_worldCurve.z, 0.0f);
	return past * past / (2.0f * u_worldCurve.w);
}

// How steeply the world goes down at a point across the ground, along x and z
vec2 WorldDropSlope(vec2 xz)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return vec2_splat(0.0f);
	}
	vec2 away = xz - u_worldCurve.xy;
	float distance = length(away);
	float past = max(distance - u_worldCurve.z, 0.0f);
	return distance > 1e-3f ? away / distance * past / u_worldCurve.w : vec2_splat(0.0f);
}

// The distance along a ray from the eye to the curved sea, from the eye's height over the sea's level. -1 when the ray
// goes over the horizon and does not meet the sea.
float CurvedSeaDistance(float heightOverSea, vec3 dir)
{
	if (dir.y >= 0.0f || heightOverSea <= 0.0f)
	{
		return -1.0f;
	}
	float straight = heightOverSea / -dir.y;
	float across = length(dir.xz);
	if (u_worldCurve.w <= 0.0f || straight * across <= u_worldCurve.z)
	{
		return straight;
	}
	// Past the flat reach the gap between the ray and the sea is c - k u + u^2 / (2 R), with u the distance across the
	// ground past the reach
	float k = -dir.y / across;
	float c = heightOverSea - k * u_worldCurve.z;
	float disc = k * k - 2.0f * c / u_worldCurve.w;
	if (disc < 0.0f)
	{
		return -1.0f;
	}
	return (2.0f * c / (k + sqrt(disc)) + u_worldCurve.z) / across;
}

// The y of the direction from the eye to the curved sea's horizon, from the eye's height over the sea's level: 0 for a
// flat world, and under 0 as the eye climbs
float WorldHorizonY(float heightOverSea)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return 0.0f;
	}
	float d = u_worldCurve.z / u_worldCurve.w;
	float k = sqrt(d * d + 2.0f * max(heightOverSea, 0.0f) / u_worldCurve.w) - d;
	return -k / sqrt(1.0f + k * k);
}

#endif // WORLD_CURVE_SH
