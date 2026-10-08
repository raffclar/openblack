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
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <entt/entity/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "3D/Billboard.h"
#include "3D/Clouds.h"
#include "ECS/ChimneySmoke.h"
#include "ECS/Physics/Dust.h"
#include "ECS/Weather/Rain.h"
#include "Graphics/CreatureDraw.h"
#include "Graphics/GameFont.h"
#include "Graphics/Haze.h"
#include "Graphics/LightBeams.h"
#include "Graphics/Mists.h"
#include "Graphics/RenderPass.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/SeaRows.h"
#include "Graphics/UniqueHandle.h"
#include "Graphics/WorldTriangles.h"
#include "Particles/PSysManager.h"
#include "Particles/Rules/SurfRevol.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
struct BgfxCallback;
class LandBlock;
class LandLightPalette;
class LandLightTable;
class Clouds;
class Game;

namespace help
{
enum class TextFont : uint8_t;
}

namespace ecs
{
class Registry;
}

namespace graphics
{
struct BeamMesh;
class FrameBuffer;
class L3DSubMesh;
namespace shadow_list
{
class List;
struct ShadowInfo;
} // namespace shadow_list
class Mesh;
struct SpiritQuadVertex;  // Graphics/OverlayFrame.h
struct IntroLightOverlay; // Graphics/OverlayFrame.h
struct InputPromptFrame;  // Graphics/InputPromptFrame.h

class Renderer final: public RendererInterface
{
	/// Rebuilds the landscape light table for this frame and uploads it (256x1 RGBA8, point sampled)
	void UpdateLandLight() const;
	/// Bakes the static object shadows into the island-wide shadow texture
	void DrawStaticShadowPass(const DrawSceneDesc& drawDesc) const;
	/// The rivers' footprints (ECS/Rivers): river2.l3d into the footprint target (drawRiverBeds) or river.l3d into the
	/// land alpha target with MIN blending
	void DrawRiverFootprints(bgfx::ViewId viewId, bool channel) const;
	void DrawLandAlphaPass(const DrawSceneDesc& drawDesc) const;
	/// The projected shadows of this frame (shadow_list::List::Frame; RendererShadows.cpp)
	void UpdateShadows(const DrawSceneDesc& drawDesc) const;
	/// The projected shadows over one land block just drawn (RendererShadows.cpp)
	void DrawLandShadows(RenderPass viewId, const LandBlock& block, uint64_t cull) const;
	/// The projected shadows that fall on objects too (the hand, the boat): which objects of the main view are in which
	/// shadow's box this frame (_shadowReceivers; RendererShadows.cpp). Cleared, and left empty, for the other views
	void CollectShadowReceivers(bool mainView) const;
	/// The shadows over one object, right after its own draw, as the tail loop of the objects' Draw does: in the main
	/// view for an object drawn at once, in the queue's view inside its Z object for one drawn from the Z-sorter.
	/// `matrices` are the ones the object was drawn with
	void DrawShadowsOnObject(RenderPass viewId, uint32_t instance, const glm::mat4* matrices, uint8_t matrixCount) const;
	/// The shadows over the sharks' parts above the water (`cut`, the instances DrawCutAboveWater has just drawn), which
	/// the per-mesh loop leaves out. The PSys mesh atoms never receive (RenderingSystem's ReceivesDynamicShadow)
	void DrawShadowsOnCutObjects(RenderPass viewId, const std::unordered_set<uint32_t>& cut) const;
	/// The receivers left at the end of the frame's objects were not drawn: none of them gets a shadow
	void ClearShadowReceivers() const { _shadowReceivers.clear(); }
	/// Particle sprites in the order given, consecutive ones with the same material in one call (RendererParticles.cpp): a
	/// Sorted effect's sprite (or a run of them, each its own Z object), or the sprites of a Queued / Immediate effect
	/// between its other items
	void DrawParticleSprites(std::span<const psys::Effect::DrawAtom> atoms, const Camera& camera, RenderPass viewId) const;
	/// (openblack) the ids of "raw/<texture>" and "raw/<texture>a" (entt::hashed_string), made once per name: the PSys
	/// draws asked them per run, building two strings each time
	[[nodiscard]] std::pair<entt::id_type, entt::id_type> RawTextureIds(const std::string& texture) const;
	/// (openblack) the position / uv / colour layout of the PSys world quads, built once
	[[nodiscard]] static const bgfx::VertexLayout& ParticleQuadLayout();
	/// One chain ribbon (RendererChain.cpp): from its own Z object for a Sorted effect (keyed by the joint n / 2), else
	/// inside its effect
	void DrawParticleChain(RenderPass viewId, const Camera& camera, const psys::Effect::DrawChain& chain) const;
	/// One surface of revolution (the teleport pool, the dispensers' discs; RendererRevolvedSurface.cpp), never queued on its
	/// own: at once for a Sorted effect, inside its effect otherwise
	void DrawParticleSurface(RenderPass viewId, const psys::surf_revol::Surface& surface) const;
	/// The end of PreDraw: _frameParticles for the main pass
	void CollectFrameParticles(const DrawSceneDesc& drawDesc) const;
	/// _frameParticles emptied (a frame without PreDraw keeps no earlier frame's lists)
	void ClearFrameParticles() const;
	/// The raining tiles (weather::rain::CollectTiles), each one Z object of its own with its Z-sorter key and its index
	/// in _frameRain (RendererRain.cpp)
	std::vector<std::pair<float, uint32_t>> CollectRain(const Camera& camera) const;
	/// One tile of _frameRain, as the Z-sorter's callback: its streaks
	void DrawRainTile(RenderPass viewId, uint32_t index) const;
	/// The tiles of this frame, filled by CollectRain
	mutable std::vector<weather::rain::Tile> _frameRain;
	/// The sun (right after the sky dome) and its glare (at the end of the frame), eased by the frame's game time step
	/// (DrawClock::frameGameMs)
	void DrawSun(graphics::RenderPass viewId, const Camera& camera, bool glare, uint32_t frameGameMs) const;
	/// The moon and its glow
	/// In the reflection pass (sea_pass::ForPass(viewId).mirrored): the mirrored glow and the moon's DrawUnderWater
	void DrawMoon(graphics::RenderPass viewId, const Camera& camera) const;
	/// The sea: its screen rows, or the level-0 quad (RendererSea.cpp)
	void DrawSea(const DrawSceneDesc& desc) const;
	/// The reflection target follows the main view's size (RendererSea.cpp)
	void UpdateReflectionTarget() const;
	/// The hand's glow on the water at night, into the reflection target (RendererSea.cpp)
	void DrawHandWaterGlow(graphics::RenderPass viewId) const;
	/// The sky clouds are mists: the ones on screen are queued on the Z-sorter. Their Z-sorter keys and indices, and the
	/// counters of those advanced
	std::vector<std::pair<float, uint32_t>> CollectClouds(const Camera& camera) const;
	/// One cloud, as the Z-sorter's callback (the mist draw, effect branch)
	void DrawCloud(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// The mists on screen (the map's and the ones of mists::Submit) with their Z-sorter key and their index in
	/// _frameMists; the map mists' counters advanced (once per frame)
	std::vector<std::pair<float, uint32_t>> CollectMists(const Camera& camera) const;
	/// One mist of _frameMists, as the Z-sorter's callback
	void DrawMist(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// One mist
	void DrawMist(graphics::RenderPass viewId, const Camera& camera, const mists::MistDesc& mist) const;
	/// The mist of a Queued or Immediate effect, drawn at once inside its effect (the screen test, then the draw); not
	/// kept in _frameMists
	void DrawEffectMist(graphics::RenderPass viewId, const Camera& camera, const mists::MistDesc& mist) const;
	/// The mists of this frame, filled by CollectMists
	mutable std::vector<mists::MistDesc> _frameMists;
	/// Every Abode with a chimney on screen: the smoke's state and puffs advanced (the smoke simulates while it draws),
	/// its Z-sorter key and its index in _frameSmoke (RendererSmoke.cpp)
	std::vector<std::pair<float, uint32_t>> CollectChimneySmoke(const Camera& camera) const;
	/// One smoke of _frameSmoke: its visible puffs in their order 0..9 (smoke material)
	void DrawChimneySmoke(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// The puffs of every smoke of this frame, filled by CollectChimneySmoke
	mutable std::vector<std::vector<ecs::chimney_smoke::DrawnPuff>> _frameSmoke;
	/// The mirrored held object and thrown objects in the reflection (DrawUnderWater)
	void DrawObjectReflections(graphics::RenderPass viewId) const;
	/// The missionaries' boat hull in the reflection (DrawUnderWater in colour 0xFF303070), and the boat's sprites: the
	/// wake and the DisappearSmoke puffs, smoke material mode 6 (RendererBoat.cpp)
	void DrawBoatReflection(graphics::RenderPass viewId) const;
	/// Each of the boat's sprites (the wake and the puffs) goes to the Z-sorter on its own: their keys and indices in
	/// _frameBoatSprites
	std::vector<std::pair<float, uint32_t>> CollectBoatSprites(const Camera& camera) const;
	/// One sprite of _frameBoatSprites, as the Z-sorter's callback
	void DrawBoatSprite(graphics::RenderPass viewId, uint32_t index) const;
	/// The intro light's Z object (RendererIntroLight.cpp): the sprites its callback draws
	void DrawIntroLight(graphics::RenderPass viewId, const Camera& camera, const IntroLightOverlay& light) const;
	/// A boat sprite of this frame: its quad (none at or before the near plane) and its colour
	struct BoatSpriteDraw
	{
		std::optional<billboard::Quad> quad;
		uint32_t argb {0};
	};
	mutable std::vector<BoatSpriteDraw> _frameBoatSprites;
	/// The physics' dust puffs of this pass (ecs::physics::Dust::Snapshot), refilled every pass
	mutable std::vector<ecs::physics::DustParticleDraw> _frameDust;
	/// The broken buildings' fragments of this frame (world_triangles), refilled every frame
	mutable world_triangles::Frame _frameFragMeshes;
	/// The sea's scroll offsets and its frame counter (0..15), kept between frames (RendererSea.cpp)
	mutable sea::Drift _seaDrift;
	mutable uint32_t _seaFrame {0};
	/// A 1 x 1 white texture for the surfaces of revolution's specular pass, made on first use
	[[nodiscard]] const Texture2D& RevolvedSurfaceWhiteTexture() const;
	mutable std::unique_ptr<Texture2D> _revolvedSurfaceWhite;
	/// The creatures drawn this frame and what they are drawn with (RendererCreature.cpp, Graphics/CreatureDraw.h):
	/// collected at the start of DrawScene, empty on a land without a creature
	void CollectCreatures() const;
	mutable std::vector<creature_draw::Body> _frameCreatures;
	/// Whether a creature is drawn with any row of [offset, offset + count)
	[[nodiscard]] bool HasCreature(uint32_t offset, uint32_t count) const;
	/// A creature's body: its shape (the morphing program and its targets) and its painted skins
	void UseCreatureBody(const creature_draw::Body& body, L3DMeshSubmitDesc& desc) const;
	/// The second to fourth vertex streams of a creature's submesh: its matches in the meshes the body is blended
	/// towards, the submesh's own where a match differs
	void BindMorphTargets(const L3DMesh& mesh, const L3DSubMesh& subMesh, const creature_draw::MorphTargets& targets) const;
	/// The texture of a primitive's skin: the creature's painted one, else the mesh's (world_triangles::PrimitiveTexture)
	[[nodiscard]] const Texture2D* SkinTexture(const L3DMesh& mesh, uint32_t skinId, entt::entity paintedSkins) const;
	/// A creature's hair and then its eyes, right after its body and the shadows over it
	void DrawCreatureParts(graphics::RenderPass viewId, const Camera& camera, const ecs::systems::RenderContext& context,
	                       const creature_draw::Body& body) const;
	/// Each creature's painted skins, in textures of their own
	mutable creature_draw::PaintedSkins<Texture2D> _creatureSkins;
	/// The layouts a creature's variant meshes are read with as the second to fourth vertex streams, made on first use
	mutable std::array<UniqueHandle<bgfx::VertexLayoutHandle>, 3> _morphStreamLayouts;
	/// The creatures' leashes (RendererLeash.cpp): each worn rope's shadow on the land, then the rope, at once in the world
	/// view, while the scripts show the leashes and outside the temple; nothing without a worn leash
	void DrawLeashes(graphics::RenderPass viewId, const Camera& camera) const;
	/// Every frame of the world view, no option: every circle of influence::Circles() at once, after everything else
	/// drawn at once and before the Z-sorter's drain (RendererInfluence.cpp)
	void DrawInfluenceCircles(graphics::RenderPass viewId) const;
	/// The circles' writes before they draw (PreDraw): the camera gate, the scroll clock and the middle rows' alpha
	/// (influence::SetCurtainAlpha); the scroll goes to _preInfluenceScroll, none when the gate fails
	void UpdateInfluenceCurtain(const Camera& camera) const;
	/// The border's scroll clock (frame_anim::InfluenceScroll), a global of the original
	mutable int32_t _influenceScrollMs {0};
	/// One Z object for every ripple of influence::Ripples(): their Z-sorter keys and indices (RendererInfluence.cpp)
	std::vector<std::pair<float, uint32_t>> CollectInfluenceRipples(const Camera& camera) const;
	/// One ripple, as the Z-sorter's callback: its 7 sprites
	/// (`frameGameMs`: the frame's game time step, DrawClock)
	void DrawInfluenceRipple(graphics::RenderPass viewId, uint32_t index, uint32_t frameGameMs) const;
	/// A mesh drawn under the water: mirrored by the pass's camera, the part kept by sea.plane, in sea's light
	/// (sea_pass::UnderWater / UnderWaterLastDraw) (RendererCut.cpp)
	void DrawUnderWater(graphics::RenderPass viewId, const L3DMesh& mesh, std::unique_ptr<const InstanceDesc> instances,
	                    const glm::mat4* matrices, uint8_t matrixCount, bool morphWithTerrain,
	                    const sea_pass::SeaDraw& sea) const;
	/// DrawUnderWater of an entity's instance (its mesh's bones, or the identity)
	void DrawUnderWater(graphics::RenderPass viewId, entt::entity entity, const sea_pass::SeaDraw& sea) const;
	/// An entity's model cut by the plane (animated or static): the side of y = 0 the plane keeps, lit 90 + N.L in argb
	/// (0xAARRGGBB) + specular; mirrored back in the reflection target (sea_pass::Cut) (RendererCut.cpp)
	void DrawCutByPlane(graphics::RenderPass viewId, entt::entity entity, sea_pass::SeaPlane plane, uint32_t argb,
	                    uint32_t specular) const;
	/// The parts under the water of the objects with components::CutByPlane, before the sea
	void DrawCutBelowWater(graphics::RenderPass viewId) const;
	/// The parts above the water of the objects whose owner draws them cut (components::CutByPlane::drawAbove: the
	/// sharks), in the colour of the land light table[255]; the normal pass skips those instances
	void DrawCutAboveWater(graphics::RenderPass viewId) const;
	/// the instance indices (RenderContext::entityInstances) that DrawCutAboveWater draws instead of the normal pass
	[[nodiscard]] std::unordered_set<uint32_t> CutAboveInstances() const;
	/// The fish farm shoals (before the sea): misc0.raw sprites lying on the water, mode 6; drawn
	/// mirrored into the reflection target, which is what shows through the sea here
	void DrawFishShoals(graphics::RenderPass viewId) const;
	/// The fish puzzle's nets of floats (FishPlot), cut by the plane: KeepBelow the part under the water (into the
	/// reflection target, mirrored back), KeepAbove the part over it (RendererFishPlot.cpp)
	void DrawFishPlots(graphics::RenderPass viewId, sea_pass::SeaPlane plane) const;
	/// Data/Textures/smokea.raw for the chimney smoke (RendererSmoke.cpp)
	bool LoadChimneySmokeAlpha() const;
	/// The water rings (after the landscape): flat smoke.raw sprites, mode 13
	void DrawWaterRings(graphics::RenderPass viewId) const;
	/// The villagers' ground blobs ("human shadow")
	/// `lh`: the draw camera's W and its inverse (billboard::CameraFrame::clipMatrices), for the blobs' exact points
	void DrawHumanShadows(graphics::RenderPass viewId, const graphics::SuperVillagerFrame& superVillagers,
	                      const affine::CameraMatrices& lh) const;
	/// The end of the frame: `drawFade` false, the cinema bars when they are on, before the last finish-frame callbacks
	/// (the film); `drawFade` true, the screen fade after them, which draws the bars again over its colour.
	/// The colour and the bar of the frame's OverlayFrame
	void DrawScreenOverlay(const OverlayFrame& overlay, bool drawFade) const;
	/// DrawKeyOrMouse's `row`: the mouse row (0, 1, 2; 3 no icon) or a key code, or a key's name
	/// (the `wchar*` of a key with clickType 0)
	using KeyOrMouseRow = std::variant<int32_t, std::u16string_view>;
	/// A box of S pixels with the mouse button or the key cap, the text in j0 at 2S / 3, the panels (with alpha8 != 0)
	/// and the arrows (align 0x100..0x800), in the ScreenOverlay view. (x, y) is the
	/// box's left and vertical centre, or an offset from the hand with (align & 6) == 6. A null c1 is white, a null c2
	/// c1; a null text has no width and draws nothing
	void DrawKeyOrMouse(int32_t animType, int32_t clickType, KeyOrMouseRow row, std::optional<std::u16string_view> text,
	                    int32_t x, int32_t y, int32_t s, uint32_t align, const uint32_t* c1, const uint32_t* c2,
	                    int32_t alpha8) const;
	/// DrawKeyOrMouse for a caller outside the input prompt passes (the $m in the «Did you know?» bubble): in `view`, with the
	/// hand's point and the tick count of the frame's copy (OverlayFrame::inputPrompts.hand / tickCount); the input prompt
	/// passes' own state is put back afterwards. The side hysteresis (_keyOrMouseSide) is shared, as the original's one global
	void DrawKeyOrMouseIn(graphics::RenderPass view, const InputPromptFrame& frame, int32_t animType, int32_t clickType,
	                      KeyOrMouseRow row, std::optional<std::u16string_view> text, int32_t x, int32_t y, int32_t s,
	                      uint32_t align, const uint32_t* c1, const uint32_t* c2, int32_t alpha8) const;
	/// The mouse picture of DrawKeyOrMouse, mousehelp.raw
	void DrawKeyOrMouseMouse(int32_t clickType, int32_t row, int32_t animType, int32_t x, int32_t y, int32_t s,
	                         uint32_t argb) const;
	/// One panel, the 9-slice of atmos.raw (drawn in the additive material)
	void AddKeyOrMousePanel(std::vector<SpiritQuadVertex>& out, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
	                        uint32_t argb) const;
	/// The draw half (DrawKeyOrMouse) for every InputPromptIcon of the frame's copy (OverlayFrame::inputPrompts), with its
	/// alpha of `pass`: 0 the call from the help system's 3D draw, 1 the one from HelpText's callback
	void DrawInputPrompts(const OverlayFrame& overlay, size_t pass) const;
	/// The draw's copies of OverlayFrame::inputPrompts' hand point (the hand projected on screen, pixels, y from the top; none
	/// when not projected) and tick count, set by DrawInputPrompts
	mutable std::optional<glm::vec2> _keyOrMouseHand;
	/// The view of the input prompt pass being drawn (pass 0 FinishFrameIcons, pass 1 ScreenOverlay), set by DrawInputPrompts
	mutable graphics::RenderPass _keyOrMouseView {graphics::RenderPass::ScreenOverlay};
	mutable uint32_t _keyOrMouseTicks {0};
	/// The side of DrawKeyOrMouse's align & 2 (1 past 2W / 3, 0 below W / 3)
	mutable int32_t _keyOrMouseSide {0};
	/// The fonts of HelpText: Data\j0, f1 and f3, loaded on first use (f1 / f3 fall back
	/// to j0 when they are missing, as the HelpText ctor does); nullptr when not even j0 loads
	[[nodiscard]] const GameFont* GameFontAt(help::TextFont font) const;
	mutable std::array<std::shared_ptr<const GameFont>, 3> _fonts; ///< from the font cache (Resources::GetFonts)
	mutable std::array<bool, 3> _fontLoadTried {};
	/// Glyph quads (pixels from the top left) in the ScreenOverlay view: the Text program, mode 16, ZFUNC ALWAYS
	void SubmitScreenText(const GameFont& font, const std::vector<GameFont::Vertex>& glyphs,
	                      graphics::RenderPass view = graphics::RenderPass::ScreenOverlay) const;
	/// A pre-transformed rectangle vertex (rhw 1), already in clip space
	struct ScreenRectVertex
	{
		float x, y, z;
		uint32_t abgr;
	};
	/// Two triangles of the pixel rectangle [x0, x1] x [y0, y1] in colour argb
	void AddScreenRect(std::vector<ScreenRectVertex>& out, int x0, int y0, int x1, int y1, uint32_t argb) const;
	/// Two textured triangles of the pixel rectangle [x0, x1] x [y0, y1] (uv u0, v0 .. u1, v1, colour argb), in clip
	/// space for SubmitSpiritQuads in the ScreenOverlay view
	void AddScreenQuad(std::vector<SpiritQuadVertex>& out, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
	                   float v1, uint32_t argb) const;
	/// Untextured rectangles in the ScreenOverlay view, mode 1 (SmoothAlpha), ZFUNC ALWAYS, no Z write
	void SubmitScreenRects(const std::vector<ScreenRectVertex>& vertices) const;
	/// HelpText's draw callback (finish-frame priority 20000: after the bars, before the film and the fade): the box, the
	/// dialogue texts and the click cue (InputPromptIcon), as the OverlayFrame laid them out
	void DrawHelpText(const OverlayFrame& overlay) const;
	/// The did-you-know bubble's shape from OverlayFrame::didYouKnow
	void DrawDidYouKnowBubble(const OverlayFrame& overlay) const;
	/// The full screen film (Video/VideoPlayer.h): one quad per 256x256 tile of the mosaic in material mode 6
	void DrawVideoOverlay() const;
	/// The falling spell's sparks and light bursts over its film (RendererFallingSpell.cpp);
	/// `camera`: the draw's, for the near plane (none: 1)
	void DrawFallingSpellOverlay(const Camera* camera) const;
	/// The advisor spirits (Help/SpiritsRuntime.h, RendererSpirits.cpp): `overlay` false, the ones with the in-world
	/// blend >= 0.5, in the frame, after the Z-sorter's drain; true, the others from the finish-frame callback in the
	/// FinishFrame3D view (after the Z reset). The model, its halo, its puff, as the frame's OverlayFrame holds them
	void DrawSpirits(const Camera& camera, const OverlayFrame& frame, bool overlay) const;
	/// Their rainbow trails, the "before" finish-frame callback (after the Z-sorter's flush, before the Z
	/// reset): at the end of MainBlended
	void DrawSpiritTrails(const Camera& camera, const OverlayFrame& frame) const;
	/// Triangles (three vertices each) of the WorldQuad program with a .raw pair and a bgfx state
	void SubmitSpiritQuads(graphics::RenderPass viewId, const std::vector<SpiritQuadVertex>& vertices, uint32_t texture,
	                       uint32_t alpha, uint64_t state) const;
	/// The end of the frame: the bars, the film, the fade, in that order; in mode 2 (the falling spell) the film, the
	/// sparks, the bursts, the bars, the fade
	/// (`camera`: the draw's, DrawFallingSpellOverlay's near plane)
	void DrawFinishFrameOverlays(const OverlayFrame& overlay, const Camera* camera) const;
	/// The film's picture, (re)made when its size changes and updated when its serial changes
	mutable graphics::UniqueHandle<bgfx::TextureHandle> _videoTexture;
	mutable glm::u16vec2 _videoTextureSize {0, 0};
	mutable std::optional<uint32_t> _videoSerial;
	/// 1x1 white R8: the s_alpha of the WorldQuad program, the alpha 1 of the X1R5G5B5 tiles (inferred)
	mutable graphics::UniqueHandle<bgfx::TextureHandle> _videoAlphaTexture;
	/// A mesh with the celestial shader: model matrix, texture, colour, render state
	void DrawCelestialMesh(graphics::RenderPass viewId, const L3DMesh& mesh, const glm::mat4& model, const Texture2D& texture,
	                       const glm::vec4& colour, uint64_t state, const glm::vec4& celestial = glm::vec4(0.0f),
	                       const Texture2D* alpha = nullptr) const;

public:
	Renderer(uint32_t bgfxReset, std::unique_ptr<BgfxCallback>&& bgfxCallback) noexcept;
	~Renderer() noexcept final;

	[[nodiscard]] ShaderManager& GetShaderManager() const noexcept final;

	void ConfigureView(RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept final;

	void PreloadForLand() const noexcept final;
	void PreDraw(const DrawSceneDesc& drawDesc) const noexcept final;
	void DrawScene(const DrawSceneDesc& drawDesc) const noexcept final;
	[[nodiscard]] glm::u16vec2 GetResolution() const noexcept final { return _resolution; }
	[[nodiscard]] float MeasureText(help::TextFont font, std::u16string_view text, float size) const noexcept final;
	void DrawMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept final;
	void Frame() noexcept final;
	/// The OpaqueAlpha view's quad: alpha = 1 over the backbuffer, colour and depth untouched (RenderPass.h)
	void SubmitOpaqueAlpha() const;
	void RequestScreenshot(const std::filesystem::path& filepath) noexcept final;
	[[nodiscard]] bool GetDebug() const noexcept final { return _bgfxDebug; }
	void SetDebug(bool value) noexcept final { _bgfxDebug = value; }
	[[nodiscard]] bool GetProfile() const noexcept final { return _bgfxProfile; }
	void SetProfile(bool value) noexcept final { _bgfxProfile = value; }

	void Reset(glm::u16vec2 resolution) const noexcept final;

private:
	void DrawFootprintPass(const DrawSceneDesc& drawDesc) const;
	/// What every primitive of one mesh draw reads that nothing changes during that draw: worked out once, by the first
	/// sub-mesh that draws
	struct MeshDrawValues
	{
		const Texture2D& heightMap;
		const Texture2D& cellMap;
		glm::vec4 islandExtent; ///< u_islandExtent: the land's minimum and maximum
		glm::vec4 cellMapInfo;  ///< u_cellMap: the land's minimum and the cell map's size
		glm::vec4 modelLight;   ///< u_modelLight (model_light::Uniform)
		bool lit;               ///< the land light table is loaded
		bool frameCells;        ///< this frame's cells texture has the cell map's layout
	};
	[[nodiscard]] MeshDrawValues GetMeshDrawValues() const;
	/// `transformCache`: where bgfx's matrix cache holds the draw's model matrices once the first primitive of the mesh has
	/// copied them there; the later primitives point at that copy instead of copying them again
	void DrawSubMesh(const L3DMesh& mesh, const L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc, bool preserveState,
	                 std::optional<MeshDrawValues>& values, std::optional<uint32_t>& transformCache) const;
	/// The *Static program (one model matrix) for the object programs, used for meshes without bones
	[[nodiscard]] const ShaderProgram* StaticVariant(const ShaderProgram* program) const;
	mutable std::unordered_map<const ShaderProgram*, const ShaderProgram*> _staticVariants;
	/// The variant with 32 bones of an object program, for the posed villagers and animals (drawn one by one)
	[[nodiscard]] const ShaderProgram* BonesVariant32(const ShaderProgram* program) const;
	mutable std::unordered_map<const ShaderProgram*, const ShaderProgram*> _bonesVariants32;
	void DrawPass(const DrawSceneDesc& desc) const;

	// The temple (RendererTemple.cpp): while the player is inside it, the passes draw it in place of the world
	/// Whether the player is inside the temple
	[[nodiscard]] static bool InTemple();
	/// A pass of the temple: its sky, then its rooms and what is drawn in them, in the order they are submitted
	void DrawTemplePass(const DrawSceneDesc& desc) const;
	/// The main room mirrored through the plane of its origin into the reflection's target, which its floor shows
	void DrawTempleReflection(const DrawSceneDesc& drawDesc) const;
	/// The land seen from above, without the haze, which the map over the main room's pool is textured with, drawn
	/// once a visit
	void DrawTempleMapPass(const DrawSceneDesc& drawDesc) const;
	/// How a mesh of the temple is drawn, beyond its submit desc
	struct TempleLook
	{
		/// How far the textures have slid across the mesh
		glm::vec2 uvOffset {0.0f};
		/// Textures some of the submeshes are drawn with in place of their skins, colours added to some, and those
		/// left undrawn, by submesh (RenderContext::InstancedDrawDesc)
		std::span<const std::pair<uint32_t, TextureHandle>> subMeshTextures;
		std::span<const std::pair<uint32_t, glm::vec3>> subMeshGlows;
		std::span<const uint32_t> hiddenSubMeshes;
		bool onlyJoints {false};
		bool hideShutJoints {false};
		/// In its own colour alone, not shaded by the light, and its alpha multiplied by opacity
		bool unshaded {false};
		float opacity {1.0f};
		/// The reflection's target, which the floor's and the pool's programs read
		const Texture2D* reflection {nullptr};
		/// One-off instances of a mesh the renderer places itself, in place of the desc's
		const bgfx::InstanceDataBuffer* instances {nullptr};
		/// Every primitive drawn in this state, culled by its material, in place of its material's mode
		std::optional<uint64_t> state;
	};
	/// The programs a pass of the temple draws with, looked up once at the start of the pass
	struct TemplePrograms
	{
		const ShaderProgram* temple {nullptr};
		const ShaderProgram* lightmap {nullptr};
		const ShaderProgram* reflective {nullptr};
		const ShaderProgram* reflection {nullptr};
		const ShaderProgram* textured {nullptr};
		const ShaderProgram* beam {nullptr};
	};
	void DrawTempleMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, const TempleLook& look,
	                    const TemplePrograms& programs) const;
	void DrawTempleSubMesh(const L3DMesh& mesh, const L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc,
	                       const TempleLook& look, const TemplePrograms& programs, const TextureHandle* subMeshTexture,
	                       glm::vec3 glow) const;
	/// A black floor under the whole temple, which the cracks the rooms are modelled with show instead of the sky
	void DrawTempleUnderside(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The main room's pool: its water twice, turned an eighth apart, each shimmering as the other fades
	void DrawTemplePool(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The temple's map: the island in relief over the main room's pool
	void DrawTempleMap(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The markers on the temple's map, on soft glows that show through anything
	void DrawTempleMapMarkers(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The creature's room's belts and medals, in the temple's light, while the room is drawn
	void DrawTempleCaveTrophies(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The mists of the rooms drawn whole (components::MistDome), by the island's mist draw; none in the reflection
	void DrawTempleMists(const DrawSceneDesc& desc) const;
	/// The text the temple's rooms write in the world this frame: the signs' labels and the scroll the camera is close to
	void DrawTempleText(const DrawSceneDesc& desc) const;
	/// The beams of the temple's spot lights and the light its windows shed, which the game draws in its rooms but not
	/// in the reflection of the main room
	void DrawTempleLightBeams(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// The glows of the lights of the rooms drawn whole; the main room reflects its own glows alone in its floor
	void DrawTempleGlows(const DrawSceneDesc& desc, const TemplePrograms& programs) const;
	/// Shafts of light and glows (Graphics/LightBeams.h) with the beam program: the texture by the vertices' colours,
	/// with the alpha of `alpha` when given
	void SubmitBeamMesh(RenderPass viewId, const ShaderProgram* shader, const BeamMesh& mesh, const glm::mat4& model,
	                    const TextureHandle& texture, std::optional<TextureHandle> alpha, uint64_t state) const;
	/// The spot lights' cones of a pass, built again each time in storage kept from one pass to the next
	mutable BeamMesh _templeCones;
	/// The land seen from above for the temple's map, and the visit to the temple it was drawn for
	mutable std::unique_ptr<FrameBuffer> _templeMapFrameBuffer;
	mutable std::optional<uint32_t> _templeMapVisit;
	/// The map was drawn this frame, after the main pass that would read it: the map shows from the next frame
	mutable bool _templeMapFresh {false};

	std::unique_ptr<ShaderManager> _shaderManager;
	std::unique_ptr<BgfxCallback> _bgfxCallback;
	mutable std::unique_ptr<LandLightTable> _landLight;
	/// The land light's palette (resources' LandLightPalette::k_Id), null when it could not be loaded
	mutable std::shared_ptr<const LandLightPalette> _landLightPalette;
	/// The land is lit once its palette is loaded: until then the land, the sea and the models are drawn unlit
	[[nodiscard]] bool IsLandLit() const noexcept { return _landLightPalette != nullptr; }
	mutable graphics::UniqueHandle<bgfx::TextureHandle> _landLightTexture;
	/// The inputs of the land light table's last build and upload, as their exact bits: the table is built and uploaded
	/// again only when one of them changes, since the same inputs build the same table
	struct LandLightInputs
	{
		uint32_t skyType;
		uint32_t alignment;
		uint32_t overcast;
		uint8_t flash;
		bool operator==(const LandLightInputs&) const = default;
	};
	mutable std::optional<LandLightInputs> _landLightBuilt;
	mutable std::array<glm::vec4, 2> _hazeUniforms {}; ///< u_haze and u_hazeColour of the frame (PreDraw)
	mutable haze::Params _haze;                        ///< the haze of the frame (graphics::haze::Frame, PreDraw)
	/// The clouds of the main view this frame, collected by PreDraw (CollectClouds: their animation counters advance)
	mutable std::vector<std::pair<float, uint32_t>> _preClouds;
	/// What the main pass draws of the PSys this frame, collected at the end of PreDraw with the
	/// desc's FrameInputs: the effects by their draw path, the surfaces of revolution and the exploded pieces (the
	/// Sorted ones with the entities; the Queued and Immediate ones tagged with their atom). Empty outside the main view
	struct FrameParticles
	{
		psys::manager::SortedFrame sorted;
		std::vector<psys::manager::OrderedEffect> queued;
		std::vector<psys::manager::OrderedEffect> hand;
		std::vector<psys::surf_revol::Surface> surfaces;
		world_triangles::Frame sortedPieces;  ///< refilled every frame, kept for its capacity
		world_triangles::Frame orderedPieces; ///< likewise
	};
	mutable FrameParticles _frameParticles;
	/// The main view's mists, chimney smoke and raining tiles this frame (their Z-sorter keys and indices in _frameMists,
	/// _frameSmoke and _frameRain), collected by PreDraw: CollectMists, CollectChimneySmoke and CollectRain advance
	/// what they collect
	mutable std::vector<std::pair<float, uint32_t>> _preMists;
	mutable std::vector<std::pair<float, uint32_t>> _preSmoke;
	mutable std::vector<std::pair<float, uint32_t>> _preRain;
	/// The influence border's UV offset this frame (UpdateInfluenceCurtain), none when its camera gate failed
	mutable std::optional<glm::vec2> _preInfluenceScroll;
	/// RawTextureIds' cache (draw-owned)
	mutable std::unordered_map<std::string, std::pair<entt::id_type, entt::id_type>> _rawTextureIds;
	mutable float _sunGlare {0.0f}; ///< Sun glare visibility 0..255, smoothed
	mutable std::unique_ptr<Clouds> _clouds;
	mutable glm::u16vec2 _resolution {0, 0}; ///< of the main view
	/// The projected shadows, the ShadowInfo list (shadow_list)
	std::unique_ptr<shadow_list::List> _shadows;
	/// The objects of this frame's main view under a shadow that falls on objects (one not cast by the object itself,
	/// whose box contains the object's bounding box): instance index -> the shadows, newest first
	struct ShadowReceiver
	{
		entt::id_type meshId {0};
		bool morphWithTerrain {false};
		std::vector<const shadow_list::ShadowInfo*> shadows;
	};
	mutable std::unordered_map<uint32_t, ShadowReceiver> _shadowReceivers;
	mutable std::vector<float> _cloudAlpha;         ///< per cloud 0..255 this frame
	mutable std::vector<uint8_t> _cloudShadowImage; ///< sclouds.raw
	/// This frame's land cells (land_light::Texels: the stamps and the night lights in them), the cell map's layout
	mutable graphics::UniqueHandle<bgfx::TextureHandle> _landCellsTexture;
	/// The instances of the fish puzzle nets' floats (RendererFishPlot.cpp), made on first use
	mutable graphics::UniqueHandle<bgfx::DynamicVertexBufferHandle> _fishPlotInstances;
	mutable uint32_t _fishPlotCapacity {0};
	/// The advisor spirits' instances (RendererSpirits.cpp): two in the frame, two in the overlay, made on first use
	mutable graphics::UniqueHandle<bgfx::DynamicVertexBufferHandle> _spiritInstances;
	mutable glm::u16vec2 _landCellsSize {0, 0};
	/// Moves the clouds and computes their colour / alpha; then this frame's land cells (land_light): the frame's
	/// light stamps with the clouds' shadows among them, the night lights, uploaded to _landCellsTexture
	void UpdateClouds() const;
	mutable glm::vec3 _cloudRgb {1.0f};
	mutable SkyAlignment _skyAlignment;      ///< Moved towards the target every frame
	mutable uint32_t _cloudsGeneration {0};  ///< Clouds::GetLandscapeGeneration of _clouds
	mutable float _cloudMilliseconds {0.0f}; ///< this frame's game time step for the clouds' animation counters
	uint32_t _bgfxReset;
	bool _bgfxDebug = false;
	bool _bgfxProfile = false;
	std::unique_ptr<Mesh> _plane;
};
} // namespace graphics
} // namespace openblack
