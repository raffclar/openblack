$input v_world, v_texcoord0, v_color0, v_normal

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): a jellyfish, in two passes (u_uwEye.w):
// - 0, in the scene: the clear hood lit by the daylight, and the far water's colour behind it, as much as the
//   jellyfish covers. The water's pass then sees the jellyfish as near and puts its fog over it from its own distance.
// - 1, after the water's pass: its own light, added over the water. The outer shell glows at its rim; the inner shell
//   has bands of light in pink to orange that move slowly; the tentacles are faint, and their tips glow as the bell
//   beats. They glow more at night and deeper down. The water removes some of the light on its way to the eye, but
//   less than of the light that the water's pass changes, and the light keeps most of its colour
//   (underwater::jellyfish::GlowThroughWater). The scene in front of the jellyfish hides the light.

SAMPLER2D(s_sceneDepth, 0);

// The water's colour change that the light of a jellyfish keeps, and the share of the way that takes the light out
// (underwater::jellyfish::k_GlowTint and k_GlowPathShare)
#define JELLY_GLOW_TINT 0.15f
#define JELLY_GLOW_PATH_SHARE 0.5f

void main()
{
	float along = v_texcoord0.x;
	float down = v_texcoord0.y;
	float part = v_texcoord0.z;
	float squeeze = v_texcoord0.w;
	float hue = v_color0.x;
	float shows = v_color0.y;
	float t = u_uwParams.y;

	vec3 view = normalize(u_uwEye.xyz - v_world);
	float depth = max(u_uwParams.z - v_world.y, 0.0f);
	// The daylight in the water, from the light's colour and how much it shows
	float daylight = clamp(dot(u_uwLightColour.rgb, vec3(0.3f, 0.5f, 0.2f)), 0.0f, 1.0f) * u_uwLightColour.w;
	float night = 1.0f - smoothstep(0.05f, 0.6f, daylight);
	// Their own light is strong enough to show through the dark water at night
	float glow = mix(0.6f, 4.0f, night) * (0.45f + 0.55f * smoothstep(3.0f, 25.0f, depth));
	// The colour of its light, between pink and orange, that changes slowly
	vec3 accent = mix(vec3(1.0f, 0.32f, 0.62f), vec3(1.0f, 0.56f, 0.16f), 0.5f + 0.5f * sin(t * 0.25f + hue * 6.283f));
	vec3 hood = vec3(0.55f, 0.78f, 0.92f);

	// lit: the daylight on it; own: its own light
	vec3 lit = vec3_splat(0.0f);
	vec3 own;
	float covers;
	if (part < 0.5f)
	{
		// The hood: clear in its middle, bright at its rim
		float rim = pow(1.0f - abs(dot(normalize(v_normal), view)), 3.0f);
		lit = hood * (0.1f + 0.8f * rim) * daylight;
		own = hood * (0.1f + 0.8f * rim) * 0.3f * glow + accent * rim * 0.45f * glow;
		covers = 0.55f + 0.4f * rim;
	}
	else if (part < 1.5f)
	{
		// The bands inside the bell: rings that move down it, in four lobes round it
		float bands = smoothstep(0.55f, 1.0f, 0.5f + 0.5f * sin(down * 22.0f - t * 1.5f + hue * 5.0f));
		float lobes = 0.55f + 0.45f * cos(along * 6.283f * 4.0f);
		float under = smoothstep(0.08f, 0.4f, down);
		own = accent * bands * lobes * under * glow * (0.7f + 0.6f * squeeze);
		covers = 0.3f;
	}
	else
	{
		// A tentacle: faint along it, its tip lit by the beat; the thin outer ones fainter
		float tip = smoothstep(0.6f, 1.0f, along) * squeeze;
		float edge = 1.0f - abs(down);
		float strength = (part < 2.5f ? 1.0f : 0.7f) * (0.12f * (1.0f - 0.5f * along) + 1.3f * tip);
		own = mix(hood, accent, 0.4f + 0.5f * tip) * strength * glow * (0.4f + 0.6f * edge);
		covers = 0.5f * edge;
	}
	covers *= shows;

	if (u_uwEye.w < 0.5f)
	{
		// In the scene: the daylight on it, and the far water where it covers what is behind it
		gl_FragColor = vec4(lit * shows + u_uwScatter.rgb * covers, covers);
	}
	else
	{
		// After the water's pass: its own light, where the scene in front does not hide it
		vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
		if (texture2D(s_sceneDepth, uv).r > gl_FragCoord.z)
		{
			discard;
		}
		// The way through the water to the eye: all of it under the water, or the part under the sea's level
		float distance = length(u_uwEye.xyz - v_world);
		float seaLevel = u_uwParams.z;
		float drop = u_uwEye.y - v_world.y;
		float path = u_uwEye.y <= seaLevel ? distance : distance * clamp(depth / max(drop, 1e-3f), 0.0f, 1.0f);
		vec3 left = UwTransmittance(path * JELLY_GLOW_PATH_SHARE);
		vec3 kept = mix(vec3_splat(dot(left, vec3(0.2126f, 0.7152f, 0.0722f))), left, JELLY_GLOW_TINT);
		// From above, the surface sends back some of the light, more at a low angle
		float through = 1.0f;
		if (u_uwEye.y > seaLevel)
		{
			through = 1.0f - (0.02f + 0.98f * pow(1.0f - clamp(view.y, 0.0f, 1.0f), 5.0f));
		}
		gl_FragColor = vec4(own * shows * kept * through, 0.0f);
	}
}
