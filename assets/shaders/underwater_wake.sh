#ifndef UNDERWATER_WAKE_SH
#define UNDERWATER_WAKE_SH

// EXPERIMENT (not for merge): the field of small waves that the things in the water write into, and the hulls that push
// the water down. A shader that reads them defines UW_WAKE before it includes underwater.sh. In all other shaders the
// field adds nothing to the water.

#ifdef UW_WAKE
// The field (see underwater::wake): x the height of the water over the sea, y the height a step before, z the foam
SAMPLER2D(s_wake, 8);
// xy: the low corner of the field across the ground; z: 1 over the field's width in units; w: 1 while the field shows
uniform vec4 u_uwWake;
// x: the space between two texels of the field in units; y: 1 over the field's side in texels; z: how many hulls
// there are; w: 1 while the hulls push the water down
uniform vec4 u_uwWakeInfo;
// For each hull, three: (the middle across the ground, the direction to the bow), (half the length, half the width,
// the draft, 1 for a boat), (the speed towards the bow, 0, 0, 0)
uniform vec4 u_uwHulls[24];

// The field at a point: x the height, y the foam, zw the slope along x and z. All 0 outside the field, and it fades to 0
// at the field's edges.
vec4 UwWakeAt(vec2 xz)
{
	if (u_uwWake.w < 0.5f)
	{
		return vec4_splat(0.0f);
	}
	vec2 uv = (xz - u_uwWake.xy) * u_uwWake.z;
	vec2 edge = min(uv, 1.0f - uv);
	float shown = smoothstep(0.0f, 0.06f, min(edge.x, edge.y));
	if (shown <= 0.0f)
	{
		return vec4_splat(0.0f);
	}
	vec4 here = texture2DLod(s_wake, uv, 0.0f);
	vec2 texel = vec2(u_uwWakeInfo.y, 0.0f);
	float east = texture2DLod(s_wake, uv + texel.xy, 0.0f).x;
	float west = texture2DLod(s_wake, uv - texel.xy, 0.0f).x;
	float north = texture2DLod(s_wake, uv + texel.yx, 0.0f).x;
	float south = texture2DLod(s_wake, uv - texel.yx, 0.0f).x;
	// The slope shows a little steeper than the height gives, so that a wake catches the light as a real one does
	vec2 slope = vec2(east - west, north - south) / (2.0f * u_uwWakeInfo.x) * 1.6f;
	return vec4(here.x, here.z, slope) * shown;
}

// How far a point is outside the edge of a hull, less than 0 inside it: round at the stern, pointed at the bow (the same
// as underwater::wake::HullDistance). `along` is how far along the hull the point is, -1 at the stern to 1 at the bow.
float UwHullDistance(vec4 a, vec4 b, vec2 p, out float along)
{
	vec2 away = p - a.xy;
	vec2 across = vec2(-a.w, a.z);
	along = dot(away, a.zw) / max(b.x, 1e-3f);
	float v = abs(dot(away, across));
	float u = clamp(along, -1.0f, 1.0f);
	float width = b.y * (u < 0.0f ? sqrt(max(1.0f - u * u, 0.0f)) : 1.0f - u * u);
	return max(v - width, (abs(along) - 1.0f) * b.x);
}

// How the hulls move the water at a point: the water is drawn down a little next to a hull, and a moving hull pushes a
// low mound of water ahead of its bow
float UwHullHeightAt(vec2 xz)
{
	float height = 0.0f;
	for (int i = 0; i < 8; i++)
	{
		if (float(i) >= u_uwWakeInfo.z)
		{
			break;
		}
		vec4 a = u_uwHulls[i * 3];
		vec4 b = u_uwHulls[i * 3 + 1];
		vec4 c = u_uwHulls[i * 3 + 2];
		float along;
		float d = UwHullDistance(a, b, xz, along);
		float outside = max(d, 0.0f);
		float draft = min(b.z, 1.5f);
		// Inside the hull the water is under it, at its keel
		float inside = 1.0f - smoothstep(-1.5f, 0.0f, d);
		height -= draft * (0.1f * exp(-outside / 1.2f) + 0.15f * inside);
		float moving = clamp(abs(c.x) / 4.0f, 0.0f, 1.0f);
		float ahead = clamp(along * sign(c.x), 0.0f, 1.5f);
		height += 0.25f * draft * moving * ahead * exp(-outside / 1.8f);
	}
	return height;
}

// The height that the field and the hulls add to the water at a point, and the slope that they add
float UwWake(vec2 xz, out vec2 slope)
{
	vec4 field = UwWakeAt(xz);
	float height = field.x;
	slope = field.zw;
	if (u_uwWakeInfo.w > 0.5f && u_uwWakeInfo.z > 0.5f)
	{
		float hull = UwHullHeightAt(xz);
		float e = 0.25f;
		slope += vec2(UwHullHeightAt(xz + vec2(e, 0.0f)) - hull, UwHullHeightAt(xz + vec2(0.0f, e)) - hull) / e;
		height += hull;
	}
	return height;
}

// The foam of the field at a point, broken into patches and threads of foam by noise: more foam fills more of them
float UwWakeFoam(vec2 xz, float t)
{
	float amount = UwWakeAt(xz).y;
	if (amount <= 0.01f)
	{
		return 0.0f;
	}
	float pattern = UwFoamNoise(xz * 1.6f + vec2(t * 0.05f, -t * 0.04f)) * 0.7f + UwFoamNoise(xz * 4.1f - t * 0.1f) * 0.3f;
	float threshold = 1.0f - clamp(amount, 0.0f, 1.0f);
	return smoothstep(threshold - 0.12f, threshold + 0.12f, pattern) * min(amount * 1.2f, 0.9f);
}

// Whether a point of the water's surface is inside a boat's hull, where no water shows, 0 to 1
float UwInsideBoat(vec2 xz)
{
	float inside = 0.0f;
	if (u_uwWakeInfo.w < 0.5f)
	{
		return inside;
	}
	for (int i = 0; i < 8; i++)
	{
		if (float(i) >= u_uwWakeInfo.z)
		{
			break;
		}
		vec4 b = u_uwHulls[i * 3 + 1];
		if (b.w > 0.5f)
		{
			float along;
			float d = UwHullDistance(u_uwHulls[i * 3], b, xz, along);
			inside = max(inside, 1.0f - smoothstep(-0.12f, 0.0f, d));
		}
	}
	return inside;
}

// The water line on the hulls: a thin band of foam where the water meets the hull, broken a little by noise
float UwHullLine(vec2 xz, float t)
{
	float band = 0.0f;
	if (u_uwWakeInfo.w < 0.5f)
	{
		return band;
	}
	for (int i = 0; i < 8; i++)
	{
		if (float(i) >= u_uwWakeInfo.z)
		{
			break;
		}
		float along;
		float d = UwHullDistance(u_uwHulls[i * 3], u_uwHulls[i * 3 + 1], xz, along);
		float moving = clamp(abs(u_uwHulls[i * 3 + 2].x) / 3.0f, 0.0f, 1.0f);
		float width = 0.12f + 0.3f * moving;
		band = max(band, (1.0f - smoothstep(0.0f, width, max(d, 0.0f))) * step(-0.05f, d) *
		                     (0.55f + 0.45f * UwNoise(xz * 5.0f + t * 2.0f)));
	}
	return band;
}
// The flat colour of the water for the debug view that compares it with the original sea: magenta, with the field's
// height in red and blue and its foam in green, and the hulls' edges in white
vec3 UwWakeDebug(vec2 xz)
{
	vec4 field = UwWakeAt(xz);
	vec3 colour = vec3(1.0f, 0.0f, 1.0f) + vec3(field.x * 8.0f, field.y, -field.x * 8.0f);
	float band = 0.0f;
	for (int i = 0; i < 8; i++)
	{
		if (float(i) >= u_uwWakeInfo.z)
		{
			break;
		}
		float along;
		band = max(band, 1.0f - smoothstep(0.0f, 0.15f, abs(UwHullDistance(u_uwHulls[i * 3], u_uwHulls[i * 3 + 1], xz, along))));
	}
	return clamp(mix(colour, vec3_splat(1.0f), band), 0.0f, 1.0f);
}
#else
float UwWake(vec2 xz, out vec2 slope)
{
	slope = vec2_splat(0.0f);
	return 0.0f;
}
#endif

#endif // UNDERWATER_WAKE_SH
