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
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "InstanceDesc.h"
#include "RenderPass.h"
#include "ScreenshotCapture.h"

#include "../EngineConfig.h"

namespace openblack
{
class Camera;
class Profiler;
class Sky;
class Ocean;
} // namespace openblack

namespace openblack::ecs
{
class Registry;
}

namespace openblack::graphics
{
class L3DMesh;
class Texture2D;
class FrameBuffer;
class ShaderManager;
class ShaderProgram;

class RendererInterface
{
public:
	struct DrawSceneDesc
	{
		const Camera* camera;
		const graphics::FrameBuffer* frameBuffer;
		const ecs::Registry& entities;
		uint32_t time;
		float timeOfDay;
		float smallBumpMapStrength;
		graphics::RenderPass viewId;
		bool drawSky;
		bool drawWater;
		bool drawIsland;
		bool drawEntities;
		bool drawSprites;
		bool drawVegetation;
		bool drawBoundingBoxes;
		bool cullBack;
		bool wireframe;
		/// The hand and its shadow, hidden while a dialog shows its own pointer
		bool drawHand = true;
	};

	struct L3DMeshSubmitDesc
	{
		graphics::RenderPass viewId;
		/// Every primitive is added over what is behind it by this share, whatever its material says
		std::optional<float> additiveShare;
		/// Every primitive is blended over what is behind it by each instance's own alpha, whatever its material says
		bool instanceAlpha {false};
		/// Every primitive drops its fragments at or under this alpha, of 1, whatever its material says, when given
		std::optional<float> alphaThreshold;
		/// Every primitive drops its fragments under its material's alpha threshold, even where its material doesn't
		bool materialAlphaTest {false};
		const graphics::ShaderProgram* program;
		/// Draws the submeshes that have a lightmap, with it bound to s_lightmap, when set
		const graphics::ShaderProgram* lightmapProgram;
		uint64_t state;
		uint32_t rgba;
		const glm::mat4* modelMatrices;
		uint8_t matrixCount;
		/// Each instance's own bones, for a program that takes them from a texture (s_bones), with where the draw's
		/// first are, how many each instance has and the texture's size (u_bones)
		const graphics::Texture2D* boneTexture {nullptr};
		glm::vec4 bones {0.0f};
		std::unique_ptr<const graphics::InstanceDesc> instanceDesc;
		uint32_t instanceStart;
		uint32_t instanceCount;
		bool isSky;
		bool drawAll; ///< For use in the mesh viewer
		bool morphWithTerrain;
		/// Blend and write depth as each primitive's material says, instead of drawing it opaque
		bool useMaterialBlending;
		/// The table of joints the submeshes with joints turn by, about their pivots (the temple's doors)
		std::span<const glm::mat4> joints;
		/// Draws only the submeshes with joints
		bool onlyJoints;
		/// Leaves out the submeshes with joints while their joints don't turn them
		bool hideShutJoints;
		/// Culls the back faces of the primitives whose materials aren't two-sided, which the mirrored reflection pass
		/// sees from the other side
		bool useMaterialCulling;
		bool mirrored;
		/// The fraction of its depth the mesh is pushed back by
		float depthBias;
		/// Where the mesh comes in a pass sorted by depth, the greatest first
		uint32_t sortDepth;
		/// How far the mesh's texture has slid across it, after being scaled by uvScale
		glm::vec2 uvOffset;
		float uvScale {1.0f};
		/// The texture slides only on the primitives whose materials let it, as the game slides a model's skin (the
		/// creature spells' phials); otherwise on every primitive
		bool uvOffsetByMaterial {false};
		/// A texture every primitive with a skin is drawn with in place of it, when set, as the game gives the creature
		/// room's icons
		const TextureHandle* skinTexture;
		/// The environment map the mesh is drawn with where its program takes one (s_environment), each primitive taking
		/// it as its material says
		const Texture2D* environment;
		/// The villagers' bones, read by the instances of a program that takes them (s_bonePalette)
		const Texture2D* bonePalette {nullptr};
		/// Above 0, the mesh is drawn as its environment map alone in its light, at this alpha, where its program takes
		/// one: a frozen thing's ice shining over it
		float environmentOnlyAlpha {0.0f};
		/// Textures some of the submeshes are drawn with in place of their skins, by submesh
		std::span<const std::pair<uint32_t, TextureHandle>> subMeshTextures;
		/// Colours added to some of the submeshes, by submesh, after everything else
		std::span<const std::pair<uint32_t, glm::vec3>> subMeshGlows;
		/// Submeshes left undrawn
		std::span<const uint32_t> hiddenSubMeshes;
		/// The primitives of the alpha textured materials (4 and 5) are drawn added over what is behind without writing
		/// depth (13), as the physical shield's dome is made
		bool alphaTexturedAdditive {false};
		/// A colour the mesh is drawn in, where its program takes one: lit when w is 0, otherwise unlit with its alpha by w
		glm::vec4 tint {1.0f, 1.0f, 1.0f, 0.0f};
		/// A piece drawn on another object, as a detailed villager's eyes are, where its program takes it: xyz the point
		/// whose land light, land colour and haze it takes, w its shade of 255 in place of the sun's on each vertex. w 0 for
		/// none.
		glm::vec4 shadeAt {0.0f};
		/// Inside the temple, the mesh fades with the temple's depth fog: a model once where it stands, the lightmapped
		/// walls at each vertex. Off for the hand, which the game leaves unfogged.
		bool templeFog {false};
		/// The temple's origin, for a room's mesh: its walls without a lightmap are fogged once each at their first
		/// vertex, placed from here as the game reads it in the temple's own space
		std::optional<glm::vec3> fogFromFirstVertex;
		/// The temple's light, which the lightmapped submeshes are multiplied by, and which is added to every submesh
		/// that doesn't glow, as the game adds the vertices' specular
		glm::vec3 lightMultiply {1.0f};
		/// How much brighter than the land's light where it stands the mesh is, at most white: the god hand is half as
		/// bright again
		float landLightScale {1.0f};
		/// The mesh isn't shaded by the sun, only coloured by the land's light where it stands
		bool unlit {false};
		/// The sun lights the mesh at every detail level, as it does the creatures and the hand
		bool alwaysLit {false};
		/// The mesh is shaded as the game shades the models it lights as it poses them, the glow over a hand holding a
		/// miracle among them: 1.3 n.l + 0.4, from 0.4 to 1, in place of the usual model light
		bool posedModelLight {false};
		/// Where the model light is for this mesh in place of where it is this frame, as the creature in the temple's
		/// creature room is lit from the room's fire
		std::optional<glm::vec3> modelLight;
		/// The land's light and colour where the mesh stands and the world's distance haze reach it. Off for a mesh drawn
		/// in a colour of its own alone, with nothing added over it, as the columns of light over the temples and the
		/// village centres are
		bool landLightAndHaze {true};
		/// The land's colour where the mesh stands is added over it after its light. Off for a mesh lit in a colour of its
		/// own that the land's colour never reaches, as the shrinking and growing phials are
		bool landColourAdded {true};
		/// The world's distance haze fades it and adds its colour. Off for the explorers' boat in the sea, which is mirrored
		/// in its own colour and the land's alone, however far off
		bool hazed {true};
		/// Nothing of the mesh is drawn below this height, when given, where its program takes it
		std::optional<float> cutBelow;
		/// Nothing of the mesh is drawn above this height, when given, where its program takes it: a building drawn as far
		/// as it is built
		std::optional<float> cutAbove;
		/// Every primitive is drawn from both sides
		bool twoSided {false};
		/// The building's inner walls: each vertex moved in across the ground along its normal, by less for a two-sided
		/// material than for another
		bool innerWalls {false};
		/// How far the inner walls stand in whatever the material, when given: a temple's
		std::optional<float> innerWallInset;
		/// The detail the mesh is drawn in, as the bit its parts name it by (see graphics::mesh_detail::LevelBit): only its
		/// parts of that detail are drawn. Its most detailed by default.
		uint8_t detailLevel {1};
		/// Only the submeshes of this status are drawn, in place of those of status 0: a building's scaffold
		std::optional<uint32_t> onlyStatus;
		/// Only the submeshes of status one up to this are drawn, in place of those of status 0: a graveyard's graves
		std::optional<uint32_t> statusesUpTo;
		/// Where a building's model is cut, in its own space: its inner walls and the cap over them are drawn only for a
		/// primitive with a whole triangle below it
		std::optional<float> modelCutHeight;
		/// In place of its triangles, each primitive draws the cap over its walls where they are cut, unlit in three
		/// quarters of the object's colour
		bool cap {false};
		/// An object's own colour and alpha, as the game gives some objects: the sun shades the colour (0 to 255) in place
		/// of the land's light where it stands, and every primitive's alpha is its texture's times the alpha (0 to 1).
		/// Less than whole, the primitives that would be drawn opaque blend by it.
		struct ObjectLook
		{
			glm::vec3 colour {255.0f};
			float alpha {1.0f};
		};
		std::optional<ObjectLook> objectLook;
		/// The snow lying where the mesh stands shows on it
		bool snow {false};
		/// The creatures' and the hand's moving shadows fall on the mesh, where its program takes them: not on the
		/// creatures themselves, the hand or the villagers a script draws in high detail
		bool groundShadows {true};
		glm::vec3 lightAdd {0.0f};
		/// A creature's body, blended from its base mesh, which is drawn, towards other meshes of the same shape: the
		/// mesh each of the evil to good, thin to fat and weak to strong axes pulls towards, and how far
		struct MorphTargets
		{
			std::array<const L3DMesh*, 3> meshes;
			glm::vec3 weights;
			/// The skins the body is drawn with in place of its base mesh's, by skin id: blended towards how evil or
			/// good it is
			std::span<const std::pair<uint32_t, const Texture2D*>> skins;
			/// Draws the vertices at the seams blended towards their partners (see vertex_blend)
			bool blendSeams {true};
		};
		const MorphTargets* morphTargets {nullptr};
		/// A creature fizzing out of sight, drawn twice through the static scrolling over its skin (creature_fizz_look):
		/// first its depth alone where the static is strong enough, then its body blended over that depth
		struct Fizz
		{
			enum class Pass : uint8_t
			{
				Depth,
				Body,
			};
			Pass pass;
			/// How far it has fizzed, as a byte
			uint8_t level;
			/// How far the static has slid across its skin
			glm::vec2 scroll;
		};
		std::optional<Fizz> fizz;
		/// How frozen the mesh is drawn, 0 to 1, as a piece of a frozen creature that isn't instanced: sheened with ice by
		/// it, where it isn't fizzing
		float freeze {0.0f};
	};

	static std::unique_ptr<RendererInterface> Create(GraphicsBackend backend, bool vsync) noexcept;

	virtual ~RendererInterface() noexcept = default;

	virtual void ConfigureView(graphics::RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept = 0;
	virtual void Reset(glm::u16vec2 resolution) const noexcept = 0;
	virtual void DrawScene(const DrawSceneDesc& drawDesc) const noexcept = 0;
	virtual void Frame() noexcept = 0;
	/// Bakes the shadows of the things on the land into the land, as the next scene draw does. Call it behind the
	/// loading screen, so that the first frame of a land does not do the bake of the whole island.
	virtual void BakeLandShadows(bool drawVegetation) const noexcept = 0;
	/// Makes textures for the painted skins of count skins. Call it behind the loading screen, so that the first frame
	/// of a creature does not make them.
	virtual void PrepareCreatureSkins(size_t count) const noexcept = 0;
	/// Draws a scene as DrawScene does, but only so that the renderer makes its pipelines and render targets: the next
	/// frame shows none of it, and nothing that later frames draw changes. Call it behind the loading screen, before
	/// the draws of the loading frame, so that the first frames of the game do not make them.
	virtual void WarmUpScene(const DrawSceneDesc& drawDesc) const noexcept = 0;
	/// Asks for the frame being made to be written to a PNG once drawn; the file appears whole once written
	virtual void RequestScreenshot(const std::filesystem::path& filepath) noexcept = 0;
	/// Why a picture asked for was given up, once; none while it may still come
	[[nodiscard]] virtual std::optional<std::string> TakeScreenshotFailure(const std::filesystem::path& filepath) noexcept = 0;
	/// Asks for a picture of the frame that the game makes now, kept in memory under a name. The name must start with
	/// graphics::ScreenPictures::k_Prefix. The renderer calls picturer on its own thread when it reads the frame.
	virtual void RequestScreenPicture(const std::string& name, graphics::ScreenPicturer picturer) noexcept = 0;
	/// The picture made for a name, one time; none while it is not made
	[[nodiscard]] virtual std::optional<std::vector<uint8_t>> TakeScreenPicture(const std::string& name) noexcept = 0;
	/// How many pipelines the renderer made since it started. The renderer makes a pipeline the first time that a draw
	/// uses a new set of program, state, vertex layouts and target formats, and that frame is slower.
	[[nodiscard]] virtual uint32_t GetPipelinesMade() const noexcept = 0;
	[[nodiscard]] virtual bool GetDebug() const noexcept = 0;
	virtual void SetDebug(bool value) noexcept = 0;
	[[nodiscard]] virtual bool GetProfile() const noexcept = 0;
	virtual void SetProfile(bool value) noexcept = 0;

	// TODO: Remove this function. All renderables should be drawn through RenderingSystem with Components
	virtual void DrawMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept = 0;
	// TODO: Should shader manager be available through Locator as a service?
	[[nodiscard]] virtual graphics::ShaderManager& GetShaderManager() const noexcept = 0;
};

} // namespace openblack::graphics
