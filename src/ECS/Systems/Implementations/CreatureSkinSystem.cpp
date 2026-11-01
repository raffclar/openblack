/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureSkinSystem.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DSkinTexels.h"
#include "3D/TempleInteriorInterface.h"
#include "Commands/PlayerCommands.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Player.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;

namespace
{
/// The file of a creature mesh, for the texels of its skins. The game loads the files behind the loading screen. If a
/// file is not loaded, this function reads it the first time that a skin of it is painted.
const l3d::L3DFile* MeshFile(const std::string& name)
{
	if (name.empty())
	{
		return nullptr;
	}
	auto& files = Locator::resources::value().GetL3DFiles();
	const auto id = creature_skin::MeshFileId(name);
	if (!files.Contains(id))
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto path = fileSystem.GetPath<filesystem::Path::CreatureMesh>() / (name + ".l3d");
		try
		{
			files.Load(id, resources::L3DFileLoader::FromDiskTag {}, path);
		}
		catch (std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't read the skins of {}: {}", path.string(), err.what());
			return nullptr;
		}
	}
	return &*files.Handle(id);
}

/// The texels of a skin of a creature mesh as the game loads them: at half size at the two lowest detail levels
std::vector<uint16_t> TexelsOf(const l3d::L3DFile& file, const l3d::L3DTexture& skin)
{
	const bool lowResolution = Locator::config::has_value() && Locator::config::value().detail.lowResTextures;
	return l3d_skin::LoadedTexels({reinterpret_cast<const uint16_t*>(skin.texels.data()), skin.texels.size()},
	                              static_cast<uint32_t>(file.GetHeader().flags), lowResolution);
}

/// The skins as the body is drawn: the base mesh's, and the evil or good mesh's it is blended towards, the base's
/// where the species has none
struct SkinSources
{
	const l3d::L3DFile* base;
	const l3d::L3DFile* variant;
	entt::id_type baseId;
	entt::id_type variantId;
};

std::optional<SkinSources> SourcesOf(const CreatureRig& rig, float evilGood)
{
	const auto variantMesh = evilGood < 0.0f ? CreatureRig::Mesh::Evil : CreatureRig::Mesh::Good;
	const auto& baseName = rig.meshNames.at(static_cast<size_t>(CreatureRig::Mesh::Base));
	const auto& variantName = rig.meshNames.at(static_cast<size_t>(variantMesh));
	const auto* base = MeshFile(baseName);
	if (base == nullptr)
	{
		return std::nullopt;
	}
	const auto* variant = rig.hasMesh.at(static_cast<size_t>(variantMesh)) ? MeshFile(variantName) : nullptr;
	return SkinSources {
	    .base = base,
	    .variant = variant != nullptr ? variant : base,
	    .baseId = entt::hashed_string(baseName.c_str()).value(),
	    .variantId = entt::hashed_string((variant != nullptr ? variantName : baseName).c_str()).value(),
	};
}

void Paint(CreatureSkin& skin, const SkinSources& sources, uint8_t weight, const creature_tattoo::Slots& tattoos,
           const creature_tattoo::Masks* masks, const CreatureMarks& marks, const CreatureRig& rig,
           const creature_skin::Art* art)
{
	const auto& baseSkins = sources.base->GetSkins();
	const auto& variantSkins = sources.variant->GetSkins();
	std::vector<uint32_t> baseOrder;
	std::vector<uint32_t> variantOrder;
	std::ranges::transform(baseSkins, std::back_inserter(baseOrder), &l3d::L3DTexture::id);
	std::ranges::transform(variantSkins, std::back_inserter(variantOrder), &l3d::L3DTexture::id);

	skin.skins.resize(baseSkins.size());
	for (size_t i = 0; i < baseSkins.size(); ++i)
	{
		const auto& base = baseSkins[i];
		auto& painted = skin.skins[i];
		painted.id = base.id;
		const auto baseTexels = TexelsOf(*sources.base, base);
		painted.texels.resize(baseTexels.size());
		// A neutral creature, or one whose evil or good mesh lacks the skin, starts from its base skin
		std::vector<uint16_t> variant;
		if (sources.variant != sources.base)
		{
			if (const auto paired = creature_skin::PairedSkin(baseOrder, variantOrder, base.id); paired.has_value())
			{
				const auto found = std::ranges::find(variantSkins, *paired, &l3d::L3DTexture::id);
				if (found != variantSkins.end())
				{
					variant = TexelsOf(*sources.variant, *found);
				}
			}
		}
		creature_skin::Compose(painted.texels, baseTexels, variant, weight,
		                       {.skinIndex = static_cast<uint8_t>(i),
		                        .tattoos = tattoos,
		                        .sites = rig.tattooSites,
		                        .marks = marks.marks,
		                        .art = art,
		                        .masks = masks});
	}
	++skin.revision;
}
/// The entity of the player that a creature belongs to. A creature of nobody's uses the masks of the local player.
std::optional<entt::entity> PlayerEntityOf(const ecs::Registry& registry, PlayerNames owner)
{
	const auto local = Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	const auto wanted = owner == PlayerNames::NEUTRAL ? local : owner;
	std::optional<entt::entity> found;
	registry.Each<const Player>([&found, wanted](entt::entity entity, const Player& player) {
		if (player.name == wanted)
		{
			found = entity;
		}
	});
	return found;
}

const creature_tattoo::Masks* MasksOf(const ecs::Registry& registry, PlayerNames owner)
{
	const auto player = PlayerEntityOf(registry, owner);
	const auto* masks = player.has_value() ? registry.TryGet<const TattooMasks>(*player) : nullptr;
	return masks != nullptr ? &masks->masks : nullptr;
}

/// The tattoos that a creature's skin shows. The tattoos being edited come first. Then a creature of the local player,
/// or any creature while the player is in the temple, shows the tattoos of the local player's creature.
const creature_tattoo::Slots& ShownTattoos(const ecs::Registry& registry, PlayerNames owner, const CreatureTattoos& own,
                                           const CreatureTattoos& body)
{
	if (own.shown.has_value())
	{
		return *own.shown;
	}
	if (Locator::playerSystem::has_value())
	{
		const auto& players = Locator::playerSystem::value();
		const auto local =
		    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE);
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		if (owner == local || inTemple)
		{
			if (const auto primary = players.GetPrimaryCreature(local); primary.has_value())
			{
				if (const auto* tattoos = registry.TryGet<const CreatureTattoos>(*primary))
				{
					return tattoos->shown.has_value() ? *tattoos->shown : tattoos->slots;
				}
			}
		}
	}
	return body.shown.has_value() ? *body.shown : body.slots;
}

/// Paints the skin when what it shows has changed
void PaintIfChanged(CreatureSkin& skin, const Creature& creature, const CreatureMorph& morph,
                    const creature_tattoo::Slots& tattoos, uint32_t tattooRevision, const creature_tattoo::Masks* masks,
                    const CreatureMarks& marks, const creature_skin::Art* art)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(creature.species);
	if (!rigs.Contains(rigId))
	{
		return;
	}
	const auto& rig = *rigs.Handle(rigId);
	// The skins follow the alignment the body is drawn with, which moves on only past the morph threshold
	const auto sources = SourcesOf(rig, morph.drawn.evilGood);
	if (!sources)
	{
		return;
	}
	const CreatureSkin::Painted wanted {
	    .baseMesh = sources->baseId,
	    .variantMesh = sources->variantId,
	    .weight = creature_skin::BlendWeight(morph.drawn.evilGood),
	    .tattoos = tattooRevision,
	    .marks = marks.revision,
	    .art = art != nullptr,
	};
	if (skin.painted == wanted)
	{
		return;
	}
	Paint(skin, *sources, wanted.weight, tattoos, masks, marks, rig, art);
	skin.painted = wanted;
}
} // namespace

void CreatureSkinSystem::Update()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& arts = Locator::resources::value().GetCreatureSkinArt();
	const creature_skin::Art* art = arts.Contains(creature_skin::k_ArtId) ? &*arts.Handle(creature_skin::k_ArtId) : nullptr;

	registry.Each<const Creature, const CreatureMorph, const CreatureTattoos, const CreatureMarks, CreatureSkin>(
	    [&](const Creature& creature, const CreatureMorph& morph, const CreatureTattoos& tattoos, const CreatureMarks& marks,
	        CreatureSkin& skin) {
		    PaintIfChanged(skin, creature, morph, ShownTattoos(registry, creature.owner, tattoos, tattoos), tattoos.revision,
		                   MasksOf(registry, creature.owner), marks, art);
	    });
	// The creature of the tattoo menu is a thing of the drawn registry. It shows the tattoos and uses the masks of the
	// players of the land.
	if (Locator::drawnRegistry::has_value())
	{
		Locator::drawnRegistry::value()
		    .Each<const Creature, const CreatureMorph, const CreatureTattoos, const CreatureMarks, CreatureSkin>(
		        [&](const Creature& creature, const CreatureMorph& morph, const CreatureTattoos& tattoos,
		            const CreatureMarks& marks, CreatureSkin& skin) {
			        PaintIfChanged(skin, creature, morph, ShownTattoos(registry, creature.owner, tattoos, tattoos),
			                       tattoos.revision, MasksOf(registry, creature.owner), marks, art);
		        });
	}
	// A copy with a skin of its own is painted at its own times. The copy of the land's creature in the temple's
	// creature room is a thing of the temple's rooms.
	const auto paintCopies = [&](Registry& copies) {
		copies.Each<const CreatureCopy, const CreatureTattoos, CreatureSkin>(
		    [&](const CreatureCopy& copy, const CreatureTattoos& tattoos, CreatureSkin& skin) {
			    if (!registry.Valid(copy.creature))
			    {
				    return;
			    }
			    const auto* creature = registry.TryGet<const Creature>(copy.creature);
			    const auto* morph = registry.TryGet<const CreatureMorph>(copy.creature);
			    const auto* body = registry.TryGet<const CreatureTattoos>(copy.creature);
			    const auto* marks = registry.TryGet<const CreatureMarks>(copy.creature);
			    if (creature == nullptr || morph == nullptr || body == nullptr || marks == nullptr)
			    {
				    return;
			    }
			    PaintIfChanged(skin, *creature, *morph, ShownTattoos(registry, creature->owner, tattoos, *body),
			                   tattoos.revision, MasksOf(registry, creature->owner), *marks, art);
		    });
	};
	paintCopies(registry);
	if (Locator::temple::has_value())
	{
		paintCopies(Locator::temple::value().GetRooms());
	}
}

void CreatureSkinSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<CreatureMarks>([](CreatureMarks& marks) {
		if (creature_marks::Heal(marks.marks, creature_marks::k_CountsPerTurn))
		{
			++marks.revision;
		}
	});
}

void CreatureSkinSystem::CreatureTurn(entt::entity creature)
{
	if (auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature);
	    marks != nullptr && creature_marks::Heal(marks->marks, creature_marks::k_CountsPerTurn))
	{
		++marks->revision;
	}
}

void CreatureSkinSystem::SetTattoos(entt::entity creature, const creature_tattoo::Slots& slots)
{
	if (auto* tattoos = Locator::entitiesRegistry::value().TryGet<CreatureTattoos>(creature); tattoos != nullptr)
	{
		tattoos->slots = slots;
	}
}

void CreatureSkinSystem::ShowTattoos(entt::entity creature, std::optional<creature_tattoo::Slots> slots)
{
	if (auto* tattoos = Locator::entitiesRegistry::value().TryGet<CreatureTattoos>(creature); tattoos != nullptr)
	{
		tattoos->shown = slots;
	}
}

void CreatureSkinSystem::SetTattooPictures(entt::entity creature, std::vector<uint8_t> pictures)
{
	if (auto* tattoos = Locator::entitiesRegistry::value().TryGet<CreatureTattoos>(creature); tattoos != nullptr)
	{
		tattoos->pictures = std::move(pictures);
	}
}

void CreatureSkinSystem::RemakeTattooMasks(entt::entity creature, creature_tattoo::Sheet sheet)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& arts = Locator::resources::value().GetCreatureSkinArt();
	const auto* owner = registry.TryGet<const Creature>(creature);
	const auto* tattoos = registry.TryGet<const CreatureTattoos>(creature);
	if (owner == nullptr || tattoos == nullptr || !arts.Contains(creature_skin::k_ArtId))
	{
		return;
	}
	const auto player = PlayerEntityOf(registry, owner->owner);
	if (!player.has_value())
	{
		return;
	}
	auto* masks = registry.TryGet<TattooMasks>(*player);
	if (masks == nullptr)
	{
		masks = &registry.Assign<TattooMasks>(*player);
	}
	const auto& art = *arts.Handle(creature_skin::k_ArtId);
	// The masks follow what the skin shows: a change from the tattoo editor that waits for its turn comes first
	creature_tattoo::RemakeMasks(masks->masks, tattoos->shown.value_or(tattoos->slots),
	                             tattoos->picturesShown ? tattoos->shownPictures : tattoos->pictures,
	                             sheet == creature_tattoo::Sheet::Shipped ? art.shippedDesigns : art.designs);
}

void CreatureSkinSystem::PaintTattoos(entt::entity creature)
{
	if (auto* tattoos = Locator::entitiesRegistry::value().TryGet<CreatureTattoos>(creature); tattoos != nullptr)
	{
		++tattoos->revision;
	}
}

void CreatureSkinSystem::DoTattoo(PlayerNames player, const player_commands::CreatureTattoo& tattoo)
{
	auto* tattoos = Locator::entitiesRegistry::value().TryGet<CreatureTattoos>(tattoo.creature);
	if (tattoos == nullptr)
	{
		return;
	}
	tattoos->slots = tattoo.slots;
	if (tattoo.hasPictures)
	{
		tattoos->pictures = tattoo.pictures;
	}
	// The skin stops showing a change that now is on the creature
	if (tattoos->shown == tattoos->slots)
	{
		tattoos->shown.reset();
	}
	if (tattoos->picturesShown && tattoos->shownPictures == tattoos->pictures)
	{
		tattoos->picturesShown = false;
		tattoos->shownPictures.clear();
	}
	// The masks of the player are made again and the skin is painted, as the editor does on the computer of the player.
	// Only the computer of the player has the player's own pictures.
	const bool local = input::IsLocalPlayer(Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr, player);
	RemakeTattooMasks(tattoo.creature, local ? creature_tattoo::Sheet::OwnPictures : creature_tattoo::Sheet::Shipped);
	PaintTattoos(tattoo.creature);
}

void CreatureSkinSystem::PutOnTattoos(entt::entity creature, const creature_tattoo::Slots& slots,
                                      const creature_tattoo::Pictures& pictures)
{
	SetTattoos(creature, slots);
	SetTattooPictures(creature, creature_tattoo::PicturesBlock(slots, pictures));
	RemakeTattooMasks(creature, creature_tattoo::Sheet::OwnPictures);
	PaintTattoos(creature);
}

void CreatureSkinSystem::AddWound(entt::entity creature, const creature_marks::Mark& wound)
{
	if (auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature); marks != nullptr)
	{
		creature_marks::Add(marks->marks.wounds, wound);
		++marks->revision;
	}
}

void CreatureSkinSystem::AddBlood(entt::entity creature, const creature_marks::Mark& blood)
{
	if (auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature); marks != nullptr)
	{
		creature_marks::Add(marks->marks.blood, blood);
		++marks->revision;
	}
}

void CreatureSkinSystem::Heal(entt::entity creature, uint32_t counts)
{
	if (auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature);
	    marks != nullptr && creature_marks::Heal(marks->marks, counts))
	{
		++marks->revision;
	}
}
