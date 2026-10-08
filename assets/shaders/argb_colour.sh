#ifndef ARGB_COLOUR_SH
#define ARGB_COLOUR_SH

// The GPU side of src/Graphics/ArgbColour.h: the byte arithmetic of the engine's colours (a D3DCOLOR, 0xAARRGGBB), on
// colours kept as 0..255 floats with integer values. Wiki: rendering-objects.md, colour arithmetic. The CPU and this
// one must stay the same: every routine of the original truncates, none rounds. The alpha rule (multiplied, kept or
// opaque) is the caller's: these work on whatever channels they are given. vs_object.sc decodes the instance's colour
// column with it (argb_colour::PackInstance*).

// (c t) >> 8 per channel, truncated: the object tint (all four channels, or RGB keeping a.A), the model light's
// (c f) >> 8 and the trees' brightness
vec4 MultiplyShift8(vec4 c255, vec4 t255)
{
	return floor(c255 * t255 / 256.0f);
}
vec3 MultiplyShift8(vec3 c255, vec3 t255)
{
	return floor(c255 * t255 / 256.0f);
}

// min(a + b, 255) per channel, as the original adds the specular: over all four channels or over RGB only
// (then a.A is kept)
vec4 AddSaturated(vec4 a255, vec4 b255)
{
	return min(a255 + b255, vec4_splat(255.0f));
}
vec3 AddSaturated(vec3 a255, vec3 b255)
{
	return min(a255 + b255, vec3_splat(255.0f));
}

// trunc(c l / 255) per channel: the untextured primitives, the mists and the creature (the 0x80808081
// multiply). x = c l is an exact integer (0..65025) in a float; a shader division is not correctly rounded (often
// x * rcp(255), 2.5 ULP in GLSL), so a plain floor(x / 255) can drop to k - 1 when x = 255 k. With x + 0.5 the
// quotient is k + (r + 0.5) / 255 (r = x mod 255 <= 254), its fraction in [0.002, 0.998], far from either integer.
// Not checked on a GPU yet (approximate until then).
vec3 MultiplyDiv255(vec3 c255, vec3 l255)
{
	return floor((c255 * l255 + 0.5f) / 255.0f);
}

// openblack's own transport, no original: a 0xRRGGBB packed into one float (exact up to 2^24) by the CPU, as the
// instance and u_objectLight carry it, back to 0..255 per channel (the divisions are by powers of two: exact; the
// parameter is not called "packed", a reserved word of GLSL)
vec3 UnpackRgb24(float rgb24)
{
	float red = floor(rgb24 / 65536.0f);
	float green = floor((rgb24 - red * 65536.0f) / 256.0f);
	return vec3(red, green, rgb24 - red * 65536.0f - green * 256.0f);
}

#endif // ARGB_COLOUR_SH
