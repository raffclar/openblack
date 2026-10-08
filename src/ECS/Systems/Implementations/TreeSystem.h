/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TreeSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The trees' shared state, kept for the whole game (never cleared)
class TreeSystem final: public TreeSystemInterface
{
public:
	[[nodiscard]] uint8_t Brightness() const override;
	void SetBrightness(uint8_t brightness) override;

	[[nodiscard]] bool AnyBent() const override;
	void SetAnyBent(bool bent) override;

	void AddDeletedListener(DeletedListener listener) override;
	[[nodiscard]] const std::vector<DeletedListener>& DeletedListeners() const override;

private:
	uint8_t _brightness {255};
	/// true until a bend pass has run: the first one always runs
	bool _anyBent {true};
	std::vector<DeletedListener> _deletedListeners;
};
} // namespace openblack::ecs::systems
