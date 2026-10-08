/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_map>

#include "ECS/Systems/FireEffectSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The fires kept for the whole game; magic::OnLoadMap empties them for every land (fire::Clear)
class FireEffectSystem final: public FireEffectSystemInterface
{
public:
	[[nodiscard]] fire::FireEffect* FindByObject(entt::entity object) override;
	[[nodiscard]] fire::FireEffect* FindById(uint32_t id) override;

	[[nodiscard]] uint32_t TakeId() override;
	[[nodiscard]] uint8_t TakeCreateTag() override;
	[[nodiscard]] uint8_t ProcessTag() const override;
	void SetProcessTag(uint8_t tag) override;

	fire::FireEffect& Insert(std::unique_ptr<fire::FireEffect> fire) override;
	void Unlist(const fire::FireEffect& fire) override;
	void ForgetObject(entt::entity object) override;
	void ForgetId(uint32_t id) override;
	void MoveObject(entt::entity from, entt::entity to, fire::FireEffect& fire) override;
	[[nodiscard]] const std::vector<fire::FireEffect*>& List() const override;
	void FreeDeleted() override;

	void Clear() override;

private:
	/// Every fire, owned; the list order (newest first) is _list
	std::vector<std::unique_ptr<fire::FireEffect>> _pool;
	std::vector<fire::FireEffect*> _list;
	std::unordered_map<entt::entity, fire::FireEffect*> _byObject;
	std::unordered_map<uint32_t, fire::FireEffect*> _byId;
	uint32_t _nextId {1};
	/// The tag ProcessList processes (0 every turn) and the tag of the next fire (0 after each)
	uint8_t _processTag {0};
	uint8_t _createTag {0};
};
} // namespace openblack::ecs::systems
