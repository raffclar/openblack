/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <entt/fwd.hpp>
#include <entt/resource/cache.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "LoadQueue.h"

namespace openblack::resources
{
template <typename T>
[[nodiscard]] constexpr entt::id_type HashIdentifier(const T& identifier)
{
	if constexpr (std::is_same_v<T, std::string>)
	{
		return entt::hashed_string(identifier.c_str());
	}
	else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>)
	{
		return entt::hashed_string(identifier);
	}
	else
	{
		return entt::hashed_string(fmt::format("{}", static_cast<uint32_t>(identifier)).c_str());
	}
}

/// How a resource registered ahead of its use is loaded. It may run on a loading thread, so it may only read files and
/// the data it owns or shares unchanged, and make graphics resources; it must never touch the resource caches.
template <typename Resource>
using DeferredLoad = std::function<std::shared_ptr<Resource>()>;

namespace detail
{
/// One load of a registered resource, run once by whichever thread gets to it first
template <typename Resource>
class LoadJob
{
public:
	explicit LoadJob(DeferredLoad<Resource> load)
	    : _load(std::move(load))
	    , _result(_promise.get_future().share())
	{
	}

	/// Runs the load unless another thread already has; true when this call ran it
	bool Run() noexcept
	{
		if (_claimed.test_and_set())
		{
			return false;
		}
		try
		{
			_promise.set_value(_load());
		}
		catch (...)
		{
			_promise.set_exception(std::current_exception());
		}
		// What the load held on to, such as the pack it reads from, goes with it
		_load = nullptr;
		return true;
	}

	[[nodiscard]] bool Done() const { return _result.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }

	/// The resource, waiting for the load to end; the load's error is thrown again
	[[nodiscard]] std::shared_ptr<Resource> Get() const { return _result.get(); }

private:
	DeferredLoad<Resource> _load;
	std::atomic_flag _claimed;
	std::promise<std::shared_ptr<Resource>> _promise;
	std::shared_future<std::shared_ptr<Resource>> _result;
};
} // namespace detail

/// A cache of resources by id. A resource is either loaded at once with Load, or registered with Register and loaded
/// when it is first asked for, or ahead of that on a loading thread once prefetched. Everything asking for a resource
/// gets the same answer either way: Contains is true from registration, and Handle or Find wait for the load, or run it
/// there and then, so what the game does never depends on how far loading has got. Only the main thread may use it.
template <typename ResourceLoader>
class ResourceManager
{
public:
	using ResourceType = typename ResourceLoader::ResourceType;
	using Pointer = std::shared_ptr<ResourceType>;

	template <typename... Args>
	[[maybe_unused]] decltype(auto) Load(entt::id_type identifier, Args&&... args)
	{
		// One registered already is loaded as it was registered, once
		Resolve(identifier);
		return _resourceCache.load(identifier, std::forward<Args>(args)...);
	}

	template <typename... Args>
	[[maybe_unused]] decltype(auto) Erase(entt::id_type identifier, Args&&... args)
	{
		_deferred.erase(identifier);
		return _resourceCache.erase(identifier, std::forward<Args>(args)...);
	}

	template <typename T, typename... Args>
	[[maybe_unused]] decltype(auto) Load(T identifier, Args&&... args)
	{
		return Load(HashIdentifier(identifier), std::forward<Args>(args)...);
	}

	template <typename T, typename... Args>
	[[maybe_unused]] decltype(auto) Erase(T identifier, Args&&... args)
	{
		return Erase(HashIdentifier(identifier), std::forward<Args>(args)...);
	}

	/// Registers a resource to be loaded when first asked for, or ahead of that once prefetched. `bytes` is about how
	/// much it reads, for the loading budget. False when the id is already loaded or registered, which keeps the first.
	bool Register(entt::id_type identifier, DeferredLoad<ResourceType> load, size_t bytes = 0)
	{
		if (Contains(identifier))
		{
			return false;
		}
		_deferred.emplace(identifier, Deferred {.load = std::move(load), .bytes = bytes});
		return true;
	}

	template <typename T>
	bool Register(T identifier, DeferredLoad<ResourceType> load, size_t bytes = 0)
	{
		return Register(HashIdentifier(identifier), std::move(load), bytes);
	}

	/// Registers a resource to be loaded later by the cache's loader with these arguments, which are kept until then
	template <typename... Args>
	bool RegisterLoad(entt::id_type identifier, size_t bytes, Args... args)
	{
		return Register(identifier, [... args = std::move(args)] { return ResourceLoader {}(args...); }, bytes);
	}

	template <typename T, typename... Args>
	bool RegisterLoad(T identifier, size_t bytes, Args... args)
	{
		return RegisterLoad(HashIdentifier(identifier), bytes, std::move(args)...);
	}

	/// Asks for a registered resource to be loaded on a loading thread, after those asked for before it
	void Prefetch(entt::id_type identifier)
	{
		if (const auto it = _deferred.find(identifier); it != _deferred.end() && !it->second.wanted)
		{
			it->second.wanted = true;
			_wanted.push_back(identifier);
		}
	}

	/// Asks for every registered resource to be loaded on the loading threads
	void PrefetchAll()
	{
		for (auto& [identifier, deferred] : _deferred)
		{
			if (!deferred.wanted)
			{
				deferred.wanted = true;
				_wanted.push_back(identifier);
			}
		}
	}

	/// Hands the prefetched resources to the loading threads in the order they were asked for, until about `budget`
	/// bytes have gone (at least one goes). Returns the bytes handed over.
	size_t StartLoads(LoadQueue& queue, size_t budget)
	{
		_queue = &queue;
		size_t started = 0;
		size_t next = 0;
		for (; next < _wanted.size() && (started == 0 || started < budget); ++next)
		{
			const auto it = _deferred.find(_wanted[next]);
			if (it == _deferred.end() || it->second.job)
			{
				continue;
			}
			auto job = std::make_shared<detail::LoadJob<ResourceType>>(std::move(it->second.load));
			it->second.job = job;
			const auto bytes = std::max<size_t>(it->second.bytes, 1);
			_running.push_back({.identifier = it->first, .bytes = bytes});
			_runningBytes += bytes;
			queue.Submit([job] { job->Run(); });
			started += bytes;
		}
		_wanted.erase(_wanted.begin(), _wanted.begin() + static_cast<std::ptrdiff_t>(next));
		return started;
	}

	/// Moves the resources the loading threads have finished into the cache; returns how many
	size_t Collect()
	{
		size_t collected = 0;
		std::erase_if(_running, [this, &collected](const Running& running) {
			const auto it = _deferred.find(running.identifier);
			// Erased, or erased and registered again, since it was handed over, or loaded where it was asked for
			if (it != _deferred.end() && it->second.job)
			{
				if (!it->second.job->Done())
				{
					return false;
				}
				Resolve(running.identifier);
				++collected;
			}
			_runningBytes -= running.bytes;
			return true;
		});
		return collected;
	}

	/// Whether the resource is in the cache now, rather than only registered
	[[nodiscard]] bool IsLoaded(entt::id_type identifier) const { return _resourceCache.contains(identifier); }

	/// How many registered resources have not been loaded yet
	[[nodiscard]] size_t PendingCount() const { return _deferred.size(); }

	/// How many prefetched resources are waiting for a loading thread or on one
	[[nodiscard]] size_t LoadingCount() const { return _wanted.size() + _running.size(); }

	/// About how many bytes the resources handed to the loading threads and not collected yet read
	[[nodiscard]] size_t LoadingBytes() const { return _runningBytes; }

	[[nodiscard]] decltype(auto) Handle(entt::id_type identifier)
	{
		Resolve(identifier);
		return _resourceCache[identifier];
	}

	[[nodiscard]] decltype(auto) Handle(entt::id_type identifier) const
	{
		Resolve(identifier);
		return std::as_const(_resourceCache)[identifier];
	}

	[[nodiscard]] bool Contains(entt::id_type identifier) const
	{
		return _resourceCache.contains(identifier) || _deferred.contains(identifier);
	}

	/// The resource, or null when there is none of the id: one lookup, for what runs for every draw
	[[nodiscard]] const ResourceType* Find(entt::id_type identifier) const
	{
		Resolve(identifier);
		const auto resource = std::as_const(_resourceCache)[identifier];
		return resource ? &*resource : nullptr;
	}

	template <typename T>
	[[nodiscard]] bool Contains(T identifier) const
	{
		return Contains(HashIdentifier(identifier));
	}

	/// Each resource loaded so far; those registered and not loaded yet are left out
	template <typename Func>
	void Each(Func func) const
	{
		for (const auto [i, r] : std::as_const(_resourceCache))
		{
			func(i, r);
		}
	}

	/// How many resources there are, loaded or registered
	[[nodiscard]] size_t Size() const { return _resourceCache.size() + _deferred.size(); }

	void Clear()
	{
		_wanted.clear();
		_running.clear();
		_runningBytes = 0;
		_deferred.clear();
		_resourceCache.clear();
	}

private:
	/// Puts a resource already made into the cache
	struct ReadyTag
	{
	};

	/// What the cache loads with: the resource's own loader, or a resource already made
	struct CacheLoader
	{
		using result_type = Pointer;

		template <typename... Args>
		result_type operator()(Args&&... args) const
		{
			return ResourceLoader {}(std::forward<Args>(args)...);
		}

		result_type operator()(ReadyTag /*unused*/, Pointer resource) const { return resource; }
	};

	struct Running
	{
		entt::id_type identifier;
		size_t bytes;
	};

	struct Deferred
	{
		DeferredLoad<ResourceType> load;
		size_t bytes {0};
		/// Set once it is handed to a loading thread
		std::shared_ptr<detail::LoadJob<ResourceType>> job;
		bool wanted {false};
	};

	/// Loads a registered resource that isn't loaded yet: waits for its loading thread, or loads it here when no thread
	/// has started it. One that fails to load is logged and left out.
	void Resolve(entt::id_type identifier) const
	{
		if (_deferred.empty() || _resourceCache.contains(identifier))
		{
			return;
		}
		const auto it = _deferred.find(identifier);
		if (it == _deferred.end())
		{
			return;
		}
		auto job = it->second.job ? std::move(it->second.job)
		                          : std::make_shared<detail::LoadJob<ResourceType>>(std::move(it->second.load));
		_deferred.erase(it);
		// A load running on a loading thread may be waiting to upload in a later frame: it can't, while this waits
		std::optional<graphics::UploadPacer::Hurry> hurry;
		if (_queue != nullptr && !job->Done())
		{
			hurry.emplace(_queue->GetUploadPacer());
		}
		job->Run();
		try
		{
			if (auto resource = job->Get())
			{
				_resourceCache.load(identifier, ReadyTag {}, std::move(resource));
			}
		}
		catch (const std::exception& error)
		{
			if (const auto logger = spdlog::get("game"))
			{
				logger->error("Unable to load resource {}: {}", identifier, error.what());
			}
		}
	}

	// Loading on first use fills the cache from what only reads it
	mutable entt::resource_cache<ResourceType, CacheLoader> _resourceCache;
	mutable std::unordered_map<entt::id_type, Deferred> _deferred;
	/// Prefetched in the order asked for, not handed to a loading thread yet
	std::vector<entt::id_type> _wanted;
	/// Handed to a loading thread, not collected yet
	std::vector<Running> _running;
	size_t _runningBytes {0};
	/// The threads the prefetched resources were handed to
	LoadQueue* _queue {nullptr};
};
} // namespace openblack::resources
