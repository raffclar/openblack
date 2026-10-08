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
#include <vector>

#include <entt/entity/fwd.hpp>

#include "ECS/Fire/FireEffect.h"

namespace openblack::ecs::systems
{
/// The game's fires: every FireEffect, owned until the end of the turn it is deleted in, the list (newest first),
/// the look-ups by object and by handle, the next handle and the two process tags (ecs::fire goes through it)
class FireEffectSystemInterface
{
public:
	virtual ~FireEffectSystemInterface() = default;

	/// The object's fire, or null
	[[nodiscard]] virtual fire::FireEffect* FindByObject(entt::entity object) = 0;
	/// A fire by its handle while it is listed, or null
	[[nodiscard]] virtual fire::FireEffect* FindById(uint32_t id) = 0;

	/// Takes the next handle (never reused within a game)
	[[nodiscard]] virtual uint32_t TakeId() = 0;
	/// The tag of the next fire, which then goes back to 0
	[[nodiscard]] virtual uint8_t TakeCreateTag() = 0;
	/// The tag ProcessList processes
	[[nodiscard]] virtual uint8_t ProcessTag() const = 0;
	virtual void SetProcessTag(uint8_t tag) = 0;

	/// The fire is found by its object and its handle, put at the head of the list and kept until freed
	virtual fire::FireEffect& Insert(std::unique_ptr<fire::FireEffect> fire) = 0;
	/// The fire leaves the list (it stays owned)
	virtual void Unlist(const fire::FireEffect& fire) = 0;
	/// The object no longer finds a fire
	virtual void ForgetObject(entt::entity object) = 0;
	/// The handle no longer finds a fire
	virtual void ForgetId(uint32_t id) = 0;
	/// The fire is found by `to` instead of `from`
	virtual void MoveObject(entt::entity from, entt::entity to, fire::FireEffect& fire) = 0;
	/// Every listed fire, newest first
	[[nodiscard]] virtual const std::vector<fire::FireEffect*>& List() const = 0;
	/// The fires marked deleted are freed
	virtual void FreeDeleted() = 0;

	/// A land is loaded: no fires and both tags 0; the handles go on
	virtual void Clear() = 0;
};
} // namespace openblack::ecs::systems
