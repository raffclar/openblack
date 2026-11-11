$input a_position, a_texcoord0
$output v_texcoord0, v_texcoord1, v_lightColour, v_smallBumpFade, v_shadowCoord, v_haze, v_world

#include <bgfx_shader.sh>

#include "haze.sh"

uniform vec4 u_blockPositionAndSize;
uniform vec4 u_islandExtent;
// The land's luminosity this frame, a texel for each cell's corner
SAMPLER2D(s9_landLuminosity, 9);
// The colour of each cell corner this frame, which is added to the land's light, red and blue as the game reads them
SAMPLER2D(s10_landColour, 10);
// The land's light: a colour for each level of the cells' luminosity (LandLightTable)
SAMPLER2D(s7_landLight, 7);
// y: how bright the land's light is, a half for the land mirrored under the sea
uniform vec4 u_skyAndBump;
// xy: the camera's x and z, zw: the direction it faces along the ground
uniform vec4 u_smallBumpLine;
// x: how far ahead of the camera along the ground the small bump detail fades out
uniform vec4 u_smallBump;
// The corners of the blocks that morph or cross-fade, as the game works them out on the CPU this frame (see
// land_detail::CornerTexel): a square of 17 by 17 texels for each block, 16 squares along each side. r: the altitude at
// which the corner is drawn while its block morphs, g: how opaque the coarse texture is at it while its block
// cross-fades.
SAMPLER2D(s8_landCorners, 8);
// x: the block's layer of the block textures, y: how its corners morph (0 not, 1 to the half mesh, 2 to the quarter
// mesh), z: its texture (0 full, 1 cross-faded, 2 coarse)
uniform vec4 u_block;

void main()
{
	// Unpack
	vec2 blockPosition = u_blockPositionAndSize.xy;
	vec2 blockSize = u_blockPositionAndSize.zw;
	vec2 extentMin = u_islandExtent.xy;
	vec2 extentMax = u_islandExtent.zw;

	// The corner as it is drawn this frame, when its block morphs or cross-fades
	vec2 corner = floor(a_position.xz / 10.0f + 0.5f);
	vec2 square = vec2(mod(u_block.x, 16.0f), floor(u_block.x / 16.0f)) * 17.0f;
	vec2 drawn = floor(texture2DLod(s8_landCorners, (square + corner + 0.5f) / 272.0f, 0.0f).rg * 255.0f + 0.5f);
	// z: how opaque the block's coarse texture is over its full one at the corner: clear before the texture line's band,
	// opaque beyond it, in 255ths across it
	float coarseFade = u_block.z > 0.5f && u_block.z < 1.5f ? drawn.y / 255.0f : 1.0f;
	v_texcoord0 = vec4(a_position.zx / blockSize.yx, coarseFade, 0.0f);
	vec2 blockStartUv = (blockPosition + a_position.xz - extentMin) / (extentMax - extentMin);
	#if !BGFX_SHADER_LANGUAGE_GLSL
		blockStartUv.y = 1.0f - blockStartUv.y;
	#endif
	// A block a mesh line runs through morphs its corners towards the coarser mesh across the line's band. Land at sea
	// level, altitude 3 or below, is drawn flat at height 0.
	float altitude = a_texcoord0.x;
	float height = a_position.y;
	if (u_block.y > 0.5f)
	{
		altitude = drawn.x;
		height = altitude <= 3.0f ? 0.0f : altitude * 0.67f;
	}
	vec3 transformedPosition = vec3(a_position.x + blockPosition.x, height, a_position.z + blockPosition.y);
	// zw: where the vertex is along x and z, for the snow lying there
	v_texcoord1 = vec4(blockStartUv, transformedPosition.xz);

	// The luminosity of the vertex's cell corner this frame
	vec2 luminosityTexels = (extentMax - extentMin) / 10.0f + 1.0f;
	vec2 luminosityCell = floor((transformedPosition.xz - extentMin) / 10.0f + 0.5f);
	float luminosity =
	    floor(texture2DLod(s9_landLuminosity, (luminosityCell + 0.5f) / luminosityTexels, 0.0f).r * 255.0f + 0.5f);
	vec3 light = texture2DLod(s7_landLight, vec2((luminosity + 0.5f) / 256.0f, 0.5f), 0.0f).rgb;

	v_shadowCoord = vec4_splat(0.0f);
	v_world = transformedPosition;
	// The game gives the land's vertices below altitude 2 no alpha, so shadows fade out towards the water's edge. It goes
	// by the corner's own altitude: the land of altitudes 2 and 3 is drawn flat at sea level but keeps them.
	float aboveWater = altitude >= 2.0f ? 1.0f : 0.0f;
	v_shadowCoord.w = aboveWater;

	vec4 cs_position = mul(u_view, vec4(transformedPosition, 1.0f));
	// The distance haze, per vertex
	float hazeT = HazeT(cs_position.z);
	// Each vertex takes the colour of its luminosity. The game scales each channel at the vertex and rounds it down: first
	// by the brightness of the land, then by the haze. The land blends the vertex colours between the vertices.
	v_lightColour = HazeDiffuse(floor(light * 255.0f * u_skyAndBump.y + 0.001f), HazeFactor(hazeT)) / 255.0f;
	// The haze is added with the colour of the vertex's cell corner, each channel at most white
	vec3 cellColour =
	    floor(texture2DLod(s10_landColour, (luminosityCell + 0.5f) / luminosityTexels, 0.0f).rgb * 255.0f + 0.5f);
	v_haze = vec4(min(HazeColour(hazeT) + cellColour, vec3_splat(255.0f)) / 255.0f, 1.0f);

	// The small bump detail is drawn over the land near the camera. It is full up to 20 units before a line across the
	// ground ahead of the camera, and gone 20 units past it. It is never drawn at the water's edge, below altitude 2,
	// and fades out towards it from the vertices around. x: the detail's colour is kept, or black; y: its alpha.
	float lineOffset = u_smallBump.x - dot(transformedPosition.xz - u_smallBumpLine.xy, u_smallBumpLine.zw);
	if (lineOffset >= 20.0f)
	{
		v_smallBumpFade = vec2(1.0f, aboveWater);
	}
	else if (lineOffset > -20.0f && aboveWater > 0.0f)
	{
		v_smallBumpFade = vec2(1.0f, floor(255.0f - (20.0f - lineOffset) * (255.0f / 40.0f) + 0.5f) / 255.0f);
	}
	else
	{
		v_smallBumpFade = vec2(0.0f, 0.0f);
	}
	// Land at sea level is drawn flat at height 0, in the plane of the ocean. Land is never under the sea, so win the
	// tie: pull those vertices a fraction of their distance towards the camera along their line of sight, which keeps
	// them where they are on screen and only brings their depth forward.
	if (height <= 0.0f)
	{
		cs_position.xyz *= 0.999f;
	}
	gl_Position = mul(u_proj, cs_position);
}
