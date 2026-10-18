#ifndef WORLD_CURVE_SH
#define WORLD_CURVE_SH

// EXPERIMENT (not for merge): the world as a ball (atmosphere::WorldDrop and the functions near it). A flat cap holds
// the whole map, so the map and the coast do not change. Past the cap the world is a sphere of the planet's radius.
// Low down the cap is round the eye. As the eye climbs, the cap moves to the map's middle, so that from space the sea
// is the same ball as the air's planet. Each ray from the eye lies in an upright plane through the eye. In that plane
// the ground is flat along the cap's chord, and past each end of the chord it is a circle.

// xy: the eye's x and z; z: the radius of the flat cap; w: the world's radius, 0 for a flat world
uniform vec4 u_worldCurve;
// xy: the eye's x and z less the middle of the flat cap; z: the eye's height over the curved world; w: unused
uniform vec4 u_worldCap;
// The rows of the turn from the frame that the camera works in to the world's frame. It is not the identity only low
// down far round the planet, where the frame is the frame of that place.
uniform vec4 u_worldTurn[3];
// xyz: the direction towards the real sun, for the day and the night on the planet's ball
uniform vec4 u_planetSun;
// xyz: the direction towards the real moon; w: the share of the island's light that it gives at its highest
uniform vec4 u_planetMoon;

// A direction in the camera's frame as a direction in the world's frame, for what has its place on the planet
vec3 WorldDirection(vec3 dir)
{
	return vec3(dot(u_worldTurn[0].xyz, dir), dot(u_worldTurn[1].xyz, dir), dot(u_worldTurn[2].xyz, dir));
}

// Over this height the eye can be far round the planet, so the passes take heights and directions from the ball
#define ATMOS_HIGH_EYE 20000.0f

// The eye's height over the curved world. `heightOverSea` is the eye's y less the sea's level, which is the same near
// the map. Far round the ball the eye can be under the sea's level and high over the ball.
float WorldEyeHeight(float heightOverSea)
{
	return u_worldCurve.w > 0.0f ? u_worldCap.z : heightOverSea;
}

// The line from the middle of the world's ball to the eye, from the eye's height over the sea's level
vec3 WorldMiddleToEye(float heightOverSea)
{
	return vec3(u_worldCap.x, heightOverSea + u_worldCurve.w, u_worldCap.y);
}

// The up of the world's ball at a point `fromEye` from the eye: the line from the ball's middle through the point.
// Over a flat world it is straight up.
vec3 WorldUpAt(vec3 fromEye, float heightOverSea)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return vec3(0.0f, 1.0f, 0.0f);
	}
	return normalize(WorldMiddleToEye(heightOverSea) + fromEye);
}

// How much of the island's light lights a point of the ball with the up `up`, 0 to 1, from the height of the real sun
// there and over the island. A point with the sun as high as over the island takes all of the light. On the night side
// the moon gives a share of the light where it is up.
float WorldLightShare(vec3 up)
{
	float here = smoothstep(-0.12f, 0.05f, dot(up, u_planetSun.xyz));
	float overIsland = smoothstep(-0.12f, 0.05f, u_planetSun.y);
	float sun = overIsland > 1e-3f ? min(here / overIsland, 1.0f) : here;
	float moon = u_planetMoon.w * smoothstep(-0.05f, 0.3f, dot(up, u_planetMoon.xyz));
	return max(sun, moon);
}

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

// The world in the upright plane through the eye along the direction `across` (a unit vector on the ground): x and y
// are where the flat chord starts and stops along the plane from the eye, and z is the square of how far the plane is
// past the cap's edge. A plane past the cap has no chord: x and y are the point nearest to the cap.
vec3 WorldSection(vec2 across)
{
	vec2 fromMiddle = u_worldCap.xy;
	float b = dot(fromMiddle, across);
	float side = max(dot(fromMiddle, fromMiddle) - b * b, 0.0f);
	float cap = u_worldCurve.z;
	if (side < cap * cap)
	{
		float halfChord = sqrt(cap * cap - side);
		return vec3(-b - halfChord, -b + halfChord, 0.0f);
	}
	float past = sqrt(side) - cap;
	return vec3(-b, -b, past * past);
}

// The direction on the ground of a ray, or, for a ray straight up or down, the direction towards the cap
vec2 WorldAcross(vec2 dirXz)
{
	float across = length(dirXz);
	if (across > 1e-6f)
	{
		return dirXz / across;
	}
	float fromMiddle = length(u_worldCap.xy);
	return fromMiddle > 1e-3f ? -u_worldCap.xy / fromMiddle : vec2(1.0f, 0.0f);
}

// How far a point at `along` units from the eye in a plane is past the ends of the plane's flat chord
float WorldPastChord(vec3 section, float along)
{
	return along > section.y ? along - section.y : max(section.x - along, 0.0f);
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
	vec2 away = xz - u_worldCurve.xy;
	vec3 section = WorldSection(WorldAcross(away));
	float past = WorldPastChord(section, length(away));
	float square = past * past + section.z;
	if (square >= radius * radius)
	{
		return 1e9f;
	}
	return square / (radius + sqrt(radius * radius - square));
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
	float along = length(away);
	vec2 across = WorldAcross(away);
	vec3 section = WorldSection(across);
	float past = WorldPastChord(section, along);
	float rest = max(radius * radius - section.z - past * past, radius * radius * 1e-6f);
	// Before the chord the ground climbs towards the cap, and after it the ground goes down
	float downhill = along > section.y ? 1.0f : -1.0f;
	return across * (downhill * past / sqrt(rest));
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
	vec2 away = p.xz - u_worldCurve.xy;
	vec3 section = WorldSection(WorldAcross(away));
	float past = WorldPastChord(section, length(away));
	float square = past * past + section.z;
	float up = h + radius;
	// sqrt(square + up^2) - radius, with no loss of the small terms
	return sea + (square + h * (h + 2.0f * radius)) / (sqrt(square + up * up) + radius);
}

// Where a ray meets a circle in the ray's upright plane. The circle's middle is `middle` units along the plane and
// the world's radius under the sea. `c` is the square of the eye's distance from the middle less the square of the
// circle's radius. The ray goes t (across, up) from the eye. x is where it goes in and y where it comes out; none when
// x >= y.
vec2 WorldCircleSpan(float middle, float c, float across, float up, float heightOverSea)
{
	float b = -across * middle + up * (heightOverSea + u_worldCurve.w);
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

// Where a ray from the eye is inside the world with `lift` units added to its height: x where it goes in, y where it
// comes out, along the ray. `heightOverSea` is the eye's height over the sea's level. None when x >= y. In the ray's
// upright plane this shape is the slab over the flat chord with a circle at each end. The shape is convex, so the
// span goes from the first entry to the last exit of the three parts.
vec2 WorldBallSpan(float heightOverSea, vec3 dir, float lift)
{
	float radius = u_worldCurve.w;
	float across = length(dir.xz);
	vec3 section = WorldSection(WorldAcross(dir.xz));
	// The square of the eye's distance from a circle's middle less the square of its radius, with no loss of the small
	// terms. Each circle is a section of the ball of the radius plus the lift.
	float c0 = (heightOverSea - lift) * (heightOverSea + lift + 2.0f * radius) + section.z;
	vec2 nearSpan = WorldCircleSpan(section.x, section.x * section.x + c0, across, dir.y, heightOverSea);
	vec2 farSpan = WorldCircleSpan(section.y, section.y * section.y + c0, across, dir.y, heightOverSea);
	vec2 span = vec2(min(nearSpan.x, farSpan.x), max(nearSpan.y, farSpan.y));
	span = nearSpan.x < nearSpan.y ? (farSpan.x < farSpan.y ? span : nearSpan) : farSpan;
	// The slab over the chord, from its top at the lifted sea down to the bottom of the circles
	if (section.y > section.x)
	{
		vec2 slab = vec2(-1e30f, 1e30f);
		if (across > 1e-6f)
		{
			slab = vec2(section.x / across, section.y / across);
		}
		else if (section.x > 0.0f || section.y < 0.0f)
		{
			slab = vec2(1.0f, -1.0f);
		}
		float top = heightOverSea - lift;
		float bottom = heightOverSea + lift + 2.0f * radius;
		if (dir.y < 0.0f)
		{
			slab = vec2(max(slab.x, top / -dir.y), min(slab.y, bottom / -dir.y));
		}
		else if (dir.y > 0.0f)
		{
			slab = vec2(max(slab.x, -bottom / dir.y), min(slab.y, -top / dir.y));
		}
		else if (top > 0.0f)
		{
			slab = vec2(1.0f, -1.0f);
		}
		if (slab.x < slab.y)
		{
			span = span.x < span.y ? vec2(min(span.x, slab.x), max(span.y, slab.y)) : slab;
		}
	}
	return span;
}

// The distance along a ray from the eye to the curved sea, from the eye's height over the sea's level. -1 when the ray
// goes past the limb and does not meet the sea, or when the eye is under the sea.
float CurvedSeaDistance(float heightOverSea, vec3 dir)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return heightOverSea > 0.0f && dir.y < 0.0f ? heightOverSea / -dir.y : -1.0f;
	}
	vec2 span = WorldBallSpan(heightOverSea, dir, 0.0f);
	return span.x < span.y && span.x > 0.0f ? span.x : -1.0f;
}

// The y of the direction from the eye to the curved sea's horizon, from the eye's height over the sea's level: 0 for a
// flat world, and under 0 as the eye climbs. It is the horizon in the plane at a right angle to the line from the cap's
// middle.
float WorldHorizonY(float heightOverSea)
{
	if (u_worldCurve.w <= 0.0f)
	{
		return 0.0f;
	}
	float radius = u_worldCurve.w;
	float fromMiddle = length(u_worldCap.xy);
	float cap = u_worldCurve.z;
	float chord = fromMiddle < cap ? sqrt(cap * cap - fromMiddle * fromMiddle) : 0.0f;
	float past = max(fromMiddle - cap, 0.0f);
	float circle = sqrt(max(radius * radius - past * past, 0.0f));
	float up = max(heightOverSea, 0.0f) + radius;
	float toMiddle = sqrt(chord * chord + up * up);
	float down = atan2(up, chord) - asin(min(circle / toMiddle, 1.0f));
	return -sin(down);
}

#endif // WORLD_CURVE_SH
