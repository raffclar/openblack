#ifndef HAZE_SH
#define HAZE_SH

// The game's distance haze, from the land's light table, and the temple's depth fog (src/3D/TempleFog.h), which works
// the same way in 1024 steps. Colours here are 0 to 255.

uniform vec4 u_haze;       // x: near distance, y: far distance, z: k, w: 1 when the haze is on, 2 for the temple's fog
uniform vec4 u_hazeColour; // rgb: the haze's colour

bool InTempleFog()
{
	return u_haze.w > 1.5f;
}

// The temple fog's step at a depth, 0 to 1023: past its end it stays at its last
float TempleFogStep(float depth)
{
	if (depth < u_haze.x)
	{
		return 0.0f;
	}
	if (depth > u_haze.y)
	{
		return 1023.0f;
	}
	return clamp(floor((depth - u_haze.x) * (1024.0f / (u_haze.y - u_haze.x))), 0.0f, 1023.0f);
}

// How far into the haze a depth is, 0 to 1; 0 with the haze off. The temple's fog goes in its steps.
float HazeT(float depth)
{
	if (u_haze.w <= 0.0f)
	{
		return 0.0f;
	}
	if (InTempleFog())
	{
		return TempleFogStep(depth) / 1024.0f;
	}
	return (min(max(depth, u_haze.x), u_haze.y) - u_haze.x) / (u_haze.y - u_haze.x);
}

// What a colour is scaled by, of 256: down to k at full haze, in whole steps. The temple's fog takes the scale as a
// byte of 255.
float HazeFactor(float t)
{
	float factor = 256.0f - floor((256.0f - u_haze.z) * t);
	return InTempleFog() ? floor(factor * 255.0f / 256.0f) : factor;
}

// A colour scaled by the factor, (c f) >> 8 per channel
vec3 HazeDiffuse(vec3 colour, float factor)
{
	return factor < 256.0f ? floor(colour * factor / 256.0f) : colour;
}

// The haze colour added at t, rounded to the nearest with halves to even
vec3 HazeColour(float t)
{
	vec3 x = u_hazeColour.rgb * t;
	vec3 r = floor(x + 0.5f);
	return r - step(vec3_splat(0.5f), mod(r, vec3_splat(2.0f))) * step(vec3_splat(0.5f), r - x);
}

// A lightmapped wall's vertex in the temple's fog: up to the start it keeps its own colour and added colour, past the end
// it takes the last step's alone, and between them each byte moves towards the step's by a quarter of the step, of 256
void TempleFogVertex(float depth, inout vec3 diffuse, inout vec3 added)
{
	if (!(depth > u_haze.x))
	{
		return;
	}
	if (!(depth < u_haze.y))
	{
		diffuse = vec3_splat(HazeFactor(1023.0f / 1024.0f));
		added = HazeColour(1023.0f / 1024.0f);
		return;
	}
	float step = TempleFogStep(depth);
	float share = floor(step / 4.0f);
	diffuse += floor((vec3_splat(HazeFactor(step / 1024.0f)) - diffuse) * share / 256.0f);
	added += floor((HazeColour(step / 1024.0f) - added) * share / 256.0f);
}

#endif // HAZE_SH
