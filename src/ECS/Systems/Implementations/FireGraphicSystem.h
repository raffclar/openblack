/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/FireGraphicSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The fire graphics kept for the whole game; magic::OnLoadMap empties them for every land (fire::graphic::Clear)
class FireGraphicSystem final: public FireGraphicSystemInterface
{
public:
	[[nodiscard]] Graphics& All() override;
	void Set(uint32_t fire, std::unique_ptr<fire::graphic::Graphic> graphic) override;
	void Erase(uint32_t fire) override;
	void Clear() override;

	[[nodiscard]] bool SourceAdded() const override;
	void SetSourceAdded() override;

private:
	Graphics _graphics;
	bool _sourceAdded {false};
};
} // namespace openblack::ecs::systems
