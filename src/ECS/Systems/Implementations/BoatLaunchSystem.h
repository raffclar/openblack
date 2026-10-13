/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include "ECS/Systems/BoatLaunchSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class BoatLaunchSystem final: public BoatLaunchSystemInterface
{
public:
	void Play() override;
	void Play(const Places& places) override;
	void Stop() override;
	void Update(uint32_t milliseconds) override;
	void CollectDrawFrame(particles::draw::Frame& frame) const override;
	[[nodiscard]] bool IsPlaying() const override { return _playing; }
	[[nodiscard]] State GetState() const override;

private:
	/// The things of the launch (the boat and its five sailors) or of the sailing (the boat and its deck)
	void MakeParts();
	void FreeParts();
	/// The boat's matrix for this frame, from its clip
	[[nodiscard]] std::optional<glm::mat4> BoatMatrix() const;
	/// The sounds and spray of the launch at this frame
	void LaunchEffects();
	/// Every part placed and posed
	void PlaceParts();

	bool _playing {false};
	Places _places;
	boat_launch::Clocks _clocks;
	/// The two clips, in the animation cache
	entt::id_type _launchClip {0};
	entt::id_type _sailingClip {0};
	std::optional<glm::mat4> _boat;
	std::vector<entt::entity> _parts;
};

} // namespace openblack::ecs::systems
