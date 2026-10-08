/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/Clouds.h"
#include "ECS/Systems/AlignmentSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The alignment service over the game's own alignment code. The players' alignments are the player system's
/// (ecs::effects::alignment), and the camera's is the interface alignment the players' turn works out. It keeps no
/// alignment of its own but the sky's, which it moves the way the renderer moves its sky.
class AlignmentSystem final: public AlignmentSystemInterface
{
public:
	[[nodiscard]] float GetPlayerAlignment(PlayerNames player) const override;
	void SetPlayerAlignment(PlayerNames player, float alignment) override;
	void AddPlayerAlignment(PlayerNames player, float change) override;
	void AddPendingAlignment(PlayerNames player, float change) override;
	[[nodiscard]] float GetPendingAlignment(PlayerNames player) const override;
	void UpdateTurn() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] float GetCameraAlignment() const override;
	[[nodiscard]] float GetSkyAlignment() const override { return _sky.Get(); }

private:
	SkyAlignment _sky;
};

} // namespace openblack::ecs::systems
