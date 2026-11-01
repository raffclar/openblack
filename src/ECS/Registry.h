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

#include <optional>
#include <tuple>

#include <entt/entity/entity.hpp>
#include <entt/entity/helper.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>

#include "ECS/DrawLayoutComponents.h"
#include "ECS/RegistryContext.h"

namespace openblack
{
class Camera;
}

namespace openblack::graphics
{
class DebugLines;
class ShaderManager;
} // namespace openblack::graphics

namespace openblack::ecs
{
class Registry
{
public:
	Registry();
	decltype(auto) Create() { return _registry.create(); }
	template <typename It>
	void Create(It first, It last)
	{
		_registry.create(first, last);
	}
	virtual void Release(entt::entity entity);
	template <typename It>
	void Release(It first, It last)
	{
		ENTT_ASSERT(std::all_of(first, last, [](const auto entt) { return orphan(entt); }), "Non-orphan entity");
		_registry.storage<entt::entity>().erase(std::move(first), std::move(last));
	}
	virtual void Destroy(entt::entity entity);
	template <typename It>
	void Destroy(It first, It last)
	{
		SetLayoutDirty();
		_registry.destroy(first, last);
	}
	template <typename Component, typename... Args>
	decltype(auto) Assign(entt::entity entity, [[maybe_unused]] Args&&... args)
	{
		ComponentsChanged<Component>();
		return _registry.emplace<Component>(entity, std::forward<Args>(args)...);
	}
	template <typename Component, typename... Args>
	decltype(auto) AssignOrReplace(entt::entity entity, [[maybe_unused]] Args&&... args)
	{
		ComponentsChanged<Component>();
		return _registry.emplace_or_replace<Component>(entity, std::forward<Args>(args)...);
	}
	template <typename Component, typename... Other>
	decltype(auto) Remove(entt::entity entity)
	{
		ComponentsChanged<Component, Other...>();
		return _registry.remove<Component, Other...>(entity);
	}
	template <typename After, typename Before, typename... Args>
	decltype(auto) SwapComponents(entt::entity entity, [[maybe_unused]] Before previousComponent,
	                              [[maybe_unused]] Args&&... args)
	{
		Remove<Before>(entity);
		return Assign<After>(entity, std::forward<Args>(args)...);
	}
	/// What is drawn has moved: the instances are uploaded again
	virtual void SetDirty();
	/// Entities came or went, or gained or lost what they are drawn as: the draw lists are made again as well
	virtual void SetLayoutDirty();
	virtual RegistryContext& Context();
	[[nodiscard]] virtual const RegistryContext& Context() const;
	virtual void Reset();
	/// The next Reset gives the entities after it the ids of a new registry, and forgets the ids of the entities before
	/// it. Every computer of a network game loads its land this way, so that the entities have the same ids on each,
	/// whatever ran before the land.
	void SetFreshIdsAtNextReset() noexcept { _freshIdsAtNextReset = true; }
	template <typename Component>
	size_t Size()
	{
		return _registry.storage<Component>().size();
	}
	template <typename... Components>
	[[nodiscard]] bool AllOf(entt::entity entity) const
	{
		return _registry.all_of<Components...>(entity);
	}
	template <typename... Components>
	[[nodiscard]] bool AnyOf(entt::entity entity) const
	{
		return _registry.any_of<Components...>(entity);
	}
	template <typename... Components>
	decltype(auto) Get(entt::entity entity)
	{
		return _registry.get<Components...>(entity);
	}
	template <typename... Components>
	[[nodiscard]] decltype(auto) Get(entt::entity entity) const
	{
		return _registry.get<Components...>(entity);
	}
	template <typename... Components>
	decltype(auto) TryGet(entt::entity entity)
	{
		return _registry.try_get<Components...>(entity);
	}
	template <typename... Components>
	[[nodiscard]] decltype(auto) TryGet(entt::entity entity) const
	{
		return _registry.try_get<Components...>(entity);
	}
	template <typename... Components>
	[[nodiscard]] decltype(auto) Front() const
	{
		return _registry.view<Components...>().front();
	}
	template <typename... Components, typename... Exclude, typename Func>
	decltype(auto) Each(Func func, Exclude... exclude)
	{
		return _registry.view<Components...>(exclude...).each(func);
	}
	template <typename... Components, typename... Exclude, typename Func>
	[[nodiscard]] decltype(auto) Each(Func func, Exclude... exclude) const
	{
		return _registry.view<Components...>(exclude...).each(func);
	}
	/// The first entity in the order of Each that has the components and that the test accepts, or none. The search stops
	/// at that entity. The test takes the entity and its components, as the function of Each does.
	template <typename... Components, typename Test>
	[[nodiscard]] std::optional<entt::entity> FindFirst(Test test) const
	{
		for (const auto found : _registry.view<Components...>().each())
		{
			if (std::apply(test, found))
			{
				return std::get<0>(found);
			}
		}
		return std::nullopt;
	}
	template <typename Component>
	[[nodiscard]] decltype(auto) ToEntity(const Component& component) const
	{
		const auto* storage = _registry.template storage<Component>();
		return entt::to_entity(*storage, component);
	}
	template <typename Dst, typename Src>
	decltype(auto) As(Src& component)
	{
		return Get<Dst>(ToEntity(component));
	}
	template <typename Dst, typename Src>
	decltype(auto) As(const Src& component) const
	{
		return Get<Dst>(ToEntity(component));
	}
	template <typename... Components>
	[[nodiscard]] decltype(auto) Size() const
	{
		return _registry.view<Components...>().size();
	}
	[[nodiscard]] decltype(auto) Valid(entt::entity entity) const { return _registry.valid(entity); }
	/// The registry beneath, read without knowing its component types, as the debug inspector lists entities
	[[nodiscard]] const entt::registry& Underlying() const { return _registry; }
	/// The same written to, as the debug inspector adds, removes and sets components by their names; whoever writes
	/// through it marks what is drawn as changed
	[[nodiscard]] entt::registry& Underlying() { return _registry; }
	/// Told whenever an entity gains a component, or loses one (also as it is destroyed)
	template <typename Component>
	[[nodiscard]] decltype(auto) OnConstruct()
	{
		return _registry.on_construct<Component>();
	}
	template <typename Component>
	[[nodiscard]] decltype(auto) OnDestroy()
	{
		return _registry.on_destroy<Component>();
	}
	/// Told just before an entity goes through Destroy, while it and its components are all still there. Not told for
	/// the entities that go together, nor when the registry is emptied.
	[[nodiscard]] entt::sink<entt::sigh<void(entt::entity)>> OnDestroying() { return entt::sink {_destroying}; }
	/// Every entity made since the registry was last emptied, those destroyed since too: the statistics' count of the
	/// objects made on the land
	[[nodiscard]] uint32_t GetEntitiesMade() const noexcept { return _entitiesMade; }
	/// Goes up each time the registry is emptied, as before a land or a saved world loads. A load can give an entity the
	/// same id and version that it had before, so a cache that keeps data by entity also keeps this number.
	[[nodiscard]] uint32_t GetResets() const noexcept { return _resets; }
	/// How many entities went from the registry since it was made, however they went. The count only goes up: emptying
	/// the registry counts too.
	[[nodiscard]] uint64_t GetEntitiesGone() const noexcept { return _entitiesGone; }
	Registry(const Registry&) = delete;
	Registry& operator=(const Registry&) = delete;
	virtual ~Registry() = default;

protected:
	/// What is drawn changes with the components an entity gains or loses (see k_ChangesDrawLayout)
	template <typename... Components>
	void ComponentsChanged()
	{
		if constexpr ((k_ChangesDrawLayout<Components> || ...))
		{
			SetLayoutDirty();
		}
		else
		{
			SetDirty();
		}
	}

	entt::registry _registry;

private:
	void EntityMade(entt::registry& /*registry*/, entt::entity /*entity*/) { ++_entitiesMade; }
	void EntityGone(entt::registry& /*registry*/, entt::entity /*entity*/) { ++_entitiesGone; }
	/// A living that goes leaves its flock. A flock that it leaves with no members goes too.
	void LivingGone(entt::registry& registry, entt::entity living);

	uint32_t _entitiesMade {0};
	uint32_t _resets {0};
	uint64_t _entitiesGone {0};
	entt::sigh<void(entt::entity)> _destroying;
	bool _freshIdsAtNextReset {false};
};

/// The store of one kind of component, found one time for a walk of many things. Each look after that costs no search
/// of the registry. It gives the same answers as the registry while the store exists. Make it again when the walk can
/// make the first component of its kind.
template <typename Component>
class ComponentLookup
{
public:
	explicit ComponentLookup(const Registry& registry)
	    : _store(registry.Underlying().storage<Component>())
	{
	}
	[[nodiscard]] bool Has(entt::entity entity) const { return _store != nullptr && _store->contains(entity); }
	[[nodiscard]] const Component* TryGet(entt::entity entity) const
	{
		return Has(entity) ? &static_cast<const entt::storage_for_t<Component>*>(_store)->get(entity) : nullptr;
	}

private:
	// The store as its untyped base, so that a class can hold a lookup of a component that it only declares
	const entt::sparse_set* _store;
};

} // namespace openblack::ecs
