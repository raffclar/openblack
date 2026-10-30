/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ParticleSystem.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>

#include "Common/GameRandom.h"
#include "Common/StringUtils.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/LightSheet.h"

using namespace openblack;
using namespace openblack::ecs::systems;

ParticleSystem::ParticleSystem(FindRules findRules, FindTextureStems findTextureStems)
    : _findRules(std::move(findRules))
    , _findTextureStems(std::move(findTextureStems))
{
}

std::map<std::string, std::string, std::less<>> ParticleSystem::TextureStemsOnDisk()
{
	std::map<std::string, std::string, std::less<>> stems;
	if (!Locator::filesystem::has_value())
	{
		return stems;
	}
	auto& fileSystem = Locator::filesystem::value();
	fileSystem.Iterate(fileSystem.GetPath<filesystem::Path::Textures>(), false, [&stems](const std::filesystem::path& file) {
		if (string_utils::LowerCase(file.extension().string()) == ".raw")
		{
			const auto stem = file.stem().string();
			stems.insert_or_assign(string_utils::LowerCase(stem), stem);
		}
	});
	return stems;
}

void ParticleSystem::LoadTextures()
{
	if (!_stemsFound && _findTextureStems)
	{
		_textureStems = _findTextureStems();
		_stemsFound = true;
	}
}

void ParticleSystem::ResolveTextures(const ParticleRuleSystemInterface& rules) const
{
	const auto names = rules.SheetNames();
	// New rules start a new list
	if (names.size() < _sheetsResolved)
	{
		_sheetsResolved = 0;
	}
	if (names.size() == _sheetsResolved)
	{
		return;
	}
	if (!_stemsFound && _findTextureStems)
	{
		_textureStems = _findTextureStems();
		_stemsFound = true;
	}
	for (const auto& texture : names.subspan(_sheetsResolved))
	{
		if (texture.empty() || _textures.contains(texture))
		{
			continue;
		}
		// The files do not always spell the name of a sheet as it is on the disk
		const auto found = _textureStems.find(string_utils::LowerCase(texture));
		const auto& stem = found != _textureStems.end() ? found->second : texture;
		_textures.insert_or_assign(texture, std::pair {entt::hashed_string(fmt::format("raw/{}", stem).c_str()).value(),
		                                               entt::hashed_string(fmt::format("raw/{}a", stem).c_str()).value()});
	}
	_sheetsResolved = names.size();
}

particles::draw::Sources ParticleSystem::DrawSources(const ParticleRuleSystemInterface& rules) const
{
	const auto* world = rules.World();
	return {
	    .textures = [this](std::string_view texture) -> std::optional<std::pair<entt::id_type, entt::id_type>> {
		    const auto found = _textures.find(texture);
		    if (found == _textures.end())
		    {
			    return std::nullopt;
		    }
		    return found->second;
	    },
	    .playerColour = [world](int player) { return world != nullptr ? world->PlayerColour(player) : 0xFFFFFFu; },
	    .random =
	        [](float range) {
		        return Locator::gameRandom::has_value() ? Locator::gameRandom::value().LocalFloatRand(range) : 0.0f;
	        },
	    .landHeight = [world](glm::vec2 xz) { return world != nullptr ? world->LandHeight(xz) : 0.0f; },
	    .objectPosition = [world](entt::entity object) -> std::optional<glm::vec3> {
		    return world != nullptr ? world->ObjectPosition(object) : std::nullopt;
	    },
	    .gameMilliseconds = world != nullptr ? world->GameMilliseconds() : 0,
	};
}

void ParticleSystem::AddDrawn(const DrawnEffect& drawn, float turnFraction, particles::draw::Frame& frame,
                              const particles::draw::Sources& sources) const
{
	_walk.Clear();
	drawn.effect->Walk(turnFraction, _walk);
	if (drawn.drawOffset != glm::vec3(0.0f))
	{
		for (auto& atom : _walk.atoms)
		{
			atom.position += drawn.drawOffset * atom.offsetWeight;
		}
		for (auto& joint : _walk.joints)
		{
			joint.position += drawn.drawOffset * joint.offsetWeight;
		}
	}
	// Move each atom that follows the hand of the cast of this computer by the move of the hand since the step
	if (drawn.liveHand.has_value())
	{
		const auto followHand = [&hand = *drawn.liveHand](particles::Effect::DrawAtom& atom) {
			if (atom.handOffset.has_value())
			{
				atom.position += particles::LiveHandShift(*atom.handOffset, hand);
			}
		};
		std::ranges::for_each(_walk.atoms, followHand);
		std::ranges::for_each(_walk.joints, followHand);
	}
	particles::draw::AddEffect(frame, _walk, drawn.path, drawn.effect->GetOrigin(), drawn.effect->GetPlayer(), sources);
}

void ParticleSystem::CollectEffectDrawFrame(EffectId id, float turnFraction, particles::draw::Frame& frame) const
{
	frame.Clear();
	const auto* rules = _findRules ? _findRules() : nullptr;
	if (rules == nullptr)
	{
		return;
	}
	ResolveTextures(*rules);
	if (const auto drawn = rules->FindDrawn(id); drawn.has_value() && drawn->effect != nullptr)
	{
		AddDrawn(*drawn, turnFraction, frame, DrawSources(*rules));
	}
}

void ParticleSystem::AddLightSheets(const ParticleRuleSystemInterface& rules, particles::draw::Frame& frame)
{
	std::vector<particles::LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	for (const auto& sheet : rules.LightSheets())
	{
		if (sheet->Hidden())
		{
			continue;
		}
		// It takes its place by the point where its middle was at its last draw
		const auto sortPoint = sheet->Middle();
		sheet->Build(vertices, triangles);
		particles::draw::AddLightSheet(frame, vertices, triangles, sortPoint, sheet->GetLook());
	}
}

void ParticleSystem::CollectDrawFrame(float turnFraction, particles::draw::Frame& frame, bool inTemple) const
{
	frame.Clear();
	const auto* rules = _findRules ? _findRules() : nullptr;
	if (rules != nullptr)
	{
		ResolveTextures(*rules);
		const auto sources = DrawSources(*rules);
		rules->ForEachDrawn([&](const DrawnEffect& drawn) {
			if (drawn.effect == nullptr || drawn.inTemple != inTemple || drawn.drawnAlone || !drawn.shown)
			{
				return;
			}
			AddDrawn(drawn, turnFraction, frame, sources);
		});
		// The light sheets of the land and the symbols of the players over their artefacts are of the world
		if (!inTemple)
		{
			AddLightSheets(*rules, frame);
			for (const auto& symbol : rules->PlayerSymbols())
			{
				particles::draw::AddPlayerSymbol(frame, symbol, sources);
			}
			particles::draw::AddFightMarks(frame, rules->FightMarks());
		}
	}
	if (!inTemple)
	{
		particles::draw::AddLearningMarks(frame, _learningMarks);
	}
	_drawStats = {
	    .sprites = frame.sprites.size(),
	    .chains = frame.chains.size(),
	    .meshes = frame.meshes.size(),
	    .mists = frame.mists.size(),
	    .lightStamps = frame.lightStamps.size(),
	    .effects = frame.groups.size(),
	};
}
