/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "Help/HelpProfile.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HelpProfileSystem final: public HelpProfileSystemInterface
{
public:
	void Trigger(uint32_t event) override;
	void ProcessTurn() override;

	[[nodiscard]] const help::profile::HelpProfile& Get() const override { return _profile; }
	[[nodiscard]] help::profile::HelpProfile& Get() override { return _profile; }

private:
	help::profile::HelpProfile _profile;
};

} // namespace openblack::ecs::systems
