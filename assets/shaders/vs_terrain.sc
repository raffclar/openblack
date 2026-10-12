$input a_position, a_texcoord0, a_texcoord1
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
// The land's detail lines (see land_detail): xy the camera's x and z, zw the way it faces along the ground
uniform vec4 u_landLine;
// How far ahead the first and second mesh lines and the texture line lie; w 0 when the camera looks straight down, when
// every corner counts as on the lines
uniform vec4 u_landLineAhead;
// x: how far either side of a mesh line its morph reaches, y: the texture line's cross-fade
uniform vec4 u_landBands;
// x: the block's layer of the block textures, y: how its corners morph (0 not, 1 to the half mesh, 2 to the quarter
// mesh), z: its texture (0 full, 1 cross-faded, 2 coarse)
uniform vec4 u_block;

// How far a point on the ground is before a detail line lying ahead units from the camera; negative beyond it
float LandLineOffset(vec2 xz, float ahead)
{
	return u_landLineAhead.w > 0.5f ? ahead - dot(xz - u_landLine.xy, u_landLine.zw) : 0.0f;
}

// A corner's altitude as it is drawn while it morphs: its own before the band, the target beyond it, and between them
// blended in 256ths, rounded down to a whole altitude
float MorphedAltitude(float own, float target, float offset, float band)
{
	if (offset >= band)
	{
		return own;
	}
	if (offset <= -band)
	{
		return target;
	}
	float share = floor((band - offset) * 256.0f / (2.0f * band) + 0.5f);
	return floor(((256.0f - share) * own + share * target) / 256.0f);
}

void main()
{
	// Unpack
	vec2 blockPosition = u_blockPositionAndSize.xy;
	vec2 blockSize = u_blockPositionAndSize.zw;
	vec2 extentMin = u_islandExtent.xy;
	vec2 extentMax = u_islandExtent.zw;

	v_texcoord0 = vec4(a_position.zx / blockSize.yx, 0.0f, 0.0f);
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
		bool toHalf = u_block.y < 1.5f;
		float offset = LandLineOffset(blockPosition + a_position.xz, toHalf ? u_landLineAhead.x : u_landLineAhead.y);
		altitude = MorphedAltitude(altitude, toHalf ? a_texcoord1.x : a_texcoord1.y, offset, u_landBands.x);
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
	// Each vertex takes the colour of its luminosity, each channel scaled down and rounded down, and the colours are
	// blended across the land between them
	vec3 light = texture2DLod(s7_landLight, vec2((luminosity + 0.5f) / 256.0f, 0.5f), 0.0f).rgb;
	v_lightColour = floor(light * 255.0f * u_skyAndBump.y + 0.001f) / 255.0f;

	v_shadowCoord = vec4_splat(0.0f);
	v_world = transformedPosition;
	// The game gives the land's vertices below altitude 2 no alpha, so shadows fade out towards the water's edge. It goes
	// by the corner's own altitude: the land of altitudes 2 and 3 is drawn flat at sea level but keeps them.
	float aboveWater = altitude >= 2.0f ? 1.0f : 0.0f;
	v_shadowCoord.w = aboveWater;

	vec4 cs_position = mul(u_view, vec4(transformedPosition, 1.0f));
	// The distance haze, per vertex: rgb the haze's colour added, a what the land's light is scaled by
	float hazeT = HazeT(cs_position.z);
	// The haze is added with the colour of the vertex's cell corner, each channel at most white
	vec3 cellColour =
	    floor(texture2DLod(s10_landColour, (luminosityCell + 0.5f) / luminosityTexels, 0.0f).rgb * 255.0f + 0.5f);
	v_haze = vec4(min(HazeColour(hazeT) + cellColour, vec3_splat(255.0f)) / 255.0f, HazeFactor(hazeT) / 256.0f);

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
