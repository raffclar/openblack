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

#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "Enums.h"
#include "Particles/PSys.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// Shows the miracle tables (costs, timers and seeds of every magic type), and starts any particle type at the hand or
/// where the camera looks
class Magic final: public Window
{
public:
	Magic() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void DrawSelected() noexcept;
	void DrawTable() noexcept;
	void DrawParticles() noexcept;
	void DrawRunningEffects() noexcept;
	/// Where a started effect goes: the hand, or what the camera looks at, on the land
	[[nodiscard]] glm::vec3 SpawnPoint() const noexcept;

	MagicType _selected {MagicType::Fireball};

	// The particles' tab
	enum class SpawnAt : uint8_t
	{
		CameraFocus,
		Hand,
	};
	ParticleType _particleType {ParticleType::Smoke};
	SpawnAt _spawnAt {SpawnAt::CameraFocus};
	float _spawnHeight {0.0f};
	float _magnitude {1.0f};
	psys::DrawPath _drawPath {psys::DrawPath::Sorted};
	/// The effects this window started, to close down or delete
	struct Started
	{
		uint32_t id;
		std::string file;
	};
	std::vector<Started> _started;
};

} // namespace openblack::debug::gui
