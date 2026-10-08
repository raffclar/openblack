$input v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>

// The sky dome's three textures, one layer per alignment: the day / dusk / night blend is
// already in them, built on the CPU by sky_type::DomeBlend
SAMPLER2DARRAY(s_diffuse, 0);
uniform vec4 u_typeAlignment; // x: unused (the sky type is in the textures), y: alignment 0 evil .. 2 good

void main()
{
	// constants
	const float evilIndex = 0.0f;
	const float goodIndex = 2.0f;

	// unpack uniform
	float alignment = clamp(u_typeAlignment.y, evilIndex, goodIndex);

	// How the original mixes the alignments is not read yet (inferred): openblack's linear mix of the two nearest
	float alignT = mod(alignment, 1.0f);
	float alignA = alignment - alignT;
	float alignB = min(alignA + 1.0f, goodIndex);

	vec4 colorA = texture2DArray(s_diffuse, vec3(v_texcoord0.xy, alignA));
	vec4 colorB = texture2DArray(s_diffuse, vec3(v_texcoord0.xy, alignB));

	gl_FragColor = mix(colorA, colorB, alignT);
	gl_FragColor.a = 1.0f;
}
