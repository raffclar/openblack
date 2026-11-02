/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "3D/DesignedWaterfall.h"
#include "Common/FramePace.h"
#include "ECS/Systems/DesignedWaterfallSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class DesignedWaterfallSystem final: public DesignedWaterfallSystemInterface
{
public:
	void Reset() override;
	void UpdateFrame(int32_t landNumber, std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime,
	                 bool landDrawn) override;
	[[nodiscard]] bool HasModel() const override { return ModelShown(); }
	[[nodiscard]] float GetSlide() const override { return _slide; }
	[[nodiscard]] std::optional<uint32_t> GetSound() const override;

private:
	/// Whether the model is a thing of the drawn registry now: the drawn registry loses its things as the land clears
	[[nodiscard]] bool ModelShown() const;
	void MakeModel();
	void RemoveModel();
	void MakeSound();
	void RemoveSound();
	/// The model shows the snow where it stands and the slide of its water
	void ShowModel() const;

	designed_waterfall::Presence _presence;
	entt::entity _model {entt::null};
	uint32_t _sound {0};
	/// The water's slide and the clock of the spray. The game keeps both from one land to the next.
	float _slide {0.0f};
	float _sprayClock {0.0f};
	frame_pace::Pace _pace;
};

} // namespace openblack::ecs::systems
