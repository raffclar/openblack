$input v_world, v_texcoord0, v_color0, v_normal

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the reef's coral and plants, lit by the light under the water and its caustics

// The coral atlas: nine corals, each grown by an L-system, with their alpha
SAMPLER2D(s_coral, 0);
// The land's light: a colour for each level of the cells' luminosity, as the land reads it
SAMPLER2D(s_landLight, 1);

void main()
{
	bool plant = v_texcoord0.w > 0.5f;
	vec3 albedo = v_color0.rgb;
	if (!plant)
	{
		vec4 coral = texture2D(s_coral, v_texcoord0.xy);
		if (coral.a < 0.5f)
		{
			discard;
		}
		albedo *= coral.rgb * 1.15f;
	}
	else
	{
		// Darker at the foot of a plant, and lighter at its edges
		albedo *= 0.55f + 0.45f * v_texcoord0.z;
	}
	vec3 landLight = texture2DLod(s_landLight, vec2(150.5f / 256.0f, 0.5f), 0.0f).rgb;
	float depth = max(u_uwParams.z - v_world.y, 0.0f);
	vec3 towards = u_uwLight.xyz;
	// Lit from either side, as the quads and strips have no back
	float facing = 0.35f + 0.65f * abs(dot(normalize(v_normal), towards));
	vec2 surface = v_world.xz + towards.xz / max(towards.y, 0.2f) * depth;
	float sharpness = 1.0f - smoothstep(4.0f, 40.0f, depth);
	float caustics = u_uwAbove.w > 0.5f ? UwCaustics(surface * UW_CAUSTIC_FREQUENCY, u_uwParams.y * 1.4f, sharpness) : 0.5f;
	caustics = mix(caustics, 0.55f, 0.4f * u_uwWaves.y);
	vec3 light = u_uwLightColour.rgb * UwTransmittance(depth * 0.6f / max(towards.y, 0.2f));
	vec3 colour = albedo * (landLight * 0.7f + light * facing * (0.6f + caustics));
	gl_FragColor = vec4(colour, 1.0f);
}
