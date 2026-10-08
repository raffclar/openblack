$input a_position, a_texcoord1, a_color1, a_color2, a_texcoord2, a_color0, a_color3, a_normal
$output v_normal, v_texcoord0, v_texcoord1, v_weight, v_materialID0, v_materialID1, v_materialBlend, v_lightLevel, v_shoreFade, v_distToCamera, v_smallBumpFade, v_landLight, v_landSpecular, v_worldXZ

#include <bgfx_shader.sh>

#if BGFX_SHADER_LANGUAGE_HLSL > 300 || BGFX_SHADER_LANGUAGE_SPIRV
#   define materialIdFix(x) (floatBitsToInt(x))
#else
#   define materialIdFix(x) (ivec4(x))
#endif

SAMPLER2D(s_landLightTable, 4); // the landscape light table, 256 x 1
SAMPLER2D(s_landCells, 6);      // this frame's cells (land_light::Texels): the light and shadow stamps are in them
#include "land_light.sh"
#include "haze.sh"
#include "land_position.sh" // the same depth as vs_land_shadow, which redraws the block (LEQUAL)

uniform vec4 u_blockPositionAndSize;
uniform vec4 u_islandExtent;
uniform vec4 u_skyAndBump;    // w: distance of the small bump fade line ahead of the camera
uniform vec4 u_smallBumpLine; // xy: camera x/z, zw: normalised horizontal camera forward
uniform vec4 u_hazeBlock;     // x: the block's haze class (graphics::haze::BlockClass): 0, 1, 2

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
	v_texcoord1 = vec4(blockStartUv, 0.0f, 0.0f);
	v_weight = a_texcoord1;
	v_materialID0 = materialIdFix(a_color1);
	v_materialID1 = materialIdFix(a_color2);
	v_materialBlend = a_texcoord2;
	v_lightLevel = a_color0.x;
	// Vertex diffuse = table[cell byte 3] and specular = the cell colour, the cells of this frame
	// (land_light.sh), interpolated across the triangle like D3D
	vec2 cellIndex = floor((blockPosition + a_position.xz - u_islandExtent.xy) * 0.1f + 0.5f);
	vec4 landCell = LandCell(cellIndex);
	vec3 landDiffuse = LandTable(landCell.a);
	vec3 landSpecular = landCell.rgb;
	v_shoreFade = a_color3; // 0 at altitude 1 or less: no small bump there, dynamic shadows fade out

	vec3 transformedPosition = LandWorldPosition(a_position.xyz, blockPosition);

	// Small bump pass vertex diffuse (the same in both of the original's code paths), d the
	// signed distance to the fade line (e = 20, ramp 40): d >= e -> specular alpha | 0xFFFFFF (white, alpha
	// 255, or 0 at altitude 1 or less); -e < d < e -> alpha round(255 - (e - d) * 255 / 40) << 24 | 0xFFFFFF if the
	// specular alpha is set, else 0 (black, alpha 0); d <= -e -> 0. So the detail fades out (Gouraud) towards every
	// vertex at altitude 1 or less: no hard edge where the shallow water meets the open sea cells.
	float forwardDistance = dot(transformedPosition.xz - u_smallBumpLine.xy, u_smallBumpLine.zw);
	float lineOffset = u_skyAndBump.w - forwardDistance;
	if (lineOffset >= 20.0f)
	{
		v_smallBumpFade = vec2(1.0f, a_color3);
	}
	else if (lineOffset > -20.0f && a_color3 > 0.5f)
	{
		v_smallBumpFade = vec2(1.0f, floor(255.0f - (20.0f - lineOffset) * (255.0f / 40.0f) + 0.5f) / 255.0f);
	}
	else
	{
		v_smallBumpFade = vec2(0.0f, 0.0f);
	}

	v_normal = a_normal;
	v_worldXZ = transformedPosition.xz;
	vec4 cs_position = LandViewPosition(transformedPosition);
	v_distToCamera = cs_position.z;

	// Distance haze per vertex (haze.sh) by the block's class: 2 f = k and the colour
	// truncated, 1 from the view depth clamped to [near, far]; the specular first (saturated), then
	// the diffuse (c f) >> 8
	if (u_hazeBlock.x > 1.5f)
	{
		landSpecular = HazeAddSaturated(landSpecular, HazeColourFull());
		landDiffuse = ApplyHazeDiffuse(landDiffuse, u_haze.z);
	}
	else if (u_hazeBlock.x > 0.5f)
	{
		float hazeT = HazeT(cs_position.z);
		landSpecular = HazeAddSaturated(landSpecular, HazeColour(hazeT));
		landDiffuse = ApplyHazeDiffuse(landDiffuse, HazeFactor(hazeT));
	}
	v_landLight = landDiffuse / 255.0f;
	v_landSpecular = landSpecular / 255.0f;
	gl_Position = LandClipPosition(cs_position);
}
