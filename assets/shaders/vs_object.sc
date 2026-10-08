#if defined(USE_INSTANCING) && defined(USE_MORPH)
$input a_position, a_texcoord0, a_normal, a_indices, a_tangent, a_bitangent, a_texcoord1, a_texcoord2, a_color1, a_weight, i_data0, i_data1, i_data2, i_data3, i_data4
#elif defined(USE_INSTANCING)
$input a_position, a_texcoord0, a_normal, a_indices, i_data0, i_data1, i_data2, i_data3, i_data4
#else
$input a_position, a_texcoord0, a_normal, a_indices
#endif // USE_INSTANCING
$output v_position, v_texcoord0, v_normal, v_color0

// The *_static variants define 1: every draw copies the whole u_model array into the backend's per-frame uniform
// scratch buffer (8 MB with Vulkan), so 128 bones for every static mesh overflowed it on the bigger maps
#ifndef BGFX_CONFIG_MAX_BONES
#if BGFX_SHADER_LANGUAGE_HLSL == 3
#define BGFX_CONFIG_MAX_BONES 48
#else
#define BGFX_CONFIG_MAX_BONES 128
#endif
#endif

#include <bgfx_shader.sh>

#ifdef USE_HEIGHT_MAP
// LandAltitude and LandMelting: the GPU side of src/3D/LandMorph.h
#include "land_altitude.sh"
#endif // USE_HEIGHT_MAP

#ifdef USE_INSTANCING
// ModelLightI / ModelLightFactor / ModelLightDiffuse / ModelLightLocal and u_modelLight: the GPU side of
// src/Graphics/ModelLight.h (the original's one light and its ambient = 90)
#include "model_light.sh"
// MultiplyShift8 / AddSaturated / UnpackRgb24: the GPU side of src/Graphics/ArgbColour.h (LH3DColor's byte arithmetic
// and the instance's colour column)
#include "argb_colour.sh"

// Model lighting of the original (the land light, then the model light): the object takes the landscape light of the ground it
// stands on, table[cell luminosity] interpolated bilinearly over the 4 cells around its origin, and the cells' r, g, b
// as specular; each vertex is then lit by the renderer's one point light with the integer rule of model_light.sh.
SAMPLER2D(s_landLightTable, 3); // landscape light table, 256x1
SAMPLER2D(s_landCells, 4);      // this frame's cells (land_light::Texels): rgb = the colour as a D3DCOLOR, a = luminosity
#include "land_light.sh"
#include "haze.sh"
uniform vec4 u_objectLight; // x > 0: light like the original, y: colour boost (the hand: x1.5, as the original's hand),
                            // w: 1 = no distance haze (the hand) + 2 x the mesh's land_light::ObjectMode
uniform vec4 u_window;      // x > 0: a window submesh (L3D isWindow), lit at night by the instance (the house's draw)
                            // w: 1 = the primitive takes the object's texture offset

#endif // USE_INSTANCING
#ifdef USE_MORPH
// A creature's body: how far it is pulled towards its evil or good, thin or fat and weak or strong mesh, whose
// positions and normals come in the second to fourth vertex streams (vs_object_morph_instanced.sc)
uniform vec4 u_morphWeights;
#endif // USE_MORPH
// u_objectClip and SeaUnmirror (y: drawn back unmirrored in the reflection target): the GPU side of
// src/Graphics/SeaPass.h, in both branches (the sky writes 0)
#include "sea_plane.sh"

void main()
{
	// Unpack
#if BGFX_SHADER_LANGUAGE_HLSL > 300 || BGFX_SHADER_LANGUAGE_PSSL || BGFX_SHADER_LANGUAGE_SPIRV
	uint modelIndex = uint(max(0, asint(a_indices.x)));
#else
	uint modelIndex = uint(max(0, a_indices.x));
#endif
	modelIndex = min(modelIndex, uint(BGFX_CONFIG_MAX_BONES - 1));

#ifdef USE_MORPH
	// The base mesh moved towards each mesh by its weight, its normal alike and made unit length again
	vec3 morphPosition = a_position.xyz + u_morphWeights.x * (a_tangent - a_position.xyz) +
	                     u_morphWeights.y * (a_texcoord1 - a_position.xyz) + u_morphWeights.z * (a_color1.xyz - a_position.xyz);
	vec3 morphNormal = normalize(a_normal.xyz + u_morphWeights.x * (a_bitangent - a_normal.xyz) +
	                             u_morphWeights.y * (a_texcoord2 - a_normal.xyz) +
	                             u_morphWeights.z * (a_weight.xyz - a_normal.xyz));
#define VERTEX_POSITION morphPosition
#define VERTEX_NORMAL morphNormal
#else
// Every other mesh: the names expand to the attributes themselves, so its shaders are the same as before
#define VERTEX_POSITION a_position.xyz
#define VERTEX_NORMAL a_normal.xyz
#endif // USE_MORPH

	v_position = mul(u_model[modelIndex], vec4(VERTEX_POSITION, 1.0f));
	// Normals follow the bone / model rotation and then the instance rotation (uniform scales only, renormalised)
	vec3 normal = mul(u_model[modelIndex], vec4(VERTEX_NORMAL, 0.0f)).xyz;

#ifdef USE_INSTANCING
	// The w of the first column carries 1 - opacity for fading meshes (0 for the others).
	float fade = i_data0.w;
	mat4 model;
	model[0] = vec4(i_data0.xyz, 0.0f);
	model[1] = vec4(i_data1.xyz, 0.0f);
	model[2] = vec4(i_data2.xyz, 0.0f);
	model[3] = vec4(i_data3.xyz, 1.0f);
	// The fifth column, the object's colour fields (argb_colour::PackInstance*, src/Graphics/ArgbColour.h), each
	// an rgb 0xRRGGBB below 2^24:
	// x, the object colour: 0 the land light alone; < 0: -1 - a tint t that multiplies the land light (the fields,
	// the white tint 0xFFFFFFFF, the poison, the charring grey; or the trees' own, see w); > 0: 1 + the colour set
	// directly instead of the land light (the power-up bands, the PSys mesh atoms)
	// y, the specular: 8 bits a channel (a living's own, the poison's, the fire's glow, the bands' 0x141414)
	// z: 0, or 1 + the house's window colour at night
	// w: 1 = the tint after the haze (the trees: the haze, then their brightness)
	vec3 drawColour = i_data4.x < -0.5f ? UnpackRgb24(-i_data4.x - 1.0f) : vec3_splat(-1.0f);
	bool setColour = i_data4.x > 0.5f;
	vec3 setColour255 = setColour ? UnpackRgb24(i_data4.x - 1.0f) : vec3_splat(0.0f);
	vec3 objectSpecular255 = UnpackRgb24(i_data4.y);
	bool tintAfterHaze = i_data4.w > 0.5f;
	bool windowLit = i_data4.z > 0.5f;
	vec3 windowColour = windowLit ? UnpackRgb24(i_data4.z - 1.0f) / 255.0f : vec3_splat(0.0f);

	v_position = instMul(model, v_position);
	normal = instMul(model, vec4(normal, 0.0f)).xyz;

	// The one light of the original in the space the vertex lives in, for the model light branches below (the inverse
	// object matrix for the rigid meshes; the boned path inverts each bone matrix over the
	// light already in camera space, which is the light per bone only if those matrices go from the bone to
	// the camera, so that the camera cancels out (inferred)). It is taken from the ORIGIN of the bone (or of the object)
	// and meets the raw local normal, so a non-uniform scale (a tree's sway, a field's shear) gives the light of the
	// original and not that of a rotated normal. The axes come out of mul / instMul instead of u_model[i][k]: with HLSL
	// that indexes a row of the maths matrix, not the axis, because bgfx packs its matrices column major (the compiler
	// folds the unit vectors away). Only the vertex-lit cases need it: mode 1 and the PSys mesh atoms of modes 1 and 3;
	// the cut (mode 4) takes its own light below.
	vec3 lightLocal = vec3_splat(0.0f);
	if (u_objectLight.x > 0.0f &&
	    (u_objectLight.x < 1.5f || (u_objectLight.x > 2.5f && u_objectLight.x < 3.5f && setColour)))
	{
		vec3 lightAxisX = instMul(model, vec4(mul(u_model[modelIndex], vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz, 0.0f)).xyz;
		vec3 lightAxisY = instMul(model, vec4(mul(u_model[modelIndex], vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz, 0.0f)).xyz;
		vec3 lightAxisZ = instMul(model, vec4(mul(u_model[modelIndex], vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz, 0.0f)).xyz;
		vec3 lightOrigin = instMul(model, mul(u_model[modelIndex], vec4(0.0f, 0.0f, 0.0f, 1.0f))).xyz;
		lightLocal = ModelLightLocal(lightAxisX, lightAxisY, lightAxisZ, lightOrigin, u_modelLight.xyz);
	}
	float lightAmbient = u_modelLight.w;
#endif // USE_INSTANCING

#ifdef USE_HEIGHT_MAP
	// Morphing with the land (the melting, land_altitude.sh): every vertex is raised along the
	// object's local Y by the land's height under it minus the height under the object's origin
#ifdef USE_INSTANCING
	// (the scale: column 0 is the rotation x the uniform scale; the field and tree sway only touch column 1)
	v_position.xyz = LandMelting(v_position.xyz, i_data3.xz, i_data1.xyz, length(i_data0.xyz));
#else
	v_position.xyz = LandMelting(v_position.xyz, u_model[modelIndex][3].xz, u_model[modelIndex][1].xyz,
	                             length(u_model[modelIndex][0].xyz));
#endif // USE_INSTANCING
#endif // USE_HEIGHT_MAP

	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
#ifdef USE_INSTANCING
	// The w of the second column carries the object's texture offset (components::UvScroll, SetUVOffset): V (food piles,
	// the Land 3 waterfall) + 4 x U in 1/256 steps (the one-shot orbs' 4x4 animation, the PSys AnimTextured meshes),
	// added (U and V) only to the primitives whose material lacks the flag bit 0x10 (u_window.w,
	// as the original's triangle draw). V may be -2..2: the waterfall's is -1..0 (frac of a decreasing V), the piles'
	// and orbs' 0..1
	float uSteps = floor((i_data1.w + 2.0f) / 4.0f);
	v_texcoord0.x += uSteps / 256.0f * u_window.w;
	v_texcoord0.y += (i_data1.w - uSteps * 4.0f) * u_window.w;
#endif // USE_INSTANCING
	vec3 specular = vec3_splat(0.0f);
#ifdef USE_INSTANCING
	vec3 objectColour = vec3_splat(1.0f);
	if (u_objectLight.x > 3.5f)
	{
		// Cut by the sea plane (per vertex): the same model light rule with the
		// light in the OBJECT's space: both callers set it once from the object's
		// matrix (static and animated alike), and the cut reads it in both its branches (rigid
		// and boned), using the bone matrices only for the positions. So a boned
		// mesh is lit with the object's light, not per bone: the instance matrix alone, without u_model. rgb =
		// colour.rgb (the object's set colour, u_objectLight.z) x f >> 8, no land light, no haze, + the
		// object's specular (u_objectLight.w, sea_pass::SeaDraw::specular). z < 0: each instance's own
		// colour / specular from the fifth column (sea_pass::CutAtoms: the PSys mesh atoms' draw data colours, set by
		// the colour setter before the cut draw)
		bool cutOwnColour = u_objectLight.z < -0.5f;
		vec3 cutLight = ModelLightLocal(i_data0.xyz, i_data1.xyz, i_data2.xyz, i_data3.xyz, u_modelLight.xyz);
		vec3 cutColour = cutOwnColour ? setColour255 : UnpackRgb24(u_objectLight.z);
		float cutFactor = ModelLightFactor(ModelLightI(VERTEX_NORMAL, cutLight, false), lightAmbient);
		objectColour = ModelLightDiffuse(cutColour, cutFactor) / 255.0f;
		specular = (cutOwnColour ? objectSpecular255 : UnpackRgb24(u_objectLight.w)) / 255.0f;
	}
	else if (u_objectLight.x > 1.5f && u_objectLight.x < 2.5f)
	{
		// The underwater draw in a constant colour (sea_pass::SeaLight::Constant): the object colour packed
		// r 65536 + g 256 + b (the hand's 0xA0A0A0, the boat's 0x303070), no vertex light, + the object's specular (w)
		objectColour = UnpackRgb24(u_objectLight.z) / 255.0f;
		specular = UnpackRgb24(u_objectLight.w) / 255.0f;
	}
	else if (u_objectLight.x > 0.0f)
	{
		// The land light at the origin by the mesh's mode (land_light.sh, land_light::ObjectMode): bilinear,
		// cell >> 8, the cell alone, table[255] alone (the dove writes only its colour;
		// (inferred) its specular left at 0, nothing in the dove's draw sets it)
		float landMode = floor(u_objectLight.w / 2.0f);
		bool hazeOff = u_objectLight.w - landMode * 2.0f > 0.5f;
		vec3 landSpecular = vec3_splat(0.0f);
		vec3 landDiffuse = LandLightFull();
		if (landMode < 0.5f)
		{
			landDiffuse = LandLightBilinear(i_data3.xz, landSpecular);
		}
		else if (landMode < 1.5f)
		{
			landDiffuse = LandLightCellShift(i_data3.xz, landSpecular);
		}
		else if (landMode < 2.5f)
		{
			landDiffuse = LandLightCell(i_data3.xz, landSpecular);
		}
		objectColour = landDiffuse / 255.0f;
		// + the object's own specular, per channel with saturation (before the haze)
		specular = AddSaturated(landSpecular, objectSpecular255) / 255.0f;
		objectColour = min(objectColour * u_objectLight.y, vec3_splat(1.0f));
		// the tint multiplies the land light byte by byte, (c t) >> 8: the white
		// 0xFFFFFFFF takes 1 off each channel
		if (drawColour.r >= 0.0f && !tintAfterHaze)
		{
			objectColour = MultiplyShift8(floor(objectColour * 255.0f + 0.5f), drawColour) / 255.0f;
		}
		// x = 3: only that colour and specular, as the land light leaves them in the object for
		// the underwater draw (reflections: no haze, no vertex lighting)
		if (u_objectLight.x < 2.5f)
		{
		// Distance haze once per object at its origin (haze.sh): none with the key off or closer than near;
		// otherwise the colour (c f) >> 8 per byte, the specular + the rounded haze colour, saturated
		float originDepth = mul(u_view, vec4(i_data3.xyz, 1.0f)).z;
		if (u_haze.w > 0.0f && !hazeOff && !(originDepth < u_haze.x))
		{
			float hazeT = HazeT(originDepth);
			objectColour = ApplyHazeDiffuse(floor(objectColour * 255.0f + 0.5f), HazeFactor(hazeT)) / 255.0f;
			specular = HazeAddSaturated(floor(specular * 255.0f + 0.5f), HazeColour(hazeT)) / 255.0f;
		}
		}
		// The trees' tint, the same (c t) >> 8 over the hazed colour; the
		// specular is left as the haze made it
		if (drawColour.r >= 0.0f && tintAfterHaze)
		{
			objectColour = MultiplyShift8(floor(objectColour * 255.0f + 0.5f), drawColour) / 255.0f;
		}
		if (u_objectLight.x < 2.5f)
		{
			// The vertex light (model_light.sh) over the object's byte colour, so it is at most 254/256
			float factor = ModelLightFactor(ModelLightI(VERTEX_NORMAL, lightLocal, false), lightAmbient);
			objectColour = ModelLightDiffuse(floor(objectColour * 255.0f + 0.5f), factor) / 255.0f;
		}
		// Windows at night: unlit, the flat window colour instead of the land light, the specular kept
		if (u_window.x > 0.0f && windowLit)
		{
			objectColour = windowColour;
		}
	}
	// The colour set directly on the object (colour and specular) instead of the land light
	// and without the object haze: a PSys mesh atom's draw data colour (the atom's own draw,
	// Particles/Creators/Mesh.h) and the power-up bands (components::ObjectColour). The model light stays: the object is an
	// object that draws like every other model, with the same model light (inferred: the draw that follows
	// the atom's draw is not read yet). Not in the cut (mode 4), which lights that colour itself
	if (setColour && u_objectLight.x > 0.0f && (u_objectLight.x < 1.5f || (u_objectLight.x > 2.5f && u_objectLight.x < 3.5f)))
	{
		objectColour = setColour255 / 255.0f;
		float factor = ModelLightFactor(ModelLightI(VERTEX_NORMAL, lightLocal, false), lightAmbient);
		objectColour = ModelLightDiffuse(setColour255, factor) / 255.0f;
		specular = objectSpecular255 / 255.0f;
	}
	float opacity = 1.0f - fade;
	if (u_objectLight.x > 3.5f)
	{
		opacity *= u_objectLight.y; // A = colour A, as in the cut
	}
	v_color0 = vec4(objectColour, opacity);
#else
	v_color0 = vec4(1.0f, 1.0f, 1.0f, 1.0f);
#endif // USE_INSTANCING
	v_normal = normal; // not normalised here: the sky mesh has zero normals
	// The specular colour rides in the unused texcoord z/w and position w: vs_object is shared with the sky (fs_sky)
	// and a new varying broke its interface
	// fs_object clips on the real position; only the drawn one is mirrored back
	gl_Position = mul(u_viewProj, SeaUnmirror(v_position));
#ifdef USE_INSTANCING
	// Window submeshes exist only while the house's windows are lit (by day they fail the LOD test)
	if (u_window.x > 0.0f && !windowLit)
	{
		gl_Position = vec4(2.0f, 2.0f, 2.0f, 1.0f);
	}
#endif // USE_INSTANCING
	v_texcoord0.zw = specular.rg;
	v_position.w = specular.b;
}
