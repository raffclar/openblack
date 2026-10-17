#ifndef FOG_SH
#define FOG_SH

// EXPERIMENT (not for merge): the volumetric fog's grid. The grid follows the view: its cells lie across and down the
// screen, and its slices go into the view, farther apart far away. Each grid lies in one texture, its slices in tiles
// (see volumetric_fog in src/Graphics/VolumetricFog.h, whose formulas these match).

// x: the haze's density at the sea's level, y: the height over which the haze thins, z: the ground fog's density,
// w: the top of the ground fog over the sea
uniform vec4 u_fogLayers;
// xy: how far the wind has moved the mist; z: the seconds for the mist's slow change; w: the frame's number
uniform vec4 u_fogDrift;
// xyz: the direction towards the sun or the moon; w: how much of the light is the moon's
uniform vec4 u_fogLight;
// rgb: the light's colour and strength; w: 1 while there is light
uniform vec4 u_fogLightColour;
// rgb: the light of the sky in the fog; w: the sea's level
uniform vec4 u_fogAmbient;
// x: the cells across, y: the cells down, z: the slices, w: the farthest distance
uniform vec4 u_fogGrid;

#define FOG_TILES_ACROSS 8.0f
#define FOG_NEAREST 3.0f

// The distance of a slice from the eye; the middle of slice i is at i
float FogSliceDistance(float slice)
{
	return FOG_NEAREST * exp(log(u_fogGrid.w / FOG_NEAREST) * (slice + 0.5f) / u_fogGrid.z);
}

// The slice at a distance, as a fraction
float FogSliceAt(float distance)
{
	return log(max(distance, 1e-3f) / FOG_NEAREST) / log(u_fogGrid.w / FOG_NEAREST) * u_fogGrid.z - 0.5f;
}

// Where a point of the screen (0 to 1) is in a slice's tile of a grid's texture, kept half a texel inside the tile
vec2 FogTileUv(vec2 screenUv, float slice)
{
	float rows = u_fogGrid.z / FOG_TILES_ACROSS;
	vec2 tile = vec2(mod(slice, FOG_TILES_ACROSS), floor(slice / FOG_TILES_ACROSS));
	vec2 cells = u_fogGrid.xy;
	vec2 inside = clamp(screenUv, 0.5f / cells, 1.0f - 0.5f / cells);
	return (tile + inside) / vec2(FOG_TILES_ACROSS, rows);
}

#endif // FOG_SH
