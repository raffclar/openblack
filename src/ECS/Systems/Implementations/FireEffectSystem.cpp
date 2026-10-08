/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FireEffectSystem.h"

#include <algorithm>

using namespace openblack::ecs::systems;
using openblack::ecs::fire::FireEffect;

FireEffect* FireEffectSystem::FindByObject(entt::entity object)
{
	const auto it = _byObject.find(object);
	return it != _byObject.end() ? it->second : nullptr;
}

FireEffect* FireEffectSystem::FindById(uint32_t id)
{
	const auto it = _byId.find(id);
	return it != _byId.end() ? it->second : nullptr;
}

uint32_t FireEffectSystem::TakeId()
{
	return _nextId++;
}

uint8_t FireEffectSystem::TakeCreateTag()
{
	const auto tag = _createTag;
	_createTag = 0;
	return tag;
}

uint8_t FireEffectSystem::ProcessTag() const
{
	return _processTag;
}

void FireEffectSystem::SetProcessTag(uint8_t tag)
{
	_processTag = tag;
}

FireEffect& FireEffectSystem::Insert(std::unique_ptr<FireEffect> fire)
{
	auto* raw = fire.get();
	_byObject[raw->object] = raw;
	_byId[raw->id] = raw;
	_list.insert(_list.begin(), raw);
	_pool.push_back(std::move(fire));
	return *raw;
}

void FireEffectSystem::Unlist(const FireEffect& fire)
{
	std::erase(_list, &fire);
}

void FireEffectSystem::ForgetObject(entt::entity object)
{
	_byObject.erase(object);
}

void FireEffectSystem::ForgetId(uint32_t id)
{
	_byId.erase(id);
}

void FireEffectSystem::MoveObject(entt::entity from, entt::entity to, FireEffect& fire)
{
	_byObject.erase(from);
	_byObject[to] = &fire;
}

const std::vector<FireEffect*>& FireEffectSystem::List() const
{
	return _list;
}

void FireEffectSystem::FreeDeleted()
{
	std::erase_if(_pool, [](const std::unique_ptr<FireEffect>& fire) { return (fire->flags & FireEffect::k_Deleted) != 0; });
}

void FireEffectSystem::Clear()
{
	_list.clear();
	_byObject.clear();
	_byId.clear();
	_pool.clear();
	_processTag = 0;
	_createTag = 0;
}
