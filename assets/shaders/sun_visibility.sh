#ifndef SUN_VISIBILITY_SH
#define SUN_VISIBILITY_SH

// EXPERIMENT (not for merge): the map of the sun's visibility over the land (see fs_sun_visibility.sc), a texel a
// corner of the land as the shore map. r: how much of the sun's or the moon's light reaches the ground, 0 to 1, with
// the hills' shadows soft at their edges; g: how much of the sky the ground sees, 0 to 1; b: the height under which
// a point over the ground is in the hills' shadow.
// The map is always at sampler stage 12.

SAMPLER2D(s_sunVisibility, 12);
// x: 1 while the map shows, 0 while it is off; y: how dark the hills' shadow is, 0 to 1; zw: unused
uniform vec4 u_sunVisibility;

vec4 SunVisibilityTexel(vec2 xz)
{
	return texture2DLod(s_sunVisibility, (xz / 10.0f + 0.5f) / 513.0f, 0.0f);
}

// How much of the sun's or the moon's light reaches the ground at a point across the land, 1 while the map is off
float SunVisibilityAt(vec2 xz)
{
	if (u_sunVisibility.x < 0.5f)
	{
		return 1.0f;
	}
	return mix(1.0f, SunVisibilityTexel(xz).r, u_sunVisibility.y);
}

// How much of the sun's or the moon's light reaches the ground at a point, 0 to 1, without the softer darkness that the
// shade keeps: the glint on the water goes fully where the hills hide the sun. 1 while the map is off.
float SunLightAt(vec2 xz)
{
	return u_sunVisibility.x < 0.5f ? 1.0f : SunVisibilityTexel(xz).r;
}

// How much of the sun's or the moon's light reaches a point over the land: none under the hills' shadow, with a soft
// edge a few units deep
float SunVisibilityAbove(vec3 p)
{
	if (u_sunVisibility.x < 0.5f)
	{
		return 1.0f;
	}
	vec4 texel = SunVisibilityTexel(p.xz);
	return mix(1.0f, smoothstep(texel.b - 6.0f, texel.b + 6.0f, p.y), u_sunVisibility.y);
}

// How much of the sky the ground sees at a point, 1 while the map is off
float SkyOpennessAt(vec2 xz)
{
	return u_sunVisibility.x < 0.5f ? 1.0f : SunVisibilityTexel(xz).g;
}

#endif // SUN_VISIBILITY_SH
