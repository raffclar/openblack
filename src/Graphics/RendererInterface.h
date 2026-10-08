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

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GraphicsHandle.h"
#include "InstanceDesc.h"
#include "RenderModes.h"
#include "RenderPass.h"
#include "SeaPass.h"

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

namespace openblack::help
{
enum class TextFont : uint8_t;
}

namespace bgfx
{
struct InstanceDataBuffer;
}

namespace openblack::graphics::creature_draw
{
struct MorphTargets;
}

namespace openblack::graphics
{
class L3DMesh;
class FrameBuffer;
class ShaderManager;
class ShaderProgram;
struct OverlayFrame;
struct SuperVillagerFrame;

class RendererInterface
{
public:
	/// The game clocks of the frame the draw reads (openblack engine), taken with the camera's copy where the desc is
	/// made (Game::Run, at the end of the frame's logic). (pending) the real-time reads (the sea's
	/// game_clock::EngineMs, DrawMoon's MoonPhase(UnixTime)) are still made live in the draw, as before
	struct DrawClock
	{
		uint32_t frameGameMs; ///< game_clock::FrameGameMs: the game time step, whole ms, 0 paused
		uint32_t turn;        ///< game_clock::Turn
		float turnFraction;   ///< game_clock::TurnFraction
		bool paused;          ///< game_clock::IsPaused (no draw reader yet: the tooltip's)
	};

	/// The hand of the frame the draw reads, taken with the camera's copy and DrawClock
	struct DrawHand
	{
		std::optional<glm::vec3> position; ///< HandSystem::GetHandMatrix's translation (none: no hand system)
	};

	struct DrawSceneDesc
	{
		/// The draw's copy of the camera (Camera::CopyViewTo), not Locator::camera
		const Camera* camera;
		const graphics::FrameBuffer* frameBuffer;
		const ecs::Registry& entities;
		/// What the end of the frame draws over the scene (Graphics/OverlayFrame.h), filled before the draw
		const graphics::OverlayFrame& overlay;
		/// What the SuperVillagers' draw reads (Graphics/SuperVillagerFrame.h), filled before the draw
		const graphics::SuperVillagerFrame& superVillagers;
		uint32_t time;
		float timeOfDay;
		DrawClock clock;
		DrawHand hand;
		float bumpMapStrength;
		float smallBumpMapStrength;
		graphics::RenderPass viewId;
		bool drawSky;
		bool drawWater;
		bool drawIsland;
		bool drawEntities;
		bool drawSprites;
		bool drawBoundingBoxes;
		bool wireframe;
	};

	struct L3DMeshSubmitDesc
	{
		graphics::RenderPass viewId;
		const graphics::ShaderProgram* program;
		/// What the draw adds to every primitive's mode (render_modes::State): Z func, alpha written, MSAA, a cull for the
		/// whole mesh (the sky, the hand's shadow); without one, each primitive's material culls
		render_modes::StateOptions options;
		/// The render mode table: GlobalAlpha for an object drawn with its own alpha
		render_modes::Table table {render_modes::Table::Normal};
		/// with the GlobalAlpha table: the object's alpha byte (the diffuse colour >> 24), the ALPHAREF of modes 9 / 15
		uint8_t globalAlpha {255};
		/// every primitive drawn in this mode instead of its own, alpha test included (the particle additive atoms: 13;
		/// the hand's shadow on the objects: 6)
		std::optional<render_modes::Mode> mode;
		uint32_t rgba;
		const glm::mat4* modelMatrices;
		uint8_t matrixCount;
		std::unique_ptr<const graphics::InstanceDesc> instanceDesc;
		uint32_t instanceStart;
		uint32_t instanceCount;
		bool isSky;
		bool drawAll; ///< For use in the mesh viewer
		bool morphWithTerrain;
		float lightBoost {1.0f};   ///< model colour multiplier (the hand: 1.5)
		bool noHaze {false};       ///< no distance haze (the hand's draw never applies it)
		uint8_t landLightMode {0}; ///< land_light::ObjectMode: how the model takes the land light (vs_object)
		uint8_t blendFilter {0};   ///< 0: every primitive, 1: the opaque ones only, 2: the blended ones only
		/// A draw of the pass under the sea (graphics::sea_pass): DrawUnderWater in a constant or the last Draw's
		/// colour, DrawCutByPlane, the plane kept and whether it is mirrored back
		sea_pass::SeaDraw sea {};
		/// A projected shadow drawn on the object (programs *ShadowInstanced, shadow_list): its texture, its box
		/// (x0, z0, 1 / (x1 - x0), 1 / (z1 - z0)) and its cull values (d.x, d.z, the least k)
		std::optional<graphics::TextureHandle> dynamicShadow;
		glm::vec4 dynamicShadowBox {0.0f};
		glm::vec4 dynamicShadowCull {0.0f};

		// The temple's (Graphics/RendererTemple.cpp): the world's draws leave them as they are and never read them
		/// The table of joints the submeshes with a joint turn by about their pivots, as the temple's doors do
		std::span<const glm::mat4> joints {};
		/// The temple's light: what every colour is multiplied by, and what is added after
		glm::vec3 lightMultiply {1.0f};
		glm::vec3 lightAdd {0.0f};
		/// How far back the mesh is pushed towards the far plane, as a fraction of its depth
		float depthBias {0.0f};
		/// The program the submeshes with a lightmap are drawn with, through it
		const graphics::ShaderProgram* lightmapProgram {nullptr};

		// A creature's (Graphics/CreatureDraw.h, RendererCreature.cpp): the world's other draws leave them as they are
		/// The meshes the body's shape is blended towards, read as the second to fourth vertex streams by the program
		/// "ObjectMorphInstanced"
		const graphics::creature_draw::MorphTargets* morphTargets {nullptr};
		/// The creature whose painted skins take the place of the mesh's own
		entt::entity paintedSkins {entt::null};
		/// A row of the instances made for this draw alone, in place of instanceDesc's (a creature's eyelid, tinted)
		const bgfx::InstanceDataBuffer* transientInstance {nullptr};
	};

	static std::unique_ptr<RendererInterface> Create(GraphicsBackend backend, bool vsync) noexcept;

	virtual ~RendererInterface() noexcept = default;

	virtual void ConfigureView(graphics::RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept = 0;
	virtual void Reset(glm::u16vec2 resolution) const noexcept = 0;
	/// (openblack engine) a new land: what the draw would load lazily (the chimney smoke's alpha)
	virtual void PreloadForLand() const noexcept = 0;
	/// (openblack engine) the frame's writes of the draw, on the logic side, just before DrawScene, which then only
	/// reads: the sky type, the dome, the land light table, the frame's light, the dynamic shadows, the clouds (and
	/// their collection), the haze, the foliage, the PSys surface creators, the mists, the chimney smoke, the rain and
	/// the influence border
	virtual void PreDraw(const DrawSceneDesc& drawDesc) const noexcept = 0;
	virtual void DrawScene(const DrawSceneDesc& drawDesc) const noexcept = 0;
	/// The size of the Main view (ConfigureView), what the overlays are laid out for; 0 x 0 before it.
	/// Called from the logic side (the OverlayFrame filled before the draw): read-only, no bgfx calls
	[[nodiscard]] virtual glm::u16vec2 GetResolution() const noexcept = 0;
	/// The string width of a HelpText font (f1 / f3 fall back to j0): the width
	/// of the text at that size, 0 without the font. For the layouts made before the draw (Graphics/OverlayFrame.h).
	/// Called from the logic side: read-only, no bgfx calls; the font's first-use load (GameFontAt) must move before
	/// the game or go through gpu::Submit (pending)
	[[nodiscard]] virtual float MeasureText(help::TextFont font, std::u16string_view text, float size) const noexcept = 0;
	virtual void Frame() noexcept = 0;
	virtual void RequestScreenshot(const std::filesystem::path& filepath) noexcept = 0;
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
