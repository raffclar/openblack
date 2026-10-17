#ifndef WORLD_CURVE_SH
#define WORLD_CURVE_SH

// EXPERIMENT (not for merge): the world as a ball (atmosphere::WorldDrop and the functions near it). Inside the flat
// reach round the eye the world is flat, so the map and the coast do not change. Past it the world is a sphere of the
// planet's radius: each ray from the eye lies in an upright plane through the eye, and in that plane the ground past the
// reach is a circle. From high up the sea is a ball with its limb against space.

// xy: the eye's x and z; z: the flat reach; w: the world's radius, 0 for a flat world
uniform vec4 u_worldCurve;

// The direction of the ray from the eye through a point of the screen (0 to 1). It comes from the view's turn alone,
// not from two far points less the eye, so it keeps its precision when the eye is far from the world's middle.
vec3 ViewRayAt(vec2 uv)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	vec4 ndc = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
#else
	vec4 ndc = vec4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.5f, 1.0f);
#endif
	vec4 inView = mul(u_invProj, ndc);
	return normalize(mul(u_invView, vec4(inView.xyz / inView.w, 0.0f)).xyz);
}

// How far the world goes down at a point across the ground. Past a quarter of the planet the ground is out of sight
// and the drop is very large.
float WorldDrop(vec2 xz)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return 0.0f;
	}
	float radius = u_worldCurve.w;
	float past = max(length(xz - u_worldCurve.xy) - u_worldCurve.z, 0.0f);
	if (past >= radius)
	{
		return 1e9f;
	}
	return past * past / (radius + sqrt(radius * radius - past * past));
}

// How steeply the world goes down at a point across the ground, along x and z
vec2 WorldDropSlope(vec2 xz)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return vec2_splat(0.0f);
	}
	float radius = u_worldCurve.w;
	vec2 away = xz - u_worldCurve.xy;
	float distance = length(away);
	float past = min(max(distance - u_worldCurve.z, 0.0f), radius * 0.999f);
	return distance > 1e-3f ? away / distance * past / sqrt(radius * radius - past * past) : vec2_splat(0.0f);
}

// The height of a point over the curved world, along the line from the ball's middle, as a height in world units
float WorldAltitude(vec3 p, float sea)
{
	float h = p.y - sea;
	if (u_worldCurve.w <= 0.0f)
	{
		return p.y;
	}
	float radius = u_worldCurve.w;
	float past = max(length(p.xz - u_worldCurve.xy) - u_worldCurve.z, 0.0f);
	float up = h + radius;
	// sqrt(past^2 + up^2) - radius, with no loss of the small terms
	return sea + (past * past + h * (h + 2.0f * radius)) / (sqrt(past * past + up * up) + radius);
}

// Where a ray from the eye is inside the ball of the world with `lift` units added to its radius: x where it goes in,
// y where it comes out, along the ray. `heightOverSea` is the eye's height over the sea's level. None when x >= y.
// The ball's middle is under the far edge of the flat reach in the ray's upright plane.
vec2 WorldBallSpan(float heightOverSea, vec3 dir, float lift)
{
	float radius = u_worldCurve.w;
	float across = length(dir.xz);
	float reach = u_worldCurve.z;
	// The eye from the ball's middle, in the upright plane: (-reach, heightOverSea + radius). The ray's length along
	// the plane is 1, so the square term is 1.
	float up = heightOverSea + radius;
	float b = -across * reach + dir.y * up;
	// reach^2 + up^2 - (radius + lift)^2, with no loss of the small terms
	float c = reach * reach + (heightOverSea - lift) * (heightOverSea + lift + 2.0f * radius);
	float disc = b * b - c;
	if (disc < 0.0f)
	{
		return vec2(1.0f, -1.0f);
	}
	float s = sqrt(disc);
	// The roots of t^2 + 2 b t + c, kept exact for a small c
	float q = b < 0.0f ? -b + s : -b - s;
	vec2 roots = abs(q) > 1e-20f ? vec2(c / q, q) : vec2(-b, -b);
	return vec2(min(roots.x, roots.y), max(roots.x, roots.y));
}

// The distance along a ray from the eye to the curved sea, from the eye's height over the sea's level. -1 when the ray
// goes past the limb and does not meet the sea.
float CurvedSeaDistance(float heightOverSea, vec3 dir)
{
	if (heightOverSea <= 0.0f)
	{
		return -1.0f;
	}
	float across = length(dir.xz);
	if (u_worldCurve.w <= 0.0f)
	{
		return dir.y < 0.0f ? heightOverSea / -dir.y : -1.0f;
	}
	if (dir.y < 0.0f)
	{
		float straight = heightOverSea / -dir.y;
		if (straight * across <= u_worldCurve.z)
		{
			return straight;
		}
	}
	vec2 span = WorldBallSpan(heightOverSea, dir, 0.0f);
	// The ray must meet the ball past the flat reach
	return span.x < span.y && span.x > 0.0f && span.x * across >= u_worldCurve.z * 0.999f ? span.x : -1.0f;
}

// The y of the direction from the eye to the curved sea's horizon, from the eye's height over the sea's level: 0 for a
// flat world, and under 0 as the eye climbs
float WorldHorizonY(float heightOverSea)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return 0.0f;
	}
	float radius = u_worldCurve.w;
	float up = max(heightOverSea, 0.0f) + radius;
	float toMiddle = sqrt(u_worldCurve.z * u_worldCurve.z + up * up);
	float down = atan2(up, u_worldCurve.z) - asin(min(radius / toMiddle, 1.0f));
	return -sin(down);
}

#endif // WORLD_CURVE_SH
