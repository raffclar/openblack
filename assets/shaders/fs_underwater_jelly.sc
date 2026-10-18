$input v_world, v_texcoord0, v_texcoord1, v_color0, v_normal

#include <bgfx_shader.sh>

#include "underwater.sh"
#include "jelly_light.sh"

// EXPERIMENT (not for merge): a jellyfish, in two passes (u_uwEye.w):
// - 0, in the scene: the clear hood lit by the daylight and by the light of the jellyfish near it, and the far water's
//   colour behind it, as much as the jellyfish covers. The water's pass then sees the jellyfish as near and puts its
//   fog over it from its own distance.
// - 1, after the water's pass: its own light, added over the water. The light is blue-green, as the light of deep sea
//   animals. It is dim at rest and pulses with the beat of the bell. Small waves of light go round the rim of the bell
//   and down the tentacles, faster and brighter in a flash. No colour of it is brighter than
//   underwater::jellyfish::k_MostGlow. The water absorbs it and scatters it out of its way to the eye
//   (underwater::jellyfish::GlowThroughWater), so it fades faster than the light of the scene. They are deep, so no
//   light comes to the surface. The scene in front of the jellyfish hides the light.
// - The halo (part 4, in the second pass only): the water in a ball of half a metre round the bell scatters some of the
//   light to the eye, most at its middle and none at its edge. The shader adds up the light along the part of the ray
//   from the eye that is in the ball and in front of the scene.

SAMPLER2D(s_sceneDepth, 0);

// The brightest colour of their light, the radius of the halo and its strength (underwater::jellyfish::k_MostGlow,
// k_LightRadius and k_HaloStrength)
#define JELLY_MOST_GLOW 0.8f
#define JELLY_LIGHT_RADIUS (0.5f * UW_UNITS_PER_METRE)
#define JELLY_HALO_STRENGTH 0.35f
// The colour of their light, blue-green and a little more green, and the most that it moves to the green
// (underwater::jellyfish::k_LightColour, k_GreenColour and k_MostGreen)
#define JELLY_LIGHT_COLOUR vec3(0.04f, 0.62f, 1.0f)
#define JELLY_GREEN_COLOUR vec3(0.10f, 0.95f, 0.70f)
#define JELLY_MOST_GREEN 0.4f

// The sum of (1 - r^2 / R^2)^2 along a ray from s0 to s1. s is along the ray from its nearest point to the middle, b2 is
// the square of the distance from the ray to the middle, and R2 is the square of the radius.
float JellyHaloSum(float s0, float s1, float b2, float R2)
{
	float a = 1.0f - b2 / R2;
	float a0 = a * a * s0 - 2.0f * a * s0 * s0 * s0 / (3.0f * R2) + s0 * s0 * s0 * s0 * s0 / (5.0f * R2 * R2);
	float a1 = a * a * s1 - 2.0f * a * s1 * s1 * s1 / (3.0f * R2) + s1 * s1 * s1 * s1 * s1 / (5.0f * R2 * R2);
	return a1 - a0;
}

// Where a point of the screen is in the world, at a depth of the depth buffer
vec3 JellyWorldAt(vec2 uv, float depth)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	vec3 ndc = vec3(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f);
#else
	vec3 ndc = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
#endif
	vec4 p = mul(u_invViewProj, vec4(ndc, 1.0f));
	return p.xyz / p.w;
}

// The length of the way from the eye to a point that goes through the water: all of it with the eye under the water,
// and the part under the level of the sea with the eye above it (underwater::jellyfish::WaterPath)
float JellyWaterPath(vec3 at, float seaLevel)
{
	float distance = length(u_uwEye.xyz - at);
	if (u_uwEye.y <= seaLevel)
	{
		return distance;
	}
	float drop = u_uwEye.y - at.y;
	return drop > 0.0f ? distance * clamp(max(seaLevel - at.y, 0.0f) / drop, 0.0f, 1.0f) : 0.0f;
}

void main()
{
	float along = v_texcoord0.x;
	float down = v_texcoord0.y;
	float part = v_texcoord0.z;
	float hue = v_color0.x;
	float shows = v_color0.y;
	float seed = v_color0.w;
	float flash = v_texcoord1.x;
	float ownLight = v_texcoord1.y;
	float emission = v_texcoord1.z;
	float t = u_uwParams.y;

	vec3 view = normalize(u_uwEye.xyz - v_world);
	float depth = max(u_uwParams.z - v_world.y, 0.0f);
	// The daylight in the water, from the light's colour and how much it shows
	float daylight = clamp(dot(u_uwLightColour.rgb, vec3(0.3f, 0.5f, 0.2f)), 0.0f, 1.0f) * u_uwLightColour.w;
	// The colour of its light (underwater::jellyfish::LightColour)
	vec3 accent = mix(JELLY_LIGHT_COLOUR, JELLY_GREEN_COLOUR, JELLY_MOST_GREEN * hue);
	vec3 hood = vec3(0.55f, 0.78f, 0.92f);
	// A flash makes the waves of light go faster
	float speed = 1.0f + 4.0f * flash;

	// pattern: how much of its light each point sends, about 0 to 1; facing: the hood's normal to the eye
	float pattern = 0.0f;
	float covers = 0.0f;
	float rim = 0.0f;
	if (part > 3.5f)
	{
		// The halo is only in the pass of their own light
		if (u_uwEye.w < 0.5f)
		{
			discard;
		}
	}
	else if (part < 0.5f)
	{
		// The hood: clear in its middle, bright at its edge seen from the side. The rim of the bell glows, and waves of
		// light go round it.
		rim = pow(1.0f - abs(dot(normalize(v_normal), view)), 3.0f);
		float wave = pow(0.5f + 0.5f * sin(along * 6.2831853f * 4.0f - t * 1.3f * speed + seed * 6.0f), 6.0f);
		float edge = smoothstep(0.65f, 0.95f, down);
		pattern = 0.25f + 0.6f * rim + edge * (0.35f + wave * (0.6f + 0.8f * flash));
		covers = 0.55f + 0.4f * rim;
	}
	else if (part < 1.5f)
	{
		// The bands inside the bell: rings that move down it, in four lobes round it
		float bands = smoothstep(0.55f, 1.0f, 0.5f + 0.5f * sin(down * 22.0f - t * 1.5f * speed + hue * 5.0f));
		float lobes = 0.55f + 0.45f * cos(along * 6.2831853f * 4.0f);
		float under = smoothstep(0.08f, 0.4f, down);
		pattern = bands * lobes * under;
		covers = 0.3f;
	}
	else
	{
		// A tentacle: faint along it, with waves of light that go down from the bell to its tip; the thin outer ones are
		// fainter
		float which = v_texcoord1.w;
		float edge = 1.0f - abs(down);
		float wave = pow(0.5f + 0.5f * sin(along * 10.0f - t * 2.5f * speed - seed * 9.0f - which * 1.3f), 8.0f);
		pattern = (part < 2.5f ? 1.0f : 0.7f) * (0.18f * (1.0f - 0.5f * along) + wave * (0.5f + 0.9f * flash)) *
		          (0.4f + 0.6f * edge);
		covers = 0.5f * edge;
	}
	covers *= shows;
	vec3 own = min(accent * emission * pattern, vec3_splat(JELLY_MOST_GLOW));

	if (u_uwEye.w < 0.5f)
	{
		// In the scene: the daylight on it, the light of the other jellyfish near it, and the far water where it covers
		// what is behind it
		vec3 lit = hood * (0.1f + 0.8f * rim) * daylight * UwTransmittance(depth * 0.6f);
		vec3 others = JellyLightAt(v_world, normalize(v_normal), 0.5f, ownLight) * covers;
		gl_FragColor = vec4((lit + hood * others) * shows + u_uwScatter.rgb * covers, covers);
	}
	else
	{
		// After the water's pass: its own light, where the scene in front does not hide it
		vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
		float sceneDepth = texture2D(s_sceneDepth, uv).r;
		float seaLevel = u_uwParams.z;
		// From above, the surface sends back some of the light, more at a low angle
		float through = 1.0f;
		if (u_uwEye.y > seaLevel)
		{
			through = 1.0f - (0.02f + 0.98f * pow(1.0f - clamp(view.y, 0.0f, 1.0f), 5.0f));
		}
		if (part > 3.5f)
		{
			// The halo: the ball of water round the bell (its middle in v_normal)
			vec3 middle = v_normal;
			vec3 ray = normalize(v_world - u_uwEye.xyz);
			vec3 toMiddle = middle - u_uwEye.xyz;
			float nearest = dot(toMiddle, ray);
			float b2 = max(dot(toMiddle, toMiddle) - nearest * nearest, 0.0f);
			float R2 = JELLY_LIGHT_RADIUS * JELLY_LIGHT_RADIUS;
			if (b2 >= R2)
			{
				discard;
			}
			float halfChord = sqrt(R2 - b2);
			float s0 = max(-halfChord, -nearest);
			float s1 = halfChord;
			// The scene in front of the far side of the ball stops the ray (the depth is reversed: 0 is far away)
			if (sceneDepth > 0.0f)
			{
				float sceneAlong = dot(JellyWorldAt(uv, sceneDepth) - u_uwEye.xyz, ray);
				s1 = min(s1, sceneAlong - nearest);
			}
			if (s1 <= s0)
			{
				discard;
			}
			// 1 through the middle of the ball
			float share = JellyHaloSum(s0, s1, b2, R2) / (16.0f / 15.0f * JELLY_LIGHT_RADIUS);
			vec3 halo = accent * emission * JELLY_HALO_STRENGTH * share;
			gl_FragColor = vec4(halo * shows * JellyGlowThroughWater(JellyWaterPath(middle, seaLevel)) * through, 0.0f);
		}
		else
		{
			if (sceneDepth > gl_FragCoord.z)
			{
				discard;
			}
			gl_FragColor = vec4(own * shows * JellyGlowThroughWater(JellyWaterPath(v_world, seaLevel)) * through, 0.0f);
		}
	}
}
