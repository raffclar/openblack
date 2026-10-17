#ifndef LIGHTING_SH
#define LIGHTING_SH

// EXPERIMENT (not for merge): the light in the shade (see src/Graphics/GlobalIllumination.h): the sky's light as four
// spherical harmonics for each colour, and the grid of light probes over the island, which hold the light that the lit
// land and the water send back. The grid is always at sampler stage 15: one texture of three tiles side by side, one
// for each colour, each texel a probe's four harmonics of that colour (the mean, then the parts along y, z and x). The
// light is for a light of strength 1 at the top of the air.

SAMPLER2D(s_lightProbes, 15);
// The sky's light: [0] the harmonics of red, [1] of green, [2] of blue
uniform vec4 u_lightSky[3];
// x: 1 while the grid shows; y: 1 while the light in the shade shows; z: the world units between two probes; w: the
// probes along each side
uniform vec4 u_lightParams;
// rgb: the light of the open sky on flat ground; w: the share of the light's colour that the shade keeps
uniform vec4 u_lightOpenSky;

#define LIGHT_PI 3.14159265f
#define LIGHT_Y0 0.282095f
#define LIGHT_Y1 0.488603f
// The darkest and the brightest that the light of the shade may make a pixel (global_illumination::k_DarkestShade)
#define LIGHT_DARKEST 0.35f
#define LIGHT_BRIGHTEST 2.0f

// The light that falls on a surface that faces `n`, from harmonics of red, green and blue (never below 0)
vec3 LightIrradiance(vec4 r, vec4 g, vec4 b, vec3 n)
{
	vec4 basis = vec4(LIGHT_PI * LIGHT_Y0, (2.0f * LIGHT_PI / 3.0f) * LIGHT_Y1 * n.y, (2.0f * LIGHT_PI / 3.0f) * LIGHT_Y1 * n.z,
	                  (2.0f * LIGHT_PI / 3.0f) * LIGHT_Y1 * n.x);
	return max(vec3(dot(r, basis), dot(g, basis), dot(b, basis)), vec3_splat(0.0f));
}

// The light from harmonics in one direction, as the harmonics hold it (never below 0)
vec3 LightRadiance(vec4 r, vec4 g, vec4 b, vec3 d)
{
	vec4 basis = vec4(LIGHT_Y0, LIGHT_Y1 * d.y, LIGHT_Y1 * d.z, LIGHT_Y1 * d.x);
	return max(vec3(dot(r, basis), dot(g, basis), dot(b, basis)), vec3_splat(0.0f));
}

// The sky's light on a surface that faces `n`, with no hill in the way
vec3 LightSkyAt(vec3 n)
{
	return LightIrradiance(u_lightSky[0], u_lightSky[1], u_lightSky[2], n);
}

// The light that the land and the water send back onto a surface at a point that faces `n`, from the grid. Each tile is
// read inside itself, so no colour bleeds into the next.
vec3 LightBounceAt(vec3 world, vec3 n)
{
	if (u_lightParams.x < 0.5f)
	{
		return vec3_splat(0.0f);
	}
	float side = u_lightParams.w;
	vec2 inside = clamp(world.xz / u_lightParams.z, vec2_splat(0.5f), vec2_splat(side - 0.5f)) / side;
	vec4 r = texture2DLod(s_lightProbes, vec2(inside.x / 3.0f, inside.y), 0.0f);
	vec4 g = texture2DLod(s_lightProbes, vec2((inside.x + 1.0f) / 3.0f, inside.y), 0.0f);
	vec4 b = texture2DLod(s_lightProbes, vec2((inside.x + 2.0f) / 3.0f, inside.y), 0.0f);
	return LightIrradiance(r, g, b, n);
}

// What the shade's light makes of a pixel's colour (global_illumination::ShadeTint): its ratio to the open sky's light,
// with some of its colour taken out, held between the darkest and the brightest shade
vec3 LightShadeTint(vec3 light)
{
	vec3 luma = vec3(0.2126f, 0.7152f, 0.0722f);
	float reference = max(dot(u_lightOpenSky.rgb, luma), 1e-5f);
	vec3 ratio = max(light / reference, vec3_splat(0.0f));
	float grey = dot(ratio, luma);
	vec3 tint = mix(vec3_splat(grey), ratio, u_lightOpenSky.w);
	float held = clamp(grey, LIGHT_DARKEST, LIGHT_BRIGHTEST);
	return grey > 1e-5f ? tint * (held / grey) : vec3_splat(LIGHT_DARKEST);
}

#endif // LIGHTING_SH
