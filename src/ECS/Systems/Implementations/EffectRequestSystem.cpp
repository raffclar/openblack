/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "EffectRequestSystem.h"

#include <utility>

#include "ECS/Reflection/BinaryArchive.h"

using namespace openblack::ecs::systems;

EffectRequestSystem::EffectRequestSystem(FindServer findServer)
    : _findServer(std::move(findServer))
{
}

EffectHandle EffectRequestSystem::Start(const EffectRequest& request)
{
	// The number goes to the effect on every computer, also on one that shows nothing
	const auto handle = _next++;
	if (auto* server = _findServer())
	{
		server->Serve(request, handle);
	}
	return handle;
}

void EffectRequestSystem::Show(const EffectRequest& request)
{
	if (auto* server = _findServer())
	{
		server->Serve(request, k_NoEffectHandle);
	}
}

void EffectRequestSystem::Move(EffectHandle handle, glm::vec3 position)
{
	if (auto* server = _findServer(); server != nullptr && handle != k_NoEffectHandle)
	{
		server->Move(handle, position);
	}
}

void EffectRequestSystem::Stop(EffectHandle handle)
{
	if (auto* server = _findServer(); server != nullptr && handle != k_NoEffectHandle)
	{
		server->Stop(handle);
	}
}

void EffectRequestSystem::Remove(EffectHandle handle)
{
	if (auto* server = _findServer(); server != nullptr && handle != k_NoEffectHandle)
	{
		server->Remove(handle);
	}
}

void EffectRequestSystem::Reset()
{
	_next = k_FirstEffectHandle;
}

void EffectRequestSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _next);
}

bool EffectRequestSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _next);
	return !in.Failed();
}
