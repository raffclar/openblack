$input v_world, v_normal

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the sea bed past the land's blocks, of the same sand as the sea bed on them

SAMPLER2D(s_sand, 0);
// The land's light: a colour for each level of the cells' luminosity, as the land reads it
SAMPLER2D(s_landLight, 1);

#define SEABED_SAND_TEXTURE s_sand
#include "seabed_sand.sh"

void main()
{
	// The land's light at a middle level of luminosity, as the sea's cells near the coast have it
	vec3 landLight = texture2DLod(s_landLight, vec2(150.5f / 256.0f, 0.5f), 0.0f).rgb;
	vec3 normal = normalize(v_normal);
	gl_FragColor = vec4(SeabedSand(v_world, landLight, normal), 1.0f);
}
