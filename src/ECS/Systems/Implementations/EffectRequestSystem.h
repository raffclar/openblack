/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>

#include "ECS/Systems/EffectRequestSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class EffectRequestSystem final: public EffectRequestSystemInterface
{
public:
	/// Finds the side that shows the effects, none on a computer that shows nothing
	using FindServer = std::function<EffectServerInterface*()>;

	explicit EffectRequestSystem(FindServer findServer);

	EffectHandle Start(const EffectRequest& request) override;
	void Show(const EffectRequest& request) override;
	void Move(EffectHandle handle, glm::vec3 position) override;
	void Stop(EffectHandle handle) override;
	void Remove(EffectHandle handle) override;
	void Reset() override;
	[[nodiscard]] EffectHandle NextHandle() const override { return _next; }

	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

private:
	FindServer _findServer;
	EffectHandle _next {k_FirstEffectHandle};
};

} // namespace openblack::ecs::systems
