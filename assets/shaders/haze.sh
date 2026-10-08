// The original's software distance haze on the GPU: the GPU side of src/Graphics/Haze.h (graphics::haze), with the
// same rounding. u_haze: x near, y far, z k, w on (the "Fog" key);
// u_hazeColour: rgb the colour, 0..255. Colours here are bytes, 0..255.
#ifndef HAZE_SH
#define HAZE_SH

uniform vec4 u_haze;
uniform vec4 u_hazeColour;

// t = (min(max(z, near), far) - near) / (far - near), the same for the objects and the land
float HazeT(float depth)
{
	return (min(max(depth, u_haze.x), u_haze.y) - u_haze.x) / (u_haze.y - u_haze.x);
}

// f = 256 - trunc((256 - k) t); t >= 0, so floor is the truncation
float HazeFactor(float t)
{
	return 256.0f - floor((256.0f - u_haze.z) * t);
}

// (c f) >> 8 per byte, only when f < 256
vec3 ApplyHazeDiffuse(vec3 colour, float f)
{
	return f < 256.0f ? floor(colour * f / 256.0f) : colour;
}

// The FPU's default rounding: to nearest, halves to even
vec3 RoundHalfEven(vec3 x)
{
	vec3 r = floor(x + 0.5f);
	return r - step(vec3_splat(0.5f), mod(r, vec3_splat(2.0f))) * step(vec3_splat(0.5f), r - x);
}

// The haze colour c t, rounded to the nearest (halves to even)
vec3 HazeColour(float t)
{
	return RoundHalfEven(u_hazeColour.rgb * t);
}

// The land's class 2: the full haze colour, each channel truncated to a byte
vec3 HazeColourFull()
{
	return floor(u_hazeColour.rgb);
}

// a + b per channel capped at 0xFF
vec3 HazeAddSaturated(vec3 a, vec3 b)
{
	return min(a + b, vec3_splat(255.0f));
}

#endif // HAZE_SH
