$input v_world, v_texcoord0, v_color0, v_normal

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the light of a jellyfish, added to the scene. The outer shell is a clear hood that
// glows at its rim; the inner shell has bands of light in pink to orange that move slowly; the tentacles are faint,
// and their tips glow as the bell beats. They glow more at night and deeper down. The water's fog over them comes
// after, in the water's pass, from the depth that they write.

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
	float glow = (0.35f + 0.65f * night) * (0.45f + 0.55f * smoothstep(3.0f, 25.0f, depth));
	// The colour of its light, between pink and orange, that changes slowly
	vec3 accent = mix(vec3(1.0f, 0.32f, 0.62f), vec3(1.0f, 0.56f, 0.16f), 0.5f + 0.5f * sin(t * 0.25f + hue * 6.283f));
	vec3 hood = vec3(0.55f, 0.78f, 0.92f);

	vec3 colour;
	if (part < 0.5f)
	{
		// The hood: clear in its middle, bright at its rim
		float rim = pow(1.0f - abs(dot(normalize(v_normal), view)), 3.0f);
		colour = hood * (0.05f + 0.45f * rim) * (0.5f * daylight + 0.5f * glow) + accent * rim * 0.45f * glow;
	}
	else if (part < 1.5f)
	{
		// The bands inside the bell: rings that move down it, in four lobes round it
		float bands = smoothstep(0.55f, 1.0f, 0.5f + 0.5f * sin(down * 22.0f - t * 1.5f + hue * 5.0f));
		float lobes = 0.55f + 0.45f * cos(along * 6.283f * 4.0f);
		float under = smoothstep(0.08f, 0.4f, down);
		colour = accent * bands * lobes * under * glow * (0.7f + 0.6f * squeeze);
	}
	else
	{
		// A tentacle: faint along it, its tip lit by the beat; the thin outer ones fainter
		float tip = smoothstep(0.6f, 1.0f, along) * squeeze;
		float edge = 1.0f - abs(down);
		float strength = (part < 2.5f ? 1.0f : 0.7f) * (0.12f * (1.0f - 0.5f * along) + 1.3f * tip);
		colour = mix(hood, accent, 0.4f + 0.5f * tip) * strength * glow * (0.4f + 0.6f * edge);
	}
	gl_FragColor = vec4(colour * shows, 0.0f);
}
