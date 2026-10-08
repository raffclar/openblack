/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>

/// The original's materials and render modes: one material of 16 bytes and one of 19 mode functions (the normal table,
/// or its twin for the objects drawn with their own alpha) set every blending, alpha test, Z write and texture stage
/// state of a draw (wiki: docs/bw1-notes/rendering-objects.md, render modes and materials). Every openblack draw that
/// stands for an original one asks State() for its bgfx state.
///
/// What is the mode's and what is the draw's:
/// - the mode (the table): ALPHABLENDENABLE, SRCBLEND / DESTBLEND, ALPHATESTENABLE, ALPHAREF, ZWRITEENABLE and stage 0
///   (colour = TEXTURE x DIFFUSE; alpha = TEXTURE x DIFFUSE or TEXTURE). ALPHAFUNC is GREATEREQUAL for every mode, set
///   once.
/// - the material setup inlined at every draw (about 105 copies): CULLMODE from the material's two-sided flag, tiling
///   from its tiling flag or a global tiling switch.
/// - written by hand around a draw, never by a mode: ZFUNC (LESSEQUAL 4 globally, ALWAYS 8, EQUAL 3) and a few
///   ZWRITEENABLE 0 (the 2D rectangles, the sea, the mirrored land). StateOptions carries them, with what openblack's
///   own targets add (alpha written, MSAA, the primitive type).
namespace openblack::graphics::render_modes
{

/// The render mode, the index of the mode tables. The L3D material type is the same number
/// (l3d::L3DMaterial::Type), so the names follow the L3D ones; 14 and 17 have no L3D type.
enum class Mode : uint8_t
{
	Smooth = 0,
	SmoothAlpha = 1,
	Textured = 2,
	TexturedAlpha = 3,
	AlphaTextured = 4,
	AlphaTexturedAlpha = 5,
	AlphaTexturedAlphaNoZWrite = 6,
	SmoothAlphaNoZWrite = 7,
	TexturedAlphaNoZWrite = 8,
	TexturedChroma = 9,
	AlphaTexturedAlphaAdditiveChroma = 10,
	AlphaTexturedAlphaAdditiveChromaNoZWrite = 11,
	AlphaTexturedAlphaAdditive = 12,
	AlphaTexturedAlphaAdditiveNoZWrite = 13,
	Landscape = 14, ///< the land blocks: the function of mode 5
	TexturedChromaAlpha = 15,
	TexturedChromaAlphaNoZWrite = 16,
	TexturedUnused = 17, ///< the function of mode 2; no CreateMaterial caller and no L3D type
	ChromaDepthOnly = 18,
};
inline constexpr uint32_t k_ModeCount = 19;

/// ALPHABLENDENABLE and SRCBLEND / DESTBLEND of a mode (D3D7 values: 1 ZERO, 2 ONE, 5 SRCALPHA, 6 INVSRCALPHA)
enum class Blend : uint8_t
{
	Disabled, ///< ALPHABLENDENABLE 0
	Standard, ///< SRCALPHA / INVSRCALPHA
	Additive, ///< SRCALPHA / ONE
	JustZ,    ///< ZERO / ONE: no colour is written, only Z where the alpha test passes (mode 18)
};

/// The D3D states one mode function sets (stage 0; with stage != 0 a mode only binds its texture, and the six alpha
/// tested modes their ALPHAREF too)
struct ModeDesc
{
	uint8_t function;   ///< the mode whose function this is in the normal table (14 and 17 share 5's and 2's)
	Blend blend;        ///< ALPHABLENDENABLE, SRCBLEND, DESTBLEND
	bool alphaTest;     ///< ALPHATESTENABLE, ALPHAREF = the material's AlphaRef
	bool zWrite;        ///< ZWRITEENABLE, compared on every call, outside the mode cache
	bool alphaModulate; ///< ALPHAOP MODULATE(TEXTURE, DIFFUSE); else SELECTARG1(TEXTURE), or no texture
	bool textured;      ///< binds the material's texture; 0, 1 and 7 clear the stage (no texture)
};

/// The normal table (19 {function, word} pairs and a 0; the word, 1 for the blended modes, has no reader), as read in
/// the mode functions (2, 6, 9, 10, 11, 16 and 18 checked again). The functions' addresses are in the wiki
/// (docs/bw1-notes/rendering-objects.md)
// clang-format off
inline constexpr std::array<ModeDesc, k_ModeCount> k_Modes = {{
	{ 0, Blend::Disabled, false, true,  false, false}, // 0
	{ 1, Blend::Standard, false, true,  false, false}, // 1: stage 0 left as it was
	{ 2, Blend::Disabled, false, true,  false, true},  // 2: ABLEND = ATEST = 0
	{ 3, Blend::Standard, false, true,  true,  true},  // 3
	{ 4, Blend::Standard, false, true,  false, true},  // 4
	{ 5, Blend::Standard, false, true,  true,  true},  // 5
	{ 6, Blend::Standard, false, false, true,  true},  // 6: ZWRITE 0
	{ 7, Blend::Standard, false, false, false, false}, // 7: ZWRITE 0
	{ 8, Blend::Standard, false, false, true,  true},  // 8: ZWRITE 0
	{ 9, Blend::Standard, true,  true,  false, true},  // 9
	{10, Blend::Additive, true,  true,  true,  true},  // 10: SA/ONE, ATEST, Z
	{11, Blend::Additive, true,  false, true,  true},  // 11: SA/ONE, ATEST, ZWRITE 0
	{12, Blend::Additive, false, true,  true,  true},  // 12
	{13, Blend::Additive, false, false, true,  true},  // 13
	{ 5, Blend::Standard, false, true,  true,  true},  // 14 = 5
	{15, Blend::Standard, true,  true,  true,  true},  // 15
	{16, Blend::Standard, true,  false, true,  true},  // 16: ATEST, ZWRITE 0
	{ 2, Blend::Disabled, false, true,  false, true},  // 17 = 2
	{18, Blend::JustZ,    true,  true,  false, true},  // 18: only Z, where the alpha test passes
}};
// clang-format on

/// Which mode table the material setup calls through
enum class Table : uint8_t
{
	Normal,      ///< the normal table, set when the renderer opens
	GlobalAlpha, ///< an object with its own alpha (its global alpha flag, set by the object draw)
};

/// The global alpha table: the opaque modes become their blended twin (alpha = texture x diffuse), the others stay
// clang-format off
inline constexpr std::array<Mode, k_ModeCount> k_GlobalAlphaModes = {{
	Mode::SmoothAlpha, Mode::SmoothAlpha,                // 0, 1 -> 1
	Mode::TexturedAlpha, Mode::TexturedAlpha,            // 2, 3 -> 3
	Mode::AlphaTexturedAlpha, Mode::AlphaTexturedAlpha,  // 4, 5 -> 5
	Mode::AlphaTexturedAlphaNoZWrite, Mode::SmoothAlphaNoZWrite, Mode::TexturedAlphaNoZWrite,
	Mode::TexturedChromaAlpha,                           // 9 -> 15
	Mode::AlphaTexturedAlphaAdditiveChroma, Mode::AlphaTexturedAlphaAdditiveChromaNoZWrite, Mode::AlphaTexturedAlphaAdditive,
	Mode::AlphaTexturedAlphaAdditiveNoZWrite, Mode::Landscape, Mode::TexturedChromaAlpha, Mode::TexturedChromaAlphaNoZWrite,
	Mode::TexturedAlpha,                                 // 17 -> 3
	Mode::ChromaDepthOnly,
}};
// clang-format on

[[nodiscard]] constexpr const ModeDesc& Desc(Mode mode)
{
	return k_Modes[static_cast<uint8_t>(mode)];
}

/// The mode whose function the material setup calls: the selected table's entry for the material's mode
[[nodiscard]] constexpr Mode Select(Mode mode, Table table)
{
	return table == Table::GlobalAlpha ? k_GlobalAlphaModes[static_cast<uint8_t>(mode)] : mode;
}

/// D3DRS_ZFUNC around a draw (written by hand, never by a mode)
enum class ZFunc : uint8_t
{
	LessEqual,          ///< 4, the global one. openblack's inverted Z: BGFX_STATE_DEPTH_TEST_GREATER (approximate:
	                    ///< strict, as every openblack pass)
	Equal,              ///< 3: the shadows on the objects, the object fade
	Always,             ///< 8: drawn over everything (the frame's end, the 2D rectangles, the sea, text, the sun's glare)
	LessEqualInclusive, ///< 4 (D3DCMP_LESSEQUAL) where the equal depth must pass: a redraw over what was just drawn
	                    ///< (the shadows over the land blocks, and over the animated and morphable objects).
	                    ///< Inverted Z: GEQUAL
};

/// D3DRS_CULLMODE: the material setup puts ((~flags) & 1) * 2 + 1, 1 = NONE, 3 = CCW
enum class Cull : uint8_t
{
	None, ///< D3DCULL_NONE
	Ccw,  ///< D3DCULL_CCW = bgfx's CCW in openblack's view
	Cw,   ///< the CCW of the original seen through a mirroring camera (the reflection pass)
};

/// The culling of a material: none if two-sided, else CCW, flipped when the camera mirrors
[[nodiscard]] constexpr Cull CullFor(bool twoSided, bool mirrored)
{
	return twoSided ? Cull::None : (mirrored ? Cull::Cw : Cull::Ccw);
}

/// What a draw adds to its mode's states
struct StateOptions
{
	ZFunc zFunc {ZFunc::LessEqual};
	Cull cull {Cull::None};
	/// false: ZWRITEENABLE 0 written by hand after the mode
	bool zWrite {true};
	/// openblack's targets: the colour's alpha is written too (the original's back buffer has none, (inferred))
	bool writeAlpha {false};
	bool msaa {false};
	/// SRCALPHA / INVSRCALPHA as ONE / INVSRCALPHA: the shader premultiplies the colour by its alpha after sampling,
	/// which gives the same colour (docs/bw1-notes/rendering-objects.md, mode 6); only the destination alpha differs
	bool premultiplied {false};
	/// openblack's sea: fs_water composes the reflection itself, so the mode's blending is left out
	/// (docs/bw1-notes/water.md)
	bool blendInShader {false};
	/// bgfx bits that are not the original's states (the primitive type: BGFX_STATE_PT_LINES)
	uint64_t extra {0};
};

/// The bgfx state of a draw in a mode: the mode function's blending and Z write, then the draw's options
[[nodiscard]] uint64_t State(Mode mode, const StateOptions& options = {});

/// One L3D primitive in a model draw (Renderer::DrawSubMesh): State() of its mode (already through the table), without
/// the target's alpha for openblack's back-to-front primitives (`sorted`)
[[nodiscard]] uint64_t PrimitiveState(Mode mode, StateOptions options, bool sorted);

/// The passes of L3D models (objects, reflections, the hand): opaque modes write colour, alpha and Z, with MSAA
inline constexpr StateOptions k_ModelPass {.writeAlpha = true, .msaa = true};

/// ALPHAREF of a draw in the alpha tested modes (9, 10, 11, 15, 16, 18): the global override if it is on (`forced`),
/// else the material's AlphaRef. With the global alpha table, modes 9 and 15 scale it by the object's alpha:
/// max(0, ref * A * (1 / 255) - 5 truncated toward zero), A = the alpha byte of the object's diffuse. 0 for the modes without
/// alpha test.
[[nodiscard]] uint8_t AlphaRef(Mode selected, Table table, uint8_t materialRef, std::optional<uint8_t> forced = std::nullopt,
                               uint8_t globalAlpha = 255);

/// The object's alpha byte A from openblack's 0..1 opacity (components::Alpha) (inferred: rounded to the nearest)
[[nodiscard]] uint8_t AlphaByte(float opacity);

/// The alpha of stage 0 that fs_object tests and writes for one primitive (u_skyAlphaThreshold.y and .w)
enum class AlphaSource : uint8_t
{
	None,     ///< 0: the mode neither blends nor tests (0, 2, 17): the texture's alpha is not used, alpha 1
	Texture,  ///< 1: SELECTARG1(TEXTURE) (4, 9, 18), or no texture (1, 7)
	Modulate, ///< 2: MODULATE(TEXTURE, DIFFUSE) (mode 15): the alpha test sees texture x object alpha
};
struct ShaderAlpha
{
	float ref;          ///< ALPHAREF / 255 of the alpha tested modes (GREATEREQUAL), -1 without the test
	AlphaSource source; ///< the alpha the stage outputs
};

/// One primitive in the mode it is drawn in (already through the table, or forced): its alpha test and stage 0 alpha
[[nodiscard]] ShaderAlpha PrimitiveAlpha(Mode drawn, Table table, uint8_t materialRef, uint8_t globalAlpha = 255);

/// The material properties a shared mesh can be loaded with
struct MaterialProperties
{
	bool additive;    ///< the additive mode 13 (SRCALPHA / ONE)
	bool zWrite;      ///< the Z-writing variant (6 -> 5, 13 -> 12, 8 -> 3, 16 -> 9), else the one without Z
	bool doubleSided; ///< the material's two-sided flag set (D3DCULL_NONE), else cleared (back faces culled)
	bool change;      ///< the shared mesh loader applies the properties to the mesh it loads
	bool alpha;       ///< false makes every type TexturedAlpha (3) before the rules above
};

/// The new mode of a material under the properties (the double-sided bit is the caller's)
[[nodiscard]] Mode ModeFromProperties(Mode mode, const MaterialProperties& properties);

/// The states of a material (the mode, ALPHAREF, flags, the texture and a colour; ALPHAREF and flags are 0 as created)
/// that its draws use. The texture is the draw's: one texture is shared by several materials (smoke.raw by modes 6
/// and 13)
struct Material
{
	Mode mode;
	uint8_t flags {0};    ///< bit 0 two-sided, bit 2 tiling, bit 4 no texture offset
	uint8_t alphaRef {0}; ///< ALPHAREF of the alpha tested modes

	[[nodiscard]] constexpr bool TwoSided() const { return (flags & 1u) != 0; }
};

inline constexpr uint8_t k_TwoSided = 1u; ///< flags bit 0
inline constexpr uint8_t k_Tiling = 4u;   ///< flags bit 2

/// State() of a draw through the material setup of a material: its mode, and without a cull in the options the
/// material's (two-sided or CCW), flipped when the camera mirrors
[[nodiscard]] uint64_t State(const Material& material, StateOptions options = {}, bool mirrored = false);

/// The alpha test of a draw through a material in the normal table: ALPHAREF / 255 in the alpha tested modes (a texel
/// is kept when its alpha byte is at least ALPHAREF), -1 in the others (PrimitiveAlpha's ref)
[[nodiscard]] float AlphaTest(const Material& material);

/// The named materials that openblack's draws stand for (each made once by the original)
namespace materials
{
/// smoke.raw, mode 6, two-sided. The chimney smoke, the mists and clouds, the boats' wake and the smoke puffs
inline constexpr Material k_Smoke {Mode::AlphaTexturedAlphaNoZWrite, k_TwoSided};
/// The same smoke.raw, mode 13, two-sided. The water rings ((inferred): the reference is not decoded inside the
/// routine)
inline constexpr Material k_SmokeAdditive {Mode::AlphaTexturedAlphaAdditiveNoZWrite, k_TwoSided};
/// misc0.raw, mode 6, two-sided. The fish farm shoals ((inferred) likewise)
inline constexpr Material k_Misc0 {Mode::AlphaTexturedAlphaNoZWrite, k_TwoSided};
/// Made once by PLAY_JC_SPECIAL 0 and freed with the intro: misc0.raw, mode 13, one-sided. The intro light
/// (ecs/IntroSpecial.h)
inline constexpr Material k_Misc0Additive {Mode::AlphaTexturedAlphaAdditiveNoZWrite, 0};
/// The atmosphere material: atmos.raw, mode 6, two-sided and tiling. The rain streaks
inline constexpr Material k_Atmos {Mode::AlphaTexturedAlphaNoZWrite, k_TwoSided | k_Tiling};
/// The atmosphere's additive material: the same atmos.raw, mode 13, two-sided. The hand's glow on the sea
inline constexpr Material k_AtmosAdditive {Mode::AlphaTexturedAlphaAdditiveNoZWrite, k_TwoSided};
/// Made on the influence circle's first draw: burn.raw, mode 6, two-sided and tiling, ALPHAREF 0; the no-offset bit
/// clear, so the triangle draw adds the UV offset. The influence border. Not the other material made from the same
/// texture (mode 6, tiling only)
inline constexpr Material k_InfluenceCircle {Mode::AlphaTexturedAlphaNoZWrite, k_TwoSided | k_Tiling};
/// The creature hair's material, made once when the first creature loads: c_ape_hair.raw, mode 9 (blended and alpha
/// tested, writing depth), ALPHAREF 50. (approximate) two-sided: the game's is one-sided (CCW), but the ribbons are
/// built facing the camera in their own winding
inline constexpr Material k_CreatureHair {Mode::TexturedChroma, k_TwoSided, 50};
/// An untextured hair group's: the triangle draw's default material, mode 0 (opaque, writing depth). (approximate)
/// two-sided, as above
inline constexpr Material k_CreatureHairPlain {Mode::Smooth, k_TwoSided};
} // namespace materials

} // namespace openblack::graphics::render_modes
