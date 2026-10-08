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
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"
#include "3D/LandLight.h"
#include "Camera/ScreenPoint.h"
#include "Help/HelpDudeFile.h"
#include "Help/SpiritAnimClip.h"
#include "Help/Spirits.h"

// The two advisor spirits wired to the engine: the models of Data\HelpSprite\markgood.hd / markevil.hd, the
// AdvisorSpiritController of Help/Spirits.h with its Queries answered by the camera, the land, audio::advisor and game_random,
// and, once a frame, the anim layers turned into the skinning matrices plus what the spirits and their trails draw,
// written once a frame into the frame's graphics::OverlayFrame before the draw (FillOverlay;
// Graphics/RendererSpirits.cpp draws only that copy). HelpSystem owns the control in the original; here it is
// this module's, started next to help::Start.
namespace openblack::graphics
{
class L3DMesh;
struct OverlayFrame;
struct SpiritOverlay;
} // namespace openblack::graphics

namespace openblack::help::spirits
{

/// One .hd and what is built from it
struct DudeAssets
{
	HelpDudeFile file;
	RestSkeleton rest;
	DudeData data;
	bool loaded {false};
	/// The face bones of the .hd: eye bones 0..2, 3..5, then the head
	std::array<uint32_t, 7> faceBones {};
	/// The mesh of the embedded L3D0, made on the first FillOverlay; shared with the
	/// frame's OverlayFrame so the draw keeps it alive
	std::shared_ptr<const graphics::L3DMesh> mesh;
	bool meshTried {false};
};

/// The rainbow trail of a dude
struct Trail
{
	static constexpr size_t k_Points = 32;
	std::array<glm::vec3, k_Points> ring {}; ///< (hx, hy, depth channel)
	size_t head {0};
	float accumulator {0.0f};
	uint32_t resets {0}; ///< the logic's TrailResets() last seen
};

/// One vertex of the trail (a world triangle vertex, ARGB)
struct TrailVertex
{
	glm::vec3 position {0.0f};
	glm::vec2 uv {0.0f};
	uint32_t argb {0};
};

/// The trail's texture coordinates as the original's trail draw sets them: u runs along the trail (point / 32), v across
/// it, 1 (good) or 0 (evil) on the first side and 0.5 on the second
[[nodiscard]] glm::vec2 TrailUv(size_t point, bool second, bool good);
/// The trail's triangle list as the original's: from the newest segment back, (A j, B j, B j+1) and (B j+1, A j+1, A j),
/// where A j = 2 j and B j = 2 j + 1 are the two vertices of point j
[[nodiscard]] std::array<uint16_t, 6 * (Trail::k_Points - 1)> TrailIndices();

class Runtime
{
public:
	/// Both .hd through Locator::filesystem (Data\HelpSprite), then the control (both dudes at home)
	Runtime();
	~Runtime();
	Runtime(const Runtime&) = delete;
	Runtime& operator=(const Runtime&) = delete;

	[[nodiscard]] AdvisorSpiritController& Control() { return *_control; }
	[[nodiscard]] const AdvisorSpiritController& Control() const { return *_control; }
	[[nodiscard]] bool Loaded(int dude) const { return _assets.at(static_cast<size_t>(dude)).loaded; }
	/// The dude's .hd as loaded, nullptr without it
	[[nodiscard]] const HelpDudeFile* File(int dude) const
	{
		const auto& assets = _assets.at(static_cast<size_t>(dude));
		return assets.loaded ? &assets.file : nullptr;
	}

	/// The control's per-frame process (real frame time in seconds, 0.4), once a frame; then the pose of each dude and
	/// the trail ring (the puff particles are the logic's, AdvisorSpirit::PuffParticles)
	void Update();
	/// The process of both spirits, once a game turn
	void ProcessTurn();

	/// The mesh of a dude, made on first use (it needs the renderer); nullptr without the .hd
	[[nodiscard]] std::shared_ptr<const graphics::L3DMesh> Mesh(int dude);
	/// The pre-draw step (Game.cpp FillOverlayFrame, once a frame before DrawScene): the frame's spirits, Draws(false)
	/// then Draws(true), and the trails, copied into `frame` (its spirits and spiritTrails replaced)
	void FillOverlay(graphics::OverlayFrame& frame);
	/// The trail of each dude out of home and not in the world: 64 vertices each, in the order of the index list
	/// (three per triangle)
	[[nodiscard]] std::vector<TrailVertex> TrailTriangles() const;

	/// The camera of the last Update
	[[nodiscard]] const graphics::billboard::CameraFrame& Frame() const { return _frame; }

	/// The colour and specular of a dude in the world: sb = smooth(in-world blend), w = int(255 sb); colour = white
	/// lerped to the land diffuse by w (land_light::LerpBytes), specular = the land specular x w >> 8; sb 0: (white, 0).
	/// Pure: the Renderer applies it with its land light table (graphics::SpiritOverlay::landLightPoint)
	[[nodiscard]] static std::pair<uint32_t, uint32_t> WorldColour(const land_light::Sample& sample, float blend);

private:
	/// The dudes the original draws this frame, appended to `out`: `overlay` the end of frame callback's (in-world
	/// blend < 0.5), else the world's (>= 0.5). Advances the halo clock
	void Draws(bool overlay, std::vector<graphics::SpiritOverlay>& out);
	/// The anim layers 0..count of a dude applied to its rest locals, then composed under `model`
	void EvaluatePose(int dude, size_t count, const glm::mat4& model, std::vector<glm::mat4>& world) const;
	/// The head's yaw and pitch towards a target, nothing when it is behind
	[[nodiscard]] std::optional<glm::vec2> HeadAngles(int dude, const glm::mat3& rows, const glm::vec3& position,
	                                                  const glm::vec3& target) const;
	/// The fingertip of a dude in the world
	[[nodiscard]] glm::vec3 Fingertip(int dude, const glm::mat3& rows, const glm::vec3& position) const;
	Queries MakeQueries();
	void RefreshView();
	void UpdateTrail(int dude, uint32_t deltaMs);

	std::array<DudeAssets, k_Dudes> _assets;
	std::unique_ptr<AdvisorSpiritController> _control;
	std::array<std::vector<glm::mat4>, k_Dudes> _bones;
	std::array<Trail, k_Dudes> _trails {};
	int32_t _haloClockMs {0}; ///< one for both
	graphics::billboard::CameraFrame _frame;
	screen_point::Lens _lens {320.0f, 240.0f, 1.0f, 4.0f / 3.0f};
	Screen _screen {};
};

/// The control of the running game, nullptr before Start / after Shutdown
[[nodiscard]] Runtime* Get();
void Start();
void Shutdown();

/// The model matrix of the rows (row i in [i]) and the position: glm's columns are the original's rows
[[nodiscard]] glm::mat4 ModelOf(const glm::mat3& rows, const glm::vec3& position);

} // namespace openblack::help::spirits
