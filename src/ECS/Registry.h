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

#include <source_location>
#include <string_view>
#include <tuple>

#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/helper.hpp>
#include <entt/entity/registry.hpp>

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
		Dirty("Destroy", {}, 0, true);
		_registry.destroy(first, last);
	}
	template <typename Component, typename... Args>
	decltype(auto) Assign(entt::entity entity, [[maybe_unused]] Args&&... args)
	{
		ComponentsChanged<Component>("Assign", true);
		return _registry.emplace<Component>(entity, std::forward<Args>(args)...);
	}
	template <typename Component, typename... Args>
	decltype(auto) AssignOrReplace(entt::entity entity, [[maybe_unused]] Args&&... args)
	{
		// a component replaced keeps the entity's place in the draw lists: only one gained changes them
		ComponentsChanged<Component>("AssignOrReplace", !_registry.all_of<Component>(entity));
		return _registry.emplace_or_replace<Component>(entity, std::forward<Args>(args)...);
	}
	template <typename Component, typename... Other>
	decltype(auto) Remove(entt::entity entity)
	{
		// only a component the entity has, removed, changes the draw lists
		ComponentsChanged<Component, Other...>("Remove", false, entity);
		return _registry.remove<Component, Other...>(entity);
	}
	/// For simulation-only components the renderer never reads: like Assign / AssignOrReplace / Remove, without
	/// rebuilding the drawn instances
	template <typename Component, typename... Args>
	decltype(auto) AssignState(entt::entity entity, Args&&... args)
	{
		return _registry.emplace<Component>(entity, std::forward<Args>(args)...);
	}
	template <typename Component, typename... Args>
	decltype(auto) AssignOrReplaceState(entt::entity entity, Args&&... args)
	{
		return _registry.emplace_or_replace<Component>(entity, std::forward<Args>(args)...);
	}
	template <typename Component>
	decltype(auto) RemoveState(entt::entity entity)
	{
		return _registry.remove<Component>(entity);
	}
	template <typename After, typename Before, typename... Args>
	decltype(auto) SwapComponents(entt::entity entity, [[maybe_unused]] Before previousComponent,
	                              [[maybe_unused]] Args&&... args)
	{
		Remove<Before>(entity);
		return Assign<After>(entity, std::forward<Args>(args)...);
	}
	/// The drawn instances are written again at the next PrepareDraw, into the ranges they have
	/// (RenderingSystemInterface::SetDirty): for a change of what the drawn entities look like or where they are, not of
	/// which entities are drawn. `where`: the caller, counted by the profile (OPENBLACK_PROFILE, Profiler::CountDirty).
	/// (Not virtual: a default argument)
	void SetDirty(std::source_location where = std::source_location::current());
	virtual RegistryContext& Context();
	[[nodiscard]] virtual const RegistryContext& Context() const;
	virtual void Reset();
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
	/// (openblack) every storage of the registry with its type id: the entities' own first (entt keeps it apart from
	/// the components'), then the components' in the registry's order (Debug/StateHash.h)
	template <typename Func>
	void EachStorage(Func func) const
	{
		func(entt::type_hash<entt::entity>::value(), *_registry.storage<entt::entity>());
		for (auto [id, storage] : _registry.storage())
		{
			func(id, storage);
		}
	}
	/// entt's on_destroy sink of a component (Remove, Destroy and Reset publish it)
	template <typename Component>
	[[nodiscard]] decltype(auto) OnDestroy()
	{
		return _registry.on_destroy<Component>();
	}
	virtual ~Registry() = default;

protected:
	entt::registry _registry;

private:
	/// Assign / AssignOrReplace / Remove: a component of the draw layout (k_ChangesDrawLayout) gained (`gained`) or,
	/// for a removal from `entity`, one it has, has the draw lists made again; anything else only the instances
	/// written again
	template <typename... Components>
	void ComponentsChanged(std::string_view operation, bool gained, entt::entity entity = entt::null)
	{
		using First = std::tuple_element_t<0, std::tuple<Components...>>;
		const bool layout =
		    gained ? k_AnyChangesDrawLayout<Components...>
		           : entity != entt::null && ((k_ChangesDrawLayout<Components> && _registry.all_of<Components>(entity)) || ...);
		Dirty(operation, entt::type_name<First>::value(), 0, layout);
	}
	/// SetDirty, by `where` / `what` / `line` (Profiler::CountDirty); `layout`: the draw lists are made again
	/// (RenderingSystemInterface::SetLayoutDirty)
	void Dirty(std::string_view where, std::string_view what, uint32_t line, bool layout = false);
};

} // namespace openblack::ecs
