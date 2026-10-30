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
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleEffect.h"

using namespace openblack::ecs::systems;

EffectRequestSystem::EffectRequestSystem(FindServer findServer)
    : _findServer(std::move(findServer))
{
	if (auto* server = _findServer())
	{
		server->SetLifeListener(this);
	}
}

EffectRequestSystem::~EffectRequestSystem()
{
	if (auto* server = _findServer())
	{
		server->SetLifeListener(nullptr);
	}
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

EffectHandle EffectRequestSystem::StartHere(const EffectRequest& request)
{
	auto* server = _findServer();
	return server != nullptr ? server->Serve(request, k_NoEffectHandle) : k_NoEffectHandle;
}

EffectServerInterface* EffectRequestSystem::ServerFor(EffectHandle handle) const
{
	return handle != k_NoEffectHandle ? _findServer() : nullptr;
}

void EffectRequestSystem::Move(EffectHandle handle, glm::vec3 position)
{
	if (auto* server = ServerFor(handle))
	{
		server->Move(handle, position);
	}
}

void EffectRequestSystem::Stop(EffectHandle handle)
{
	if (auto* server = ServerFor(handle))
	{
		server->Stop(handle);
	}
}

void EffectRequestSystem::Remove(EffectHandle handle)
{
	if (auto* server = ServerFor(handle))
	{
		server->Remove(handle);
	}
}

bool EffectRequestSystem::IsRunning(EffectHandle handle) const
{
	// The record answers. The turns never ask the particles.
	return handle != k_NoEffectHandle && _running.contains(handle);
}

void EffectRequestSystem::SetPlayer(EffectHandle handle, int player)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetPlayer(handle, player);
	}
}

void EffectRequestSystem::SetPower(EffectHandle handle, float power)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetPower(handle, power);
	}
}

void EffectRequestSystem::SetMagnitude(EffectHandle handle, float magnitude)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetMagnitude(handle, magnitude);
	}
}

void EffectRequestSystem::SetAlpha(EffectHandle handle, float alpha)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetAlpha(handle, alpha);
	}
}

void EffectRequestSystem::SetProcessInfo(EffectHandle handle, const particles::ProcessInfo& info)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetProcessInfo(handle, info);
	}
}

void EffectRequestSystem::SetEnabled(EffectHandle handle, bool enabled)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetEnabled(handle, enabled);
	}
}

void EffectRequestSystem::AddTarget(EffectHandle handle, entt::entity target)
{
	if (auto* server = ServerFor(handle))
	{
		server->AddTarget(handle, target);
	}
}

void EffectRequestSystem::AddTargetPosition(EffectHandle handle, glm::vec3 position)
{
	if (auto* server = ServerFor(handle))
	{
		server->AddTargetPosition(handle, position);
	}
}

void EffectRequestSystem::AddPlasma(EffectHandle handle, const particles::PlasmaCommand& command)
{
	if (auto* server = ServerFor(handle))
	{
		server->AddPlasma(handle, command);
	}
}

void EffectRequestSystem::AddManaPathSpark(EffectHandle handle, const particles::mana_path::Spark& spark)
{
	if (auto* server = ServerFor(handle))
	{
		server->AddManaPathSpark(handle, spark);
	}
}

bool EffectRequestSystem::ShowsEffects() const
{
	return _findServer() != nullptr;
}

bool EffectRequestSystem::Step(EffectHandle handle, const particles::ProcessInfo& info, float seconds)
{
	auto* server = ServerFor(handle);
	return server != nullptr && server->ProcessForSpell(handle, info, seconds);
}

void EffectRequestSystem::RestoreSink(EffectHandle handle, particles::SpellSink* sink)
{
	if (auto* server = ServerFor(handle))
	{
		server->RestoreSink(handle, sink);
	}
}

void EffectRequestSystem::SetHidden(EffectHandle handle, bool hidden)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetHidden(handle, hidden);
	}
}

void EffectRequestSystem::SetDrawOffset(EffectHandle handle, glm::vec3 offset)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetDrawOffset(handle, offset);
	}
}

void EffectRequestSystem::SetLiveHand(EffectHandle handle, glm::vec3 hand)
{
	if (auto* server = ServerFor(handle))
	{
		server->SetLiveHand(handle, hand);
	}
}

std::shared_ptr<openblack::particles::ShieldSphere> EffectRequestSystem::FindShield(glm::vec3 point, float margin) const
{
	const auto* server = _findServer();
	return server != nullptr ? server->FindShield(point, margin) : nullptr;
}

void EffectRequestSystem::AddBeliefSprite(const particles::BeliefSprite& sprite)
{
	if (auto* server = _findServer())
	{
		server->AddBeliefSprite(sprite);
	}
}

void EffectRequestSystem::AddHandManaPathSpark(const particles::mana_path::Spark& spark)
{
	if (auto* server = _findServer())
	{
		server->AddHandManaPathSpark(spark);
	}
}

void EffectRequestSystem::AddStormBolt(glm::vec3 cloudPoint)
{
	if (auto* server = _findServer())
	{
		server->AddStormBolt(cloudPoint);
	}
}

void EffectRequestSystem::SetFightMarks(std::vector<particles::draw::FightMark> marks)
{
	if (auto* server = _findServer())
	{
		server->SetFightMarks(std::move(marks));
	}
}

void EffectRequestSystem::AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet)
{
	if (auto* server = _findServer())
	{
		server->AddLightSheet(sheet);
	}
}

void EffectRequestSystem::SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol> symbols)
{
	if (auto* server = _findServer())
	{
		server->SetPlayerSymbols(std::move(symbols));
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
