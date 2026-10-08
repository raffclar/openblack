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

#include <memory>
#include <unordered_map>

#include "ECS/Fire/FireGraphicData.h"

namespace openblack::ecs::systems
{
/// The game's fire graphics, by fire handle, and whether their draw has been handed to the particle manager
/// (ecs::fire::graphic goes through it)
class FireGraphicSystemInterface
{
public:
	using Graphics = std::unordered_map<uint32_t, std::unique_ptr<fire::graphic::Graphic>>;

	virtual ~FireGraphicSystemInterface() = default;

	/// Every fire's graphic, by its fire's handle
	[[nodiscard]] virtual Graphics& All() = 0;
	/// The fire's graphic is this one (an older one goes)
	virtual void Set(uint32_t fire, std::unique_ptr<fire::graphic::Graphic> graphic) = 0;
	/// The fire's graphic goes
	virtual void Erase(uint32_t fire) = 0;
	/// A land is loaded: no graphics. The draw stays handed over
	virtual void Clear() = 0;

	/// The draw of the fires was handed to the particle manager (once per game)
	[[nodiscard]] virtual bool SourceAdded() const = 0;
	virtual void SetSourceAdded() = 0;
};
} // namespace openblack::ecs::systems
