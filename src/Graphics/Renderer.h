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
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL.h>
#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/SkyDome.h"
#include "Enums.h"
#include "Graphics/DrawEncoder.h"
#include "Graphics/GroundShadow.h"
#include "Graphics/HandReflection.h"
#include "Graphics/HandShadow.h"
#include "Graphics/PaintedSkinStamp.h"
#include "Graphics/PartialBuildCap.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/SeaRows.h"
#include "Graphics/VolumeGlowShell.h"
#include "Particles/ParticleDrawFrame.h"
#include "Visual/LandDetail.h"
#include "Visual/RenderPass.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace bgfx
{
struct Encoder;
struct OcclusionQueryHandle;
} // namespace bgfx

namespace openblack
{
class Camera;
class LandLightTable;
struct BgfxCallback;
class Game;
class WorkerPool;

namespace ecs
{
class Registry;
namespace components
{
struct CreatureEyes;
struct CreatureHair;
struct CreatureHandGlows;
} // namespace components
} // namespace ecs

namespace graphics
{
class FrameBuffer;
class LandFootprints;
class LandShadowBake;
class L3DSubMesh;
class OffscreenScreenshots;
class MorphStreamLayouts;
class ShaderProgram;
class Mesh;
namespace view_frustum
{
struct Frustum;
}

class Renderer final: public RendererInterface
{
public:
	Renderer(uint32_t bgfxReset, std::unique_ptr<BgfxCallback>&& bgfxCallback) noexcept;
	~Renderer() noexcept final;

	[[nodiscard]] ShaderManager* GetShaderManager() const noexcept final;

	void ConfigureView(RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept final;

	void DrawScene(const DrawSceneDesc& drawDesc) const noexcept final;
	void BakeLandShadows(bool drawVegetation) const noexcept final;
	[[nodiscard]] bool WarmUpScene(const DrawSceneDesc& drawDesc, uint32_t part) const noexcept final;
	void PrepareCreatureSkins(size_t count) const noexcept final;
	void ReleaseGpuWorkMemory() noexcept final;
	void DrawMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept final;
	void ShowInterfaceCreatures(interface_creatures::Request request) noexcept final;
	void ShowMenuCreature(interface_creatures::MenuCreature creature) noexcept final;
	void DrawFilmCreature(const DrawSceneDesc& desc, const interface_creatures::FilmCreature& creature) const final;
	void Frame() noexcept final;
	void RequestScreenshot(const std::filesystem::path& filepath) noexcept final;
	[[nodiscard]] std::optional<std::string> TakeScreenshotFailure(const std::filesystem::path& filepath) noexcept final;
	void RequestScreenPicture(const std::string& name, graphics::ScreenPicturer picturer) noexcept final;
	[[nodiscard]] std::optional<std::vector<uint8_t>> TakeScreenPicture(const std::string& name) noexcept final;
	[[nodiscard]] bool GetDebug() const noexcept final { return _bgfxDebug; }
	void SetDebug(bool value) noexcept final { _bgfxDebug = value; }
	[[nodiscard]] uint32_t GetPipelinesMade() const noexcept final;
	[[nodiscard]] uint32_t GetDrawsOfLastFrame() const noexcept final;
	[[nodiscard]] bool GetProfile() const noexcept final { return _bgfxProfile; }
	void SetProfile(bool value) noexcept final { _bgfxProfile = value; }

	void Reset(glm::u16vec2 resolution) const noexcept final;

private:
	/// Paints the footprints into the blocks of the land where they changed
	void PaintLandFootprints(const DrawSceneDesc& drawDesc) const;
	/// The rivers' beds or channels, a footprint for each stretch of river
	/// The land's luminosity this frame: as it was laid, shaded by the clouds and lit by the lights at night
	void DrawLandLuminosityPass(const DrawSceneDesc& drawDesc) const;
	[[nodiscard]] const Texture2D& GetLandLuminosity() const;
	/// The land cells' colours this frame: as they were laid, with the frame's colour stamps (see land_colour_stamps)
	void DrawLandColourPass(const DrawSceneDesc& drawDesc) const;
	/// The sky's dome blended again where the sky has moved on, a band of rows a frame
	void DrawSkyDomePass(const DrawSceneDesc& drawDesc) const;
	[[nodiscard]] const Texture2D& GetLandColour() const;
	/// The sun in the sky, after the sky's dome
	void DrawSun(RenderPass viewId) const;
	/// The sun's glare over the finished view, dimmed by what hides the sun from the camera
	void DrawSunGlare(const Camera& camera) const;
	/// Asks the drawing of a glare sample's pixel, between two corners of the screen (0 to 1 from the top left), whether
	/// anything is drawn there nearer than a depth
	void AskGlareSampleDrawn(const Camera& camera, bgfx::OcclusionQueryHandle query, glm::vec2 topLeft, glm::vec2 bottomRight,
	                         float depth) const;
	/// Each vortex's walls, from the sea down under its middle, leaving only their depth: what is drawn after them is
	/// hidden beyond them, which keeps its funnel to the shaft under its hole in the land
	void DrawVortexDepthWalls(const DrawSceneDesc& desc, graphics::RenderPass viewId) const;
	/// The puffs of mist, blended over the scene, the farthest first
	void DrawMists(const DrawSceneDesc& desc) const;
	/// The moon and its glow in the sky, after the sky's dome
	void DrawMoon(RenderPass viewId) const;
	/// The creatures' footprints laid over the land, blended before the rest of what blends, in the main view and the
	/// sea's reflection
	void DrawCreatureFootprints(const DrawSceneDesc& desc) const;
	/// The fish farms' shoals, drawn into what lies under the sea for the sea to be blended over them
	void DrawFishShoals(const DrawSceneDesc& desc) const;
	/// The villagers' ground blobs, in the main view
	void DrawGroundBlobs(const DrawSceneDesc& desc) const;
	/// The advisors: those out in the world with the scene, their rainbow trails after all else that blends, and those
	/// near the screen in a view of their own over everything, in the main view
	void DrawAdvisors(const DrawSceneDesc& desc) const;
	/// The villagers too far away to be drawn, each a dark smudge facing the view
	void DrawFarVillagerSmudges(const DrawSceneDesc& desc) const;
	/// The eyes of the villagers a script draws in high detail, over their faces, in the main view
	void DrawVillagerEyes(const DrawSceneDesc& desc) const;
	/// The rain about the camera, each block's in its place among what blends, in the main view
	void DrawRain(const DrawSceneDesc& desc) const;
	/// The rings on the water where things splashed, added over the land and the sea
	void DrawWaterRings(const DrawSceneDesc& desc) const;
	/// The snow falling about the camera where it snows, each quarter of a land block in its place among what blends
	void DrawSnowfall(const DrawSceneDesc& desc) const;
	/// After a knock, a row of little people over each house of the knocked town: one for each adult living there and
	/// one for each free place
	void DrawKnockReadout(const DrawSceneDesc& desc) const;
	/// The smoke from the homes' chimneys, each in its place among what blends, in the main view
	void DrawChimneySmoke(const DrawSceneDesc& desc) const;
	/// Gathers what the particle effects draw this frame, once for every pass
	void CollectParticles() const;
	/// The particle effects in the order the camera of the pass draws them, among the other things that blend: runs of
	/// sprites that share a sheet as one instanced draw, ribbons, models and mists
	void DrawParticles(const DrawSceneDesc& desc) const;
	/// Draws what a frame of particles holds, in the order of its commands, seen from a point
	void SubmitParticleFrame(const DrawSceneDesc& desc, const particles::draw::Frame& frame,
	                         std::span<const particles::draw::Command> commands, std::span<const uint32_t> spriteOrder,
	                         std::span<const particles::draw::Command> beforeLand, const glm::vec3& origin) const;
	/// For a warm-up, a sprite of no size for each way that sprites blend, into the pass's blended view
	void WarmUpParticles(const DrawSceneDesc& desc) const;
	/// The opening's light falling from the sky, and its flash where it lands
	void DrawIntroLight(const DrawSceneDesc& desc) const;
	/// The ghosts of the buildings effects destroyed, each laying its depth through a scrolling pattern and then added
	/// over itself where it did, the farthest first
	void DrawDestructionGhosts(const DrawSceneDesc& desc) const;
	/// The explorers' boat mirrored in the sea, in its own flat colour
	void DrawFilmReflections(const DrawSceneDesc& desc) const;
	/// The physical shields' domes
	void DrawShieldDomes(const DrawSceneDesc& desc) const;
	/// A particle's mist, in the translucent pass at a place among what blends
	void DrawParticleMist(const DrawSceneDesc& desc, const particles::draw::MistDraw& mist, uint32_t depth) const;
	/// A particle's model, in the translucent pass at a place among what blends
	void DrawParticleMesh(const DrawSceneDesc& desc, const particles::draw::MeshDraw& mesh, uint32_t depth) const;
	/// The one-shot globes, the miracles in them and the rings round the extreme ones, in the main and reflected views
	void DrawGlobes(const DrawSceneDesc& desc) const;
	/// The miracles' seeds spinning above the village centres' stone hands and the worship sites' icons, with a ring round
	/// each for every power-up level shown
	void DrawIconSeeds(const DrawSceneDesc& desc) const;
	/// The miracle a chest's town has learnt, spinning above the opened chest
	void DrawRewardSeeds(const DrawSceneDesc& desc) const;
	/// A miracle's seed shown as a model, as a globe holds it or the creature's room shows it
	struct SeedModelDraw
	{
		SpellSeedType seed;
		/// The point its model stands its mesh height above, at its scale
		glm::vec3 middle;
		float scale;
		/// Its turn about the up axis, in radians
		float spin;
		/// A phial's pulse and its texture's frame
		float phialPhase;
		float phialFrame;
		/// How faint a phial is drawn, of 255. A seed drawn at once is drawn at this alpha too.
		uint8_t phialAlpha;
		/// The alignment of the player it is shown for, which makes the flying flock's seed a bat or a dove
		float ownerAlignment;
		/// The seed is drawn at once in white, with no light from the land, as the seeds over the gestures' hints are
		bool drawnAtOnce {false};
	};
	/// With `seenBy`, the function does not send a draw of the model that is out of that view. The draws go into the
	/// encoder given, or into the game thread's encoder without one.
	void DrawSeedModel(const DrawSceneDesc& desc, const SeedModelDraw& shown, const view_frustum::Frustum* seenBy = nullptr,
	                   DrawEncoder* encoder = nullptr) const;
	/// The pass what blends in a scene goes to: its own pass after the scene's, but in the temple the scene's own
	[[nodiscard]] static RenderPass TranslucentView(RenderPass scene);
	/// The seeds of the creature's best-learnt miracles hovering by the plinths in the creature's room
	void DrawCaveSeeds(const DrawSceneDesc& desc) const;
	/// The seeds of the miracles that the players cast, over the main room's map
	void DrawMapSpellSeeds(const DrawSceneDesc& desc) const;
	/// How the glow over a hand holding a miracle looks this frame: the flow's sheet, the cell of it shown and the
	/// colour it is drawn in
	struct HandGlowLook
	{
		const Texture2D* flow;
		glm::vec2 uvOffset;
		float uvScale;
		/// Of 255
		glm::vec3 colour;
		float alpha;
	};
	/// The glow while the hand holds a miracle, none otherwise
	[[nodiscard]] std::optional<HandGlowLook> HandGlowOf(const DrawSceneDesc& desc) const;
	[[nodiscard]] const Texture2D* HandFlowTexture() const;
	/// The bands flying onto and off the hand that holds a miracle and the bracelets it wears, in the main view
	void DrawHandMiracleBands(const DrawSceneDesc& desc) const;
	/// A tribe's power behind a miracle: its name's letters spinning round the hand or rising where it was cast
	void DrawTribalPower(const DrawSceneDesc& desc) const;
	/// The words of the game's pause and speed, after the rest of the scene
	void DrawPauseText(const DrawSceneDesc& desc) const;
	/// A piece of a broken model: its triangles placed by its atom, a draw for each skin
	void DrawParticleFragment(const DrawSceneDesc& desc, const particles::draw::FragmentDraw& fragment, uint32_t depth) const;
	/// A glowing shell over an object's model, by the object as it is drawn this frame
	void DrawParticleShell(const DrawSceneDesc& desc, const particles::draw::ShellDraw& shell, uint32_t depth) const;
	/// A glowing shell over a model at a placement, in a view (a bgfx view id), seen through a projection
	void SubmitGlowShell(uint16_t viewId, entt::id_type meshId, const glm::mat4& model, const particles::draw::ShellDraw& shell,
	                     const glm::mat4& projection, bool reflection, uint32_t depth) const;
	/// The copy of the creature in the falling spell's film, in the film's view
	void DrawFilmCopy(const DrawSceneDesc& desc, const interface_creatures::FilmCreature& shown) const;
	/// The motes, the stars and the burst of light of the falling spell's film, in the film's view
	void DrawFilmEffects(const interface_creatures::FilmCreature& film) const;
	/// The glows on a creature's body where its pose put them, in a view (a bgfx view id)
	void DrawCreatureHandGlows(uint16_t viewId, const ecs::components::CreatureHandGlows& glows, const glm::mat4& projection,
	                           bool reflection, uint32_t depth) const;
	/// The band of a glowing shell over a part of a model, made when first drawn; nullptr for a part without one
	[[nodiscard]] const VolumeGlowShell* GlowShellOf(entt::id_type mesh, uint32_t subMesh) const;
	/// A frame of a particle light map as a texture of its colours, made when first stamped
	[[nodiscard]] const Texture2D* ParticleLightMap(entt::id_type bitmap, int frame) const;
	/// The texels of a frame of a particle light map, of grey or colour; none for a light map that cannot stamp
	[[nodiscard]] std::span<const uint8_t> ParticleLightMapTexels(entt::id_type bitmap, int frame) const;
	/// The border of the players' influence, in the main view
	void DrawInfluenceBorder(const DrawSceneDesc& desc) const;
	/// The ripples the hand makes crossing a border, each in its place among what blends, in the main view
	void DrawInfluenceRipples(const DrawSceneDesc& desc) const;
	/// A mesh of the sky's, in a colour, with a texture and that texture's alpha
	struct CelestialDraw
	{
		entt::id_type meshId;
		entt::id_type textureId;
		entt::id_type alphaTextureId;
		glm::mat4 model;
		glm::vec4 colour;
		/// See vs_celestial
		glm::vec4 celestial;
		uint64_t state;
	};
	void DrawCelestialMesh(RenderPass viewId, const CelestialDraw& draw) const;
	/// The hand's glow on the water at night, under the sea
	void DrawHandWaterGlow(const DrawSceneDesc& desc) const;
	/// The sea's rows, ripple, period and colour for the camera
	void SetSeaUniforms(const ShaderProgram& waterShader, const Camera& camera) const;
	/// Bakes the shadows of the island's trees, rocks and buildings into its land (see LandShadowBake), and draws the trees'
	/// moving shadows when they are asked for (see ObjectShadows)
	void DrawObjectShadowPass(const DrawSceneDesc& drawDesc) const;
	/// Draws the coverage of the flattened silhouettes of the creatures, the hand and the villagers a script draws in
	/// high detail, for their moving shadows on the land (see GroundShadow)
	void DrawGroundShadowPass(const DrawSceneDesc& drawDesc) const;
	/// One body of a moving shadow: a mesh in its pose, blended towards morph targets when given them
	struct GroundShadowBody
	{
		const L3DMesh* mesh;
		glm::mat4 model;
		std::span<const glm::mat4> pose;
		const L3DMeshSubmitDesc::MorphTargets* morph;
		/// It covers only the second row of each texel's pair of sample rows, as the hand itself does
		bool secondRowsOnly;
	};
	/// The memory in which one lane of the workers makes a shadow (see GroundShadowScratch below)
	struct GroundShadowScratch;
	/// Adds a body's points in the world, its counted parts and what it writes, to those of the shadow being made
	static void AddGroundShadowBody(const GroundShadowBody& body, GroundShadowScratch& scratch);
	/// The levels of the texture of the shadow being made, from the coverage of its counted parts
	[[nodiscard]] static std::array<uint8_t, GroundShadow::k_Texels * GroundShadow::k_Texels>
	GroundShadowLevels(const GroundShadow::Footprint& footprint, uint8_t alpha, GroundShadowScratch& scratch);
	/// The levels of the texture of a moving body's shadow, filled through its textures' alpha (see shadow_texture)
	[[nodiscard]] std::array<uint8_t, GroundShadow::k_Texels * GroundShadow::k_Texels>
	ShadowTextureLevels(const GroundShadowBody& body, const GroundShadow::Footprint& footprint, const GroundShadow::Cast& cast,
	                    uint8_t alpha) const;
	/// Hands the moving shadows to the land's program, or none where it isn't to receive them. Each block sets its own
	/// u_groundShadowBlock, because bgfx accepts a uniform only once for each draw.
	void SetGroundShadowUniforms(const ShaderProgram& program, bool receives) const;
	/// The objects' shadows on the land: those drawn each frame and the trees' baked ones
	void SetObjectShadowUniforms(const ShaderProgram& program) const;
	/// Hands the moving shadows that fall on objects to an object's program, or none where it isn't to receive them
	void SetObjectGroundShadowUniforms(DrawEncoder& to, const ShaderProgram& program, bool receives) const;
	/// The uniforms the draws of meshes set, in the order of their names in the renderer
	enum class MeshUniform : uint8_t
	{
		DepthBias,
		Tint,
		Glow,
		Darkening,
		MorphWeights,
		VertexBlend,
		UvOffset,
		SeaClip,
		Snow,
		SnowDepth,
		SnowTexture,
		SnowAlpha,
		Window,
		Diffuse,
		Lightmap,
		Environment,
		Heightmap,
		IslandExtent,
		LandLight,
		LandLuminosity,
		LandLightTexture,
		LandColour,
		Haze,
		HazeColour,
		ModelLight,
		GroundShadowInfo,
		GroundShadowData,
		SkyAlphaThreshold,
		ObjectLook,
		KeepBelow,
		Inset,
		BonePalette,
		CreatureSpellLook,
		ShadeAt,
		FogAt,
		EnvironmentStage,
		LightRule,
		Shine,

		_count
	};
	static constexpr std::array<std::string_view, static_cast<size_t>(MeshUniform::_count)> k_MeshUniformNames {
	    "u_depthBias",         //
	    "u_tint",              //
	    "u_glow",              //
	    "u_darkening",         //
	    "u_morphWeights",      //
	    "u_vertexBlend",       //
	    "u_uvOffset",          //
	    "u_seaClip",           //
	    "u_snow",              //
	    "s_snowDepth",         //
	    "s_snow",              //
	    "s_snowAlpha",         //
	    "u_window",            //
	    "s_diffuse",           //
	    "s_lightmap",          //
	    "s_environment",       //
	    "s_heightmap",         //
	    "u_islandExtent",      //
	    "u_landLight",         //
	    "s_landLuminosity",    //
	    "s_landLight",         //
	    "s_landColour",        //
	    "u_haze",              //
	    "u_hazeColour",        //
	    "u_modelLight",        //
	    "u_groundShadowInfo",  //
	    "s_groundShadowData",  //
	    "u_skyAlphaThreshold", //
	    "u_objectLook",        //
	    "u_keepBelow",         //
	    "u_inset",             //
	    "s_bonePalette",       //
	    "u_creatureSpellLook", //
	    "u_shadeAt",           //
	    "u_fogAt",             //
	    "u_environmentStage",  //
	    "u_lightRule",         //
	    "u_shine",             //
	};
	using MeshUniforms = std::array<std::optional<UniformHandle>, static_cast<size_t>(MeshUniform::_count)>;
	/// A program's handles of the mesh uniforms it has, looked up by name the first time it draws a mesh
	[[nodiscard]] const MeshUniforms& MeshUniformsOf(const ShaderProgram& program) const;
	/// Draws a submesh, with a texture in place of its skins when given one. The offset moves the submesh in its mesh.
	void DrawSubMesh(const L3DMesh& mesh, const L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc, bool preserveState,
	                 const TextureHandle* texture = nullptr, glm::vec3 glow = glm::vec3(0.0f),
	                 glm::vec3 offset = glm::vec3(0.0f), bool picture = false, bool control = false) const;
	/// What a primitive cut at a height in its model's own space shows: whether anything of it is drawn below the cut,
	/// and the cap over its cut walls. Made once for each cut.
	struct Cap
	{
		bool drawsBelow {false};
		std::vector<partial_build_cap::CapVertex> vertices;
	};
	struct CapPrimitive
	{
		const void* key;
		uint32_t indicesOffset;
		uint32_t indicesCount;
		bool twoSided;
	};
	[[nodiscard]] const Cap& CapOf(const L3DSubMesh& subMesh, const CapPrimitive& primitive, float height,
	                               const std::optional<glm::mat4>& landPlacement) const;
	/// The same for a model that follows the land, placed in the world: each corner rises with the land under it
	[[nodiscard]] const Cap& LandCapOf(const L3DSubMesh& subMesh, const CapPrimitive& primitive, float height,
	                                   const glm::mat4& placement) const;
	/// Binds a cap's corners as the vertices of the next draw; false when there is no room for them this frame
	[[nodiscard]] static bool BindCap(bgfx::Encoder& encoder, const std::vector<partial_build_cap::CapVertex>& cap);
	void DrawPass(const DrawSceneDesc& desc) const;
	/// Draws `count` things that do not depend on each other, `draw(index, encoder)` drawing the thing at the index into
	/// the encoder. Each thing starts from an encoder with nothing bound and no state, also when the game's thread draws
	/// them all, and the encoder is clean again after the last. When the switch to encode on many threads is on, the
	/// worker threads draw some of the things at the same time, each run of things into an encoder of its thread. Their
	/// draws then still come in the order that one thread gives them, so the picture is the same. `draw` must only read
	/// the frame's state, and only the resources that are loaded.
	using EncodeDraw = std::function<void(size_t index, DrawEncoder& encoder)>;
	void EncodeInOrder(size_t count, const EncodeDraw& draw) const;
	/// How many threads encode draws together now: 1 when only the game's thread does
	[[nodiscard]] size_t EncodeLanes() const;
	/// The beams of the temple's spot lights and the light its windows shed, which the game draws in its
	/// rooms but not in the reflection of the main room
	void DrawLightBeams(const DrawSceneDesc& desc) const;
	/// The sheet of white light at the main room's way out, while the camera walks through that door
	void DrawTempleExitGlow(const DrawSceneDesc& desc) const;
	/// The shell of the creed's light over each temple's heart late in its destruction
	void DrawTempleCreedShells(const DrawSceneDesc& desc) const;
	/// The temple's domes of mist, which the game draws facing the camera with a frame of the smoke texture
	void DrawMistDomes(const DrawSceneDesc& desc) const;
	/// The text the temple's rooms write in the world this frame: the signs' labels and the scroll the camera is close to
	void DrawTempleText(const DrawSceneDesc& desc) const;
	/// The temple's map: the land seen from above, unlit, once a visit to the temple
	void DrawTempleMapPass() const;
	/// Makes the temple's map at half size from the map: each texel is the texel at twice its column and twice its row
	void HalveTempleMap() const;
	/// The main room's pool: its water twice, turned an eighth apart, each shimmering as the other fades
	void DrawTemplePool(const DrawSceneDesc& desc) const;
	/// The temple's map: the island in relief over the main room's pool
	void DrawTempleMap(const DrawSceneDesc& desc) const;
	/// A black floor under the whole temple, which the cracks the rooms are modelled with show instead of the sky
	void DrawTempleUnderside(const DrawSceneDesc& desc) const;
	/// The markers on the temple's map, on soft glows that show through anything
	void DrawTempleMapMarkers(const DrawSceneDesc& desc) const;
	/// The creature's room's belts and medals
	void DrawCaveTrophies(const DrawSceneDesc& desc) const;
	/// A creature's eyes and eyelids, after its body: frozen with it, or else fizzing with it, drawn through the static
	/// after its body's passes, from that sort depth on
	void DrawCreatureEyes(const DrawSceneDesc& desc, const ecs::components::CreatureEyes& eyes,
	                      const L3DMeshSubmitDesc& bodyDesc, float fizz, float freeze, uint32_t fizzSortDepth) const;
	/// A creature's strands of hair, as ribbons facing the camera blended over the scene, lit from where the creature is
	void DrawCreatureHair(const DrawSceneDesc& desc, const ecs::components::CreatureHair& hair, glm::vec3 origin) const;
	/// The same, in the given pass, with the ribbons turned to the given eye, for a creature in a view of its own. A
	/// creature with a look of its own gives its hair its light and added colour (0 to 255) in place of the land's.
	void DrawCreatureHair(const DrawSceneDesc& desc, const ecs::components::CreatureHair& hair, glm::vec3 origin,
	                      RenderPass pass, glm::vec3 eye, std::optional<glm::vec3> ownLight = std::nullopt,
	                      std::optional<glm::vec3> ownAdded = std::nullopt) const;
	/// Draws the body of a creature that the interface shows: its mesh, its instance among those of the interface's
	/// creatures, its pose, its shape and its eyes
	using InterfaceCreatureBody =
	    std::function<void(entt::id_type mesh, uint32_t instance, std::span<const glm::mat4> bones,
	                       const L3DMeshSubmitDesc::MorphTargets* targets, const ecs::components::CreatureEyes* eyes)>;
	/// The creatures that the interface asked to show last, each moved to its place before the interface's camera
	void DrawInterfaceCreatures(const DrawSceneDesc& desc, const InterfaceCreatureBody& drawBody) const;
	/// The creature that the main menu's tattoo editor asked to show last, in its view of its own
	void DrawMenuCreature(const DrawSceneDesc& desc, const InterfaceCreatureBody& drawBody) const;
	/// The columns of light over the village centres and temples of the players whose borders show
	void DrawLightColumns(const DrawSceneDesc& desc) const;
	/// The beams of light the started scrolls stand in
	void DrawScrollBeams(const DrawSceneDesc& desc) const;
	/// The pulse models that shrink into the charging icons at the worship sites, added over the scene in their players'
	/// colours
	void DrawIconPulses(const DrawSceneDesc& desc) const;
	/// A tall shaft of light standing at a point and turned to face the eye, added over the scene in a colour: the
	/// columns over villages and temples, and the scrolls' beams
	void DrawLightShaft(const DrawSceneDesc& desc, const L3DMesh& mesh, const glm::vec3& position, const glm::vec4& tint) const;
	/// The leashes' ropes, each a ribbon lit by the land beneath it, and their shadows on the land
	void DrawLeashes(const DrawSceneDesc& desc) const;
	/// The collars of the leashes the gestures' hints offer, each over its hint's square, seen through a camera of its
	/// own
	void DrawGestureHintCollars(const DrawSceneDesc& desc) const;
	/// The seeds and the globe that the gestures' hints picture, each over its hint's square, seen through the collars'
	/// camera: a seed spins with the effect of its holder over it and its rings round it
	void DrawGestureHintSeeds(const DrawSceneDesc& desc) const;
	/// The leashes hanging on the temples, and the one carried in the hand, each drawn just after its glow
	void DrawTempleLeashes(const DrawSceneDesc& desc) const;
	/// A texture for a painted skin: a spare one, else a new one. None if bgfx has no more textures.
	[[nodiscard]] std::unique_ptr<Texture2D> TakeCreatureSkinTexture() const;
	/// A new texture for a painted skin. None if bgfx has no more textures.
	[[nodiscard]] static std::unique_ptr<Texture2D> MakeCreatureSkinTexture();
	/// The hand's skins as blended for its player's alignment (see components::HandMorph), taken up when they change
	void UploadHandSkins() const;
	/// Every animal's bones as posed this frame, in one texture, so that all the animals of a model, a flock's birds, are
	/// drawn at once, each instance finding its own bones by its place among them. A model some of whose animals fade is
	/// drawn an animal at a time as before.
	void UploadAnimalBones(const DrawSceneDesc& drawDesc) const;
	/// What the hand's base mesh is pulled towards for its player's alignment, none while it shows the base
	[[nodiscard]] std::optional<L3DMeshSubmitDesc::MorphTargets> HandMorphTargets() const;

	std::unique_ptr<ShaderManager> _shaderManager;
	std::unique_ptr<BgfxCallback> _bgfxCallback;
	uint32_t _bgfxReset;
	/// Pictures of frames drawn aside, while the window has no buffers to read
	std::unique_ptr<OffscreenScreenshots> _offscreenShots;
	bool _bgfxDebug = false;
	bool _bgfxProfile = false;
	/// While the scene is drawn only to make the renderer's pipelines and targets: nothing that later frames use changes
	mutable bool _warmingUp {false};
	/// The next frame holds a warm-up's draws, after which its views are set back
	mutable bool _warmUpInFrame {false};
	/// The colour that the main view clears the window to
	mutable uint32_t _mainClearColour {0x000000ff};
	/// One instance that a warm-up draws its creatures with: at no size, so that it covers no pixel
	mutable std::optional<DynamicVertexBufferHandle> _warmUpInstances;

	/// How a creature's variant meshes are read to blend its body
	std::unique_ptr<MorphStreamLayouts> _morphStreamLayouts;
	/// Where the trees' moving shadows cover the island, when they are asked for, laid out as its footprints are and made
	/// for the island's size
	mutable std::unique_ptr<FrameBuffer> _objectShadowFrameBuffer;
	/// Whether this frame's trees cast their moving shadows
	mutable bool _movingTreeShadows {false};
	/// The shadows baked into the land, as the game bakes them: the buildings', rocks' and other things', and the trees'
	/// and big forests' unless they are drawn moving
	mutable std::unique_ptr<LandShadowBake> _landShadowBake;
	mutable std::unique_ptr<LandFootprints> _landFootprints;
	/// Whether this frame's land takes the baked shadows
	mutable bool _landShadowsBaked {false};
	/// The land seen from above for the temple's map, and the visit to the temple it was drawn for
	mutable std::unique_ptr<FrameBuffer> _templeMapFrameBuffer;
	mutable std::optional<uint32_t> _templeMapVisit;
	/// The temple's map at half size, which the room shows at the low detail levels in place of the map
	mutable std::unique_ptr<FrameBuffer> _templeMapHalfFrameBuffer;
	mutable bool _templeMapHalved {false};
	/// What the land and the objects read the shadows by, GroundShadow::k_DataTexels texels a shadow (see
	/// ground_shadow.sh), then each land block's list of the shadows reaching it; grown by rows as needed
	mutable std::unique_ptr<Texture2D> _groundShadowData;
	mutable std::vector<glm::vec4> _groundShadowTexels;
	/// The shadows of the frame being drawn, of which the first fall on objects too
	mutable uint32_t _groundShadowCount {0};
	mutable uint32_t _groundShadowsOnObjects {0};
	/// Each land block's first entry in the lists, and how many shadows reach it, by the block's index
	mutable std::vector<glm::vec2> _groundShadowBlocks;
	/// The land's level of detail this frame, from the camera of the view: the lines across the ground, and how each
	/// block is drawn and which of its sides are stitched to coarser neighbours, by the block's index. The land
	/// mirrored under the sea is drawn with the same.
	struct LandDetailFrame
	{
		land_detail::Lines lines;
		land_detail::ViewLine meshNear;
		land_detail::ViewLine meshFar;
		land_detail::ViewLine texture;
		std::vector<land_detail::BlockDetail> blocks;
		std::vector<uint8_t> coarserSides;
		/// The indices of the blocks at x - 1, z - 1, z + 1 and x + 1, -1 where there is none
		std::vector<glm::vec4> neighbours;
		/// Whether a part of the box over the block can be seen from the view's camera. Only those blocks are drawn,
		/// on the land and mirrored under the sea.
		std::vector<bool> inView;
	};
	mutable LandDetailFrame _landDetail;
	/// A counted part of a shadow's bodies: a submesh, its first point and whether it covers only every second row of
	/// samples
	struct GroundShadowPart
	{
		const L3DSubMesh* subMesh;
		uint32_t first;
		bool secondRowsOnly;
	};
	/// The memory for the bodies of one shadow: their points in the world, their counted parts, their points on the grid
	/// of samples and the samples that they cover. Each shadow starts it again. Each lane of the workers has its own,
	/// and keeps it from frame to frame, thus a frame does not allocate it again.
	struct GroundShadowScratch
	{
		std::vector<GroundShadow::Point> points;
		std::vector<GroundShadowPart> parts;
		std::vector<glm::vec2> flattened;
		std::vector<glm::vec2> samples;
		GroundShadow::Coverage coverage;
		/// The bones of the body being added, in the world
		std::vector<glm::mat4> bones;
	};
	mutable std::vector<GroundShadowScratch> _groundShadowScratch;
	/// A shadow that the workers made for a caster: where it lies, if anywhere, and its texture's levels
	struct GroundShadowMade
	{
		std::optional<GroundShadow::Footprint> footprint;
		std::array<uint8_t, GroundShadow::k_Texels * GroundShadow::k_Texels> levels;
	};
	mutable std::vector<GroundShadowMade> _groundShadowMade;
	/// The threads that make the shadows of a frame side by side, and that encode draws with the game's thread
	std::unique_ptr<WorkerPool> _workers;
	/// The submit order of the game thread's encoder in this frame. Each run of draws on many threads takes the orders
	/// after it, and the game thread's encoder goes on after those.
	mutable uint32_t _submitOrder {0};
	/// The shadows of the things that the physics moves, as the last frame made them: their data for the land, and
	/// where they lie. A thing under the sea keeps its shadow from when it went under.
	struct KeptGroundShadow
	{
		std::vector<glm::vec4> texels;
		GroundShadow::Footprint footprint;
	};
	mutable std::unordered_map<entt::entity, KeptGroundShadow> _keptGroundShadows;
	/// The caps over the broken buildings' cut walls, by primitive and cut height; made afresh when a cut moves
	mutable std::map<std::pair<const void*, float>, Cap> _caps;
	/// The caps of the models that follow the land, made again each frame
	mutable std::map<std::tuple<const void*, float, float, float, float>, Cap> _landCaps;
	/// The land's light this frame, and the 256 by 1 texture the terrain reads it from
	mutable std::unique_ptr<LandLightTable> _landLightTable;
	/// The sea's ripple, moving on with the frames the sea's rows are drawn in while the game's time goes on
	mutable sea_rows::Ripple _seaRipple;
	/// The land's luminosity of this frame and of the frame before, a copy of the shaded land (see land_shade) sized to
	/// the land's. Each frame changes the other one and keeps the rows it changed: the next frame changes those too.
	mutable std::array<std::unique_ptr<Texture2D>, 2> _landLuminosity;
	mutable size_t _landLuminosityNow {0};
	mutable glm::u16vec2 _landLuminosityRowsBefore {0};
	/// The land's luminosity of the frame before
	[[nodiscard]] const Texture2D& GetLandLuminosityBefore() const;
	/// The land cells' colours this frame, sized to the land's
	mutable std::unique_ptr<FrameBuffer> _landColourFrameBuffer;
	/// The sky's dome for each alignment, one above the other, kept from frame to frame
	mutable std::unique_ptr<FrameBuffer> _skyDomeFrameBuffer;
	/// The rows of each alignment's dome in the frame buffer above
	mutable uint16_t _skyDomeRows {sky_dome::k_Rows};
	/// The glow lightning stamps on the land, made the first time it is drawn
	mutable std::optional<TextureHandle> _lightningGlowTexture;
	/// How strongly the sun glares, 0 to 255, easing towards how much of the sun shows
	mutable float _sunGlare {0.0f};
	/// The question asked of each glare sample's pixel as the scene is drawn: whether anything nearer than the depth that
	/// hides it is there. Made the first time it is asked.
	mutable std::array<bgfx::OcclusionQueryHandle, 5> _glareQueries {
	    {BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};
	mutable std::optional<TextureHandle> _landLightTexture;
	/// The mesh uniforms of each program that has drawn a mesh; the programs live as long as the renderer
	mutable std::unordered_map<const ShaderProgram*, MeshUniforms> _meshUniforms;
	/// The snow's textures, looked up once a frame, null until they are loaded
	mutable const Texture2D* _snowTexture {nullptr};
	mutable const Texture2D* _snowAlphaTexture {nullptr};
	/// How deep the snow lies over the island, refreshed when it changes
	mutable std::unique_ptr<Texture2D> _snowDepth;
	mutable std::optional<uint32_t> _snowRevision;
	/// u_haze and u_hazeColour of the frame's distance haze, off until the land's light is built
	mutable std::array<glm::vec4, 2> _haze {};
	/// The colours the sky's dome is drawn in this frame
	mutable sky_dome::Tint _skyTint;
	/// icons.raw with iconsa.raw's alpha, which the creature's room's belts and medals are drawn with, once loaded
	mutable std::optional<TextureHandle> _iconsTexture;
	mutable bool _iconsLoaded {false};
	/// S_Hand_Flow.raw with S_Hand_Flowa.raw's alpha, which the hand holding a miracle glows with, once loaded
	mutable std::unique_ptr<Texture2D> _handFlowTexture;
	mutable bool _handFlowLoaded {false};
	/// A creature's skins as painted (see components::CreatureSkin), one texture each
	struct CreatureSkins
	{
		/// The painting the textures hold, none before the first upload
		std::optional<painted_skin::Stamp> stamp;
		std::vector<std::unique_ptr<Texture2D>> textures;
		/// The painted skins the body is drawn with in place of its base mesh's, by skin id
		std::vector<std::pair<uint32_t, const Texture2D*>> drawn;
	};
	mutable std::unordered_map<entt::entity, CreatureSkins> _creatureSkins;
	/// Takes up the skins of the things of a registry where they have been painted again, before their bodies are drawn
	/// with them; drops those of things that are no longer in the registry
	void UploadCreatureSkins(const ecs::Registry& entities, std::unordered_map<entt::entity, CreatureSkins>& skins) const;
	/// The painted skins of the copies of creatures in the temple's rooms, as the copy in the creature room has
	mutable std::unordered_map<entt::entity, CreatureSkins> _roomCreatureSkins;
	/// The painted skins of the things of the drawn registry, as the creature of the tattoo menu has
	mutable std::unordered_map<entt::entity, CreatureSkins> _drawnCreatureSkins;
	/// Textures for painted skins that no creature uses now. The game makes them behind the loading screen, so that the
	/// first frame of a creature does not make them.
	mutable std::vector<std::unique_ptr<Texture2D>> _spareCreatureSkins;
	/// The animals of a model drawn at once this frame: where their bones start in the texture, how many each has, and
	/// whether they take the brightest of the land's light, are white, or take the land's light where each stands
	struct AnimalBoneGroup
	{
		uint32_t firstMatrix {0};
		uint32_t bones {0};
		bool brightestLand {true};
		bool landWhereItStands {false};
	};
	mutable std::unordered_map<entt::id_type, AnimalBoneGroup> _animalBoneGroups;
	/// The bones' matrices, four texels each, a row of the texture after another
	mutable std::vector<glm::mat4> _animalBones;
	mutable std::unique_ptr<Texture2D> _animalBoneTexture;
	/// Whether running out of textures for the creatures' skins has been logged
	mutable bool _warnedOutOfSkins {false};
	/// The hand's skins as blended for its player's alignment, one texture each, and the blending they hold
	mutable std::vector<std::unique_ptr<Texture2D>> _handSkinTextures;
	mutable std::vector<std::pair<uint32_t, const Texture2D*>> _handSkins;
	mutable std::optional<painted_skin::Stamp> _handSkinStamp;
	/// The creatures drawn this frame: all of them, or the nearest the camera when there are more than the backend can
	/// upload the bones of
	struct DrawnCreature
	{
		entt::entity entity;
		uint32_t instance;
		/// A copy of the creature drawn elsewhere in its own pose: the entity it takes its pose and eyes from, and where
		/// it is lit from. The copy is a thing of the temple's rooms.
		entt::entity posedBy {entt::null};
		std::optional<glm::vec3> light;
	};
	mutable std::vector<DrawnCreature> _drawnCreatures;
	/// The creatures that the interface asked to show, which the next frame of the scene draws, and the instances that
	/// place them
	mutable std::optional<interface_creatures::Request> _interfaceCreatures;
	mutable std::optional<DynamicVertexBufferHandle> _interfaceCreatureInstances;
	mutable std::optional<interface_creatures::MenuCreature> _menuCreature;
	mutable std::optional<DynamicVertexBufferHandle> _menuCreatureInstance;
	mutable std::optional<DynamicVertexBufferHandle> _filmCreatureInstance;
	mutable bool _warnedCreatureCap {false};
	/// Found by trial in a debug build, whose backends check the uploads: Vulkan overflowed with 65 creatures drawn and
	/// Direct3D 12 with 850, and both held with 50 and 600
	static constexpr size_t k_MaxCreaturesDrawnVulkan = 40;
	static constexpr size_t k_MaxCreaturesDrawnDirect3D12 = 500;
	void SelectDrawnCreatures(const DrawSceneDesc& drawDesc) const;
	[[nodiscard]] static size_t MaxCreaturesDrawn();
	/// Sampled by the primitives without a skin, as Direct3D's texture stages read white with no texture set
	std::optional<TextureHandle> _whiteTexture;
	/// A texel that the renderer reads back to make a frame wait for the GPU; none if the backend cannot read textures
	std::optional<TextureHandle> _readBackTexture;
	/// Where the read-back texel goes
	std::array<uint8_t, 4> _readBackTexel {};
	/// u_modelLight: where the game's model light is this frame, and its ambient
	mutable glm::vec4 _modelLight {0.0f};
	/// What the land's light is scaled by for the trees this frame, of 1, once a frame has worked it out
	mutable float _treeBrightness {1.0f};
	mutable bool _treeBrightnessMade {false};
	/// The land's light of the frame before. The sea mirrors a held thing in it, as the draw of the thing in the hand
	/// took it on that frame.
	struct LightOfFrameBefore
	{
		/// Copies of the land light table and the land cells' colours of the frame before
		std::optional<TextureHandle> landLight;
		std::optional<TextureHandle> colour;
		glm::u16vec2 colourSize {0};
		/// The colours were copied at the start of this frame
		bool texturesKept {false};
		hand_reflection::FrameBefore<std::array<glm::vec4, 2>> haze;
		hand_reflection::FrameBefore<float> treeBrightness;
	};
	mutable LightOfFrameBefore _before;
	/// At the start of a frame, before the frame makes its own land light, keeps the land's light of the frame that ended
	void KeepLightOfFrameBefore() const;
	void ReleaseLightOfFrameBefore() const;
	/// u_modelLight: the game's model light this frame
	[[nodiscard]] glm::vec4 GetModelLight() const;
	/// Rebuilds the land's light for this frame from the palette; the texture of it
	TextureHandle UpdateLandLight() const;
	/// Works out the land's level of detail for this frame from the view's camera (see land_detail)
	void UpdateLandDetail(const DrawSceneDesc& drawDesc) const;
	/// The land's light, or a texture to bind in its place before it is first built
	[[nodiscard]] TextureHandle GetLandLightTexture() const;
	std::unique_ptr<Mesh> _plane;
	/// What the particle effects draw this frame, and the order of the pass being drawn, kept for their room
	mutable particles::draw::Frame _particleFrame;
	/// The light maps and shadows that the particles drew in the frame before: the game stamps them on the land at the
	/// start of the next frame
	mutable std::vector<particles::draw::LightStamp> _landStamps;
	mutable std::vector<particles::draw::Command> _particleCommands;
	mutable std::vector<uint32_t> _particleSpriteOrder;
	/// What the effect over a seed of the gestures' hints draws, and its order
	mutable particles::draw::Frame _hintParticleFrame;
	/// The camera the gestures' hints picture their seeds through, made the first time a hint pictures one
	mutable std::unique_ptr<Camera> _hintCamera;
	mutable std::vector<particles::draw::Command> _hintParticleCommands;
	mutable std::vector<uint32_t> _hintParticleSpriteOrder;
	mutable std::vector<particles::draw::Command> _particleBeforeLandCommands;
	mutable std::vector<uint32_t> _particleBeforeLandSprites;
	/// What the falling spell's film draws of the particles this frame, and their order
	mutable particles::draw::Frame _filmParticleFrame;
	mutable std::vector<particles::draw::Command> _filmParticleCommands;
	mutable std::vector<uint32_t> _filmSpriteOrder;
	/// The particle light maps' frames stamped so far, by light map and frame
	mutable std::unordered_map<uint64_t, std::unique_ptr<Texture2D>> _particleLightMaps;
	/// The glowing shells' bands made so far, by model and part; none for a part that has no triangles to glow
	mutable std::unordered_map<uint64_t, std::unique_ptr<VolumeGlowShell>> _glowShells;
};
} // namespace graphics
} // namespace openblack
