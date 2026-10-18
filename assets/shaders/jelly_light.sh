#ifndef JELLY_LIGHT_SH
#define JELLY_LIGHT_SH

// EXPERIMENT (not for merge): the light of the jellyfish nearest to the eye (see underwater::jellyfish::LightAt). Each
// is a small blue-green light in the water. Its light spreads out as the square of the distance, from the size of the
// bell, and the water absorbs it and scatters it out of its way with the coefficients of the rest of the water. Thus
// it makes the water, the sea bed, the motes and the other jellyfish bright for about half a metre to a metre round
// it. The file that includes it includes underwater.sh first.

// The most lights (underwater::jellyfish::k_Lights)
#define JELLY_LIGHTS 16
// The water that scatters their light out of its straight way, in each unit (underwater::jellyfish::k_GlowScatter)
#define JELLY_GLOW_SCATTER 0.2f
// Past this distance from a jellyfish, its light is less than one step of an 8-bit colour
// (underwater::jellyfish::k_LightReach)
#define JELLY_LIGHT_REACH 5.0f
// The soft pool of light that the water scatters onto the sea bed: where it starts to fade, and where it stops
// (underwater::jellyfish::k_PoolRadius, k_PoolReach)
#define JELLY_POOL_RADIUS 1.0f
#define JELLY_POOL_REACH 8.0f
// How much brighter the pool is than the scattered light alone (k_PoolStrength), and how much of it there is at rest
// and how much more at the full squeeze of the bell (k_PoolPulseRest, k_PoolPulse)
#define JELLY_POOL_STRENGTH 8.0f
#define JELLY_POOL_PULSE_REST 0.6f
#define JELLY_POOL_PULSE 0.8f

// Two vectors for each light: xyz where it is and w how much its bell squeezes, 0 to 1; then rgb its colour times how
// bright it is
uniform vec4 u_jellyLights[2 * JELLY_LIGHTS];
// x: how many lights there are (0 when there are none); y: the highest point that their light comes to; z: 1 / the
// square of the radius of their light; w: the highest point that their pools of light on the sea bed come to
uniform vec4 u_jellyLightInfo;

// The part of each colour of their light that comes through a length of water
// (underwater::jellyfish::GlowThroughWater)
vec3 JellyGlowThroughWater(float path)
{
	return exp(-(UW_ABSORPTION + vec3_splat(JELLY_GLOW_SCATTER)) * max(path, 0.0f));
}

// The light of the jellyfish that comes to a point. `normal` is the normal of a surface there; `wrap` is 0 for a
// surface, which gets light only on the side that faces a light, and 1 for a small thing in the water, which gets
// light from all sides. `skip` is the number of a light to leave out (the jellyfish's own light), or -1.
vec3 JellyLightAt(vec3 p, vec3 normal, float wrap, float skip)
{
	vec3 sum = vec3_splat(0.0f);
	if (u_jellyLightInfo.x < 0.5f || p.y > u_jellyLightInfo.y)
	{
		return sum;
	}
	for (int i = 0; i < JELLY_LIGHTS; i++)
	{
		if (float(i) >= u_jellyLightInfo.x)
		{
			break;
		}
		vec3 towards = u_jellyLights[2 * i].xyz - p;
		float d2 = dot(towards, towards);
		if (d2 < JELLY_LIGHT_REACH * JELLY_LIGHT_REACH && abs(float(i) - skip) > 0.5f)
		{
			float d = sqrt(d2);
			float facing = mix(max(dot(normal, towards / max(d, 1e-4f)), 0.0f), 1.0f, wrap);
			sum += u_jellyLights[2 * i + 1].rgb * JellyGlowThroughWater(d) * (facing / (d2 * u_jellyLightInfo.z + 1.0f));
		}
	}
	return sum;
}

// The soft pool of the jellyfish's light that the water scatters onto the sea bed at a point
// (underwater::jellyfish::PoolAt, PoolPulse). This light comes from all of the water round a jellyfish, so the side of
// the bed that does not face it gets half. The pool pulses with the squeeze of the bell.
vec3 JellyPoolAt(vec3 p, vec3 normal)
{
	vec3 sum = vec3_splat(0.0f);
	if (u_jellyLightInfo.x < 0.5f || p.y > u_jellyLightInfo.w)
	{
		return sum;
	}
	for (int i = 0; i < JELLY_LIGHTS; i++)
	{
		if (float(i) >= u_jellyLightInfo.x)
		{
			break;
		}
		vec3 towards = u_jellyLights[2 * i].xyz - p;
		float d2 = dot(towards, towards);
		if (d2 < JELLY_POOL_REACH * JELLY_POOL_REACH)
		{
			float d = sqrt(d2);
			float facing = mix(max(dot(normal, towards / max(d, 1e-4f)), 0.0f), 1.0f, 0.5f);
			float share = d / JELLY_POOL_REACH;
			float edge = 1.0f - share * share * share * share;
			vec3 left = exp(-UW_ABSORPTION * d) *
			            (JELLY_POOL_STRENGTH * JELLY_GLOW_SCATTER * JELLY_POOL_RADIUS / (d + JELLY_POOL_RADIUS));
			float pulse = JELLY_POOL_PULSE_REST + JELLY_POOL_PULSE * u_jellyLights[2 * i].w;
			sum += u_jellyLights[2 * i + 1].rgb * left * (facing * edge * edge * pulse);
		}
	}
	return sum;
}

#endif // JELLY_LIGHT_SH
