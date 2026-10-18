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

// Two vectors for each light: xyz where it is; then rgb its colour times how bright it is
uniform vec4 u_jellyLights[2 * JELLY_LIGHTS];
// x: how many lights there are (0 when there are none); y: the highest point that their light comes to; z: 1 / the
// square of the radius of their light
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

#endif // JELLY_LIGHT_SH
