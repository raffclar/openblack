/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/MoonSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class MoonSystem final: public MoonSystemInterface
{
public:
	/// The computer's date is read again only once more than this has gone by since it was last read
	static constexpr std::chrono::milliseconds k_DateReadInterval {2000};

	void Update(const MoonFrame& frame) override;

	[[nodiscard]] std::optional<graphics::moon::Placement> GetPlacement() const override { return _placement; }
	[[nodiscard]] float GetStrength() const override { return _strength; }
	[[nodiscard]] float GetPhase() const override { return _phase; }
	[[nodiscard]] float GetShownPhase() const override { return _shownPhase; }
	[[nodiscard]] float GetScriptPercentage() const override;

	[[nodiscard]] int64_t GetDate() const override;
	void SetDateOverride(std::optional<int64_t> date) override;
	[[nodiscard]] std::optional<int64_t> GetDateOverride() const override { return _dateOverride; }

private:
	std::optional<graphics::moon::Placement> _placement;
	float _strength {0.0f};
	float _phase {0.0f};
	float _shownPhase {0.0f};
	/// The computer's date as last read, and when it was read
	int64_t _readDate {0};
	std::chrono::milliseconds _readAt {0};
	std::optional<int64_t> _dateOverride;
};

} // namespace openblack::ecs::systems
