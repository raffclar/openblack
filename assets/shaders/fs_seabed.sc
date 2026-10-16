$input v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the sea bed past the land's blocks. It is the same ground as the sea bed on the blocks,
// with the same texture, light and normal at each point, so the two meet with no line.

SAMPLER2D(s_sand, 0);
// The land's light: a colour for each level of the cells' luminosity, as the land reads it
SAMPLER2D(s_landLight, 1);
// The shore map (see the vertex shader)
SAMPLER2D(s_shore, 2);
// The land's block textures: a layer holds the ground under the sea, painted with the land's own material
SAMPLER2DARRAY(s_floor, 3);

#define SEABED_SAND_TEXTURE s_sand
#define SEABED_FLOOR_TEXTURE s_floor
#define SEABED_SHORE_TEXTURE s_shore
#include "seabed_sand.sh"

void main()
{
	// The land's light at a middle level of luminosity, as the sea's cells near the coast have it
	vec3 landLight = texture2DLod(s_landLight, vec2(150.5f / 256.0f, 0.5f), 0.0f).rgb;
	vec3 normal = SeabedNormal(v_world);
	float sandShare;
	gl_FragColor = vec4(SeabedGround(v_world, SeabedFloorAlbedo(v_world.xz), landLight, normal, sandShare), 1.0f);
}
