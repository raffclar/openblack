/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rule that stands a ring of light round each of its atoms, as the singing stones' heal does

#include <any>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ParticleFile.h>

#include "LightSheet.h"
#include "ObjectLightSheet.h"
#include "ParticleArchive.h"
#include "ParticleClassRegistry.h"
#include "ParticleEffect.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;
namespace rules = openblack::particles::object_light_sheet;

namespace
{
/// What the rule keeps for each atom: the atom's colour and its ring, once made
struct RingHold
{
	uint32_t argb {rules::k_OtherAtomArgb};
	std::shared_ptr<LightSheet> sheet;
};

/// The rule makes its atoms once in each collection. Each atom gets a ring of light on the land round the place where
/// the atom is when its ring is made, and the ring stays there. The ring takes the colour that the atom was given when
/// it was made, as bright as the atom is opaque at each step. Its radius comes from the rule's float value when it has
/// one, else from its default radius.
class LightSheetOnObject final: public Modifier
{
public:
	explicit LightSheetOnObject(const ParticleObject& object)
	    : _creator(object.String("PCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	    , _radiusProvider(object.String("RadiusFP"))
	    , _defaultRadius(object.Float("DefaultRadius", rules::k_DefaultRadius))
	    , _atoms(object.Int("NumAtoms", rules::k_DefaultAtoms))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	void SaveSlotData(const Collection::Slot& slot, EffectWriter& out) const override
	{
		out.Write(std::any_cast<bool>(slot.data));
	}
	void LoadSlotData(Collection::Slot& slot, EffectReader& in) const override
	{
		bool made = false;
		in.Read(made);
		slot.data = made;
	}
	/// A save keeps the colour of each atom but not its ring: the first step after a load makes the ring again
	void SaveHeld(const AtomRuleData& data, EffectWriter& out) const override
	{
		out.Write(std::static_pointer_cast<const RingHold>(data.held)->argb);
	}
	void LoadHeld(AtomRuleData& data, EffectReader& in) const override
	{
		RingHold hold;
		in.Read(hold.argb);
		data.held = std::make_shared<const RingHold>(std::move(hold));
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* creator = effect.FindCreator(_creator);
		if (creator == nullptr)
		{
			return false;
		}
		auto& world = effect.Services().world;
		if (!slot.data.has_value())
		{
			slot.data = true;
			const int player = effect.GetPlayer();
			const auto argb = rules::AtomArgb(player >= 0 ? std::optional<uint32_t>(world.PlayerColour(player)) : std::nullopt);
			for (int i = 0; i < _atoms; ++i)
			{
				auto& atom = effect.NewAtom(collection, creator, _nextGroups);
				atom.data[this].held = std::make_shared<const RingHold>(RingHold {.argb = argb, .sheet = nullptr});
			}
		}

		for (auto& atom : collection.atoms)
		{
			auto& data = atom->data[this];
			const auto* hold = static_cast<const RingHold*>(data.held.get());
			if (hold == nullptr || hold->sheet == nullptr)
			{
				// An atom that the rule did not make has the rule's white
				const auto argb = hold != nullptr ? hold->argb : rules::k_OtherAtomArgb;
				data.held =
				    std::make_shared<const RingHold>(RingHold {.argb = argb, .sheet = MakeRing(effect, atom->position)});
				hold = static_cast<const RingHold*>(data.held.get());
			}
			hold->sheet->SetRgb(rules::RingArgb(hold->argb, atom->rgba[3]));
		}
		return true;
	}

private:
	/// A ring of light round a place, on the land, with the strength it keeps
	[[nodiscard]] std::shared_ptr<LightSheet> MakeRing(Effect& effect, glm::vec3 place) const
	{
		auto& world = effect.Services().world;
		// A rule whose file names no float value that the effect has takes its default radius
		const float radius = effect.FloatProvider(_radiusProvider, _defaultRadius);
		auto sheet = std::make_shared<LightSheet>();
		sheet->Start(LightSheet::RingPoints({place.x, place.z}, radius, LightSheet::k_RingPoints,
		                                    [&world](glm::vec2 at) { return world.LandHeight(at); }),
		             0xFFFFFFu, rules::k_Height, LightSheet::k_ShiftSeconds);
		sheet->SetSpread(rules::k_Spread);
		sheet->SetStrength(rules::k_Strength);
		world.AddLightSheet(sheet);
		return sheet;
	}

	std::string _creator;
	std::vector<int> _nextGroups;
	std::string _radiusProvider;
	float _defaultRadius;
	int _atoms;
};
} // namespace

void openblack::particles::RegisterLightSheetRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_LightSheetOnObject", ParticleClassRegistry::Make<LightSheetOnObject>);
}
