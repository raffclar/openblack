$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): a leaf or a twig that drifts on the water (vs_underwater_drift.sc), lit by the sun or the
// moon and the sky

// rgb: the colour of the sky
uniform vec4 u_uwSky;

void main()
{
	vec2 corner = v_texcoord0.xy;
	float kind = v_texcoord0.z;
	float shape;
	vec3 colour;
	if (kind < 0.7f)
	{
		// A leaf: pointed at both ends, a darker vein down its middle, green to yellow to brown
		float half_ = 0.95f * pow(max(1.0f - corner.x * corner.x, 0.0f), 0.7f);
		shape = 1.0f - smoothstep(half_ - 0.12f, half_, abs(corner.y));
		float vein = 1.0f - smoothstep(0.0f, 0.08f, abs(corner.y)) * 0.25f;
		float shade = kind / 0.7f;
		vec3 green = vec3(0.24f, 0.36f, 0.10f);
		vec3 yellow = vec3(0.55f, 0.45f, 0.12f);
		vec3 brown = vec3(0.33f, 0.20f, 0.09f);
		colour = (shade < 0.5f ? mix(green, yellow, shade * 2.0f) : mix(yellow, brown, shade * 2.0f - 1.0f)) * (2.0f - vein);
	}
	else
	{
		// A twig: a thin rod with knots
		shape = (1.0f - smoothstep(0.55f, 0.9f, abs(corner.y))) * (1.0f - smoothstep(0.92f, 1.0f, abs(corner.x)));
		colour = vec3(0.30f, 0.22f, 0.14f) * (0.8f + 0.4f * UwNoise(vec2(corner.x * 9.0f, kind * 50.0f)));
	}
	float cover = shape * v_texcoord0.w;
	if (cover < 0.01f)
	{
		discard;
	}
	vec3 light = u_uwLightColour.rgb * (0.35f + 0.65f * max(u_uwAbove.y, 0.0f)) + u_uwSky.rgb * 0.6f;
	gl_FragColor = vec4(colour * light * cover, cover);
}
