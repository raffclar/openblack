/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaParticleImport.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <ranges>
#include <span>
#include <string_view>

#include <VanillaWorld.h>

using namespace openblack;
using namespace openblack::ecs;
using gamesave::VanillaRecord;
using gamesave::VanillaType;
using gamesave::VanillaWorld;
namespace particle_import = openblack::ecs::vanilla_particle_import;

namespace
{

/// The deepest that collections and atoms go under one effect. The saves of the tests go 6 deep.
constexpr int k_MaxDepth = 64;

/// The size of the saved process info of an effect
constexpr size_t k_ProcessInfoSize = 60;

/// The size of a saved draw state: a matrix, a position, a scale and the alpha with three bytes after it
constexpr size_t k_DrawStateSize = 56;

/// The bits of the flags of a saved atom
constexpr uint8_t k_AtomDrawOnFirstUpdate = 1U << 0U;
constexpr uint8_t k_AtomStepsShift = 1U;
constexpr uint8_t k_AtomStepsMask = 3U;
constexpr uint8_t k_AtomDeflected = 1U << 3U;
constexpr uint8_t k_AtomVisible = 1U << 4U;

/// The bits of the flags of a saved collection
constexpr uint8_t k_CollectionNotInHierarchy = 1U << 0U;
constexpr uint8_t k_CollectionInterpolated = 1U << 1U;
constexpr uint8_t k_CollectionEmitting = 1U << 2U;

/// The bit of the flags of a saved effect that tells that it closes down
constexpr uint32_t k_EffectClosing = 1U << 0U;

float FloatAt(std::span<const uint8_t> bytes, size_t at)
{
	uint32_t word = 0;
	std::memcpy(&word, bytes.data() + at, sizeof(word));
	return std::bit_cast<float>(word);
}

uint32_t WordAt(std::span<const uint8_t> bytes, size_t at)
{
	uint32_t word = 0;
	std::memcpy(&word, bytes.data() + at, sizeof(word));
	return word;
}

glm::vec3 PointOf(const VanillaWorld& world, const VanillaRecord& record, std::string_view name)
{
	const auto point = world.Point(record, name).value_or(std::array<float, 3> {});
	return {point[0], point[1], point[2]};
}

/// A matrix of the file, whose rows are the images of the three axes, as the columns of a matrix of the game
glm::mat3 MatrixAt(std::span<const uint8_t> bytes, size_t at)
{
	glm::mat3 matrix(1.0f);
	for (int row = 0; row < 3; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			matrix[row][column] = FloatAt(bytes, at + static_cast<size_t>((row * 3 + column) * 4));
		}
	}
	return matrix;
}

/// A saved draw state. The file keeps the rotation times the scale, so the rotation is the matrix over the scale.
particles::DrawState DrawStateOf(std::span<const uint8_t> bytes, float stretch, float frame)
{
	particles::DrawState state;
	const auto matrix = MatrixAt(bytes, 0);
	state.position = {FloatAt(bytes, 36), FloatAt(bytes, 40), FloatAt(bytes, 44)};
	state.scale = FloatAt(bytes, 48);
	state.rotation = state.scale != 0.0f ? matrix / state.scale : glm::mat3(1.0f);
	state.alpha = static_cast<float>(bytes[52]);
	state.stretch = stretch;
	state.frame = frame;
	return state;
}

std::string Why(const VanillaRecord& record, std::string_view why)
{
	return "object " + std::to_string(record.number) + " of type " + std::to_string(record.type) + ": " + std::string(why);
}

class EffectReader
{
public:
	EffectReader(const VanillaWorld& world, const particle_import::EntityOf& entityOf)
	    : _world(world)
	    , _entityOf(entityOf)
	{
	}

	[[nodiscard]] const std::string& Error() const { return _why; }

	std::optional<particle_import::ImportedEffect> Effect(uint32_t systemNumber)
	{
		const auto* system = Expect(systemNumber, VanillaType::ParticleSystem);
		if (system == nullptr)
		{
			return std::nullopt;
		}
		const auto* manager = Expect(_world.Ref(*system, "systemManager").value_or(0), VanillaType::ParticleManager);
		if (manager == nullptr)
		{
			return std::nullopt;
		}
		if (_world.Ref(*manager, "managerSpell").value_or(0) != 0)
		{
			return Fail(*manager, "the effect belongs to a miracle");
		}
		particle_import::ImportedEffect imported;
		imported.type = static_cast<ParticleType>(_world.Word(*manager, "managerType").value_or(0));
		imported.synced = _world.Byte(*manager, "managerSynced").value_or(1) != 0;
		auto& state = imported.state;
		state.atomCount = _world.Word(*manager, "managerAtomCount").value_or(0);
		if (const auto player = _world.Ref(*manager, "managerPlayer").value_or(0); player != 0)
		{
			const auto* record = Expect(player, VanillaType::Player);
			if (record == nullptr)
			{
				return std::nullopt;
			}
			state.player = _world.Byte(*record, "number").value_or(0);
		}
		// The process info: three words, the hand, the camera, the direction, the power, the spin and the enabled byte
		const auto info = _world.Raw(*manager, "managerInfo");
		if (info.size() != k_ProcessInfoSize)
		{
			return Fail(*manager, "the effect has no process info");
		}
		state.info.handPosition = {FloatAt(info, 12), FloatAt(info, 16), FloatAt(info, 20)};
		state.info.cameraForward = {FloatAt(info, 24), FloatAt(info, 28), FloatAt(info, 32)};
		state.info.direction = {FloatAt(info, 36), FloatAt(info, 40), FloatAt(info, 44)};
		state.info.power = FloatAt(info, 48);
		state.info.spin = FloatAt(info, 52);
		state.info.enabled = info[56] != 0;
		state.origin = PointOf(_world, *manager, "managerOrigin");
		state.globalAlpha = static_cast<float>(_world.Byte(*manager, "managerAlpha").value_or(255));
		state.age = _world.Float(*manager, "managerAge").value_or(0.0f);
		state.closing = (_world.Word(*manager, "managerFlags").value_or(0) & k_EffectClosing) != 0;
		state.direction = PointOf(_world, *manager, "managerDirection");
		state.magnitude = _world.Float(*manager, "managerMagnitude").value_or(1.0f);
		state.closeAge = _world.Float(*manager, "managerCloseAge").value_or(0.0f);
		state.firstStepTwice = _world.Byte(*manager, "managerStepTwice").value_or(0) != 0;
		for (const auto* field : _world.Fields(*manager, "targetObject"))
		{
			state.targets.push_back(_entityOf(field->number));
		}
		for (const auto* field : _world.Fields(*manager, "targetPoint"))
		{
			const auto bytes = _world.Bytes().subspan(field->at, field->size);
			state.targetPositions.emplace_back(FloatAt(bytes, 0), FloatAt(bytes, 4), FloatAt(bytes, 8));
		}
		// The file keeps the newest first collection first
		const auto roots = _world.Refs(*manager, "managerRoots");
		for (const auto root : std::views::reverse(roots))
		{
			auto collection = Collection(root, 0);
			if (!collection.has_value())
			{
				return std::nullopt;
			}
			state.roots.push_back(std::move(*collection));
		}
		return imported;
	}

private:
	const VanillaRecord* Expect(uint32_t number, VanillaType type)
	{
		const auto* record = number != 0 ? _world.Record(number) : nullptr;
		if (record == nullptr || !record->Is(type))
		{
			_why = "object " + std::to_string(number) + " is not of type " + std::to_string(static_cast<uint32_t>(type));
			return nullptr;
		}
		return record;
	}

	std::nullopt_t Fail(const VanillaRecord& record, std::string_view why)
	{
		_why = Why(record, why);
		return std::nullopt;
	}

	std::optional<particles::RestoredCollection> Collection(uint32_t number, int depth)
	{
		const auto* record = Expect(number, VanillaType::AtomCollection);
		if (record == nullptr)
		{
			return std::nullopt;
		}
		if (depth > k_MaxDepth)
		{
			return Fail(*record, "the collections go too deep");
		}
		particles::RestoredCollection collection;
		collection.group = static_cast<int>(_world.Word(*record, "collectionGroup").value_or(0));
		collection.birth = _world.Float(*record, "collectionBirth").value_or(0.0f);
		collection.alpha = static_cast<float>(_world.Byte(*record, "collectionAlpha").value_or(255));
		const auto flags = _world.Byte(*record, "collectionFlags").value_or(0xFF);
		collection.hierarchy = (flags & k_CollectionNotInHierarchy) == 0;
		collection.interpolated = (flags & k_CollectionInterpolated) != 0;
		collection.emitting = (flags & k_CollectionEmitting) != 0;
		if (_world.Ref(*record, "collectionUnused").value_or(0) != 0)
		{
			return Fail(*record, "the collection has a value that no save of the tests has");
		}
		for (const auto* field : _world.Fields(*record, "rule"))
		{
			collection.rules.push_back(static_cast<int>(WordAt(_world.Bytes(), field->at)));
		}
		for (const auto data : _world.Refs(*record, "ruleData"))
		{
			const auto* emitter = Expect(data, VanillaType::EmitterRuleData);
			if (emitter == nullptr)
			{
				return std::nullopt;
			}
			collection.emitters.push_back({
			    .rule = static_cast<int>(_world.Word(*emitter, "dataRule").value_or(0)),
			    .clock =
			        {
			            .next = _world.Float(*emitter, "emitterNext").value_or(0.0f),
			            .emitted = static_cast<int>(_world.Word(*emitter, "emitterEmitted").value_or(0)),
			            .started = _world.Byte(*emitter, "emitterNotStarted").value_or(1) == 0,
			        },
			});
		}
		// The file keeps the newest atom first
		const auto atoms = _world.Refs(*record, "collectionAtoms");
		for (const auto atomNumber : std::views::reverse(atoms))
		{
			auto atom = Atom(atomNumber, depth);
			if (!atom.has_value())
			{
				return std::nullopt;
			}
			collection.atoms.push_back(std::move(*atom));
		}
		return collection;
	}

	std::optional<particles::RestoredAtom> Atom(uint32_t number, int depth)
	{
		const auto* record = Expect(number, VanillaType::AtomCore);
		if (record == nullptr)
		{
			return std::nullopt;
		}
		if (!_world.Refs(*record, "atomRuleData").empty() || !_world.Refs(*record, "atomSounds").empty())
		{
			return Fail(*record, "the atom has rule data or sounds, which no save of the tests has");
		}
		if (_world.Ref(*record, "atomDrawOffset").value_or(0) != 0)
		{
			return Fail(*record, "the atom has a draw offset, which no save of the tests has");
		}
		particles::RestoredAtom atom;
		atom.velocity = PointOf(_world, *record, "atomVelocity");
		atom.birth = _world.Float(*record, "atomBirth").value_or(0.0f);
		const auto rotation = _world.Raw(*record, "atomRotation");
		const auto previous = _world.Raw(*record, "atomPrevious");
		const auto current = _world.Raw(*record, "atomCurrent");
		const auto animation = _world.Raw(*record, "atomAnimation");
		if (rotation.size() != 48 || previous.size() != k_DrawStateSize || current.size() != k_DrawStateSize ||
		    animation.size() != 12)
		{
			return Fail(*record, "the atom has no whole state");
		}
		atom.rotation = MatrixAt(rotation, 0);
		atom.baseScale = _world.Float(*record, "atomBaseScale").value_or(1.0f);
		atom.ruleScale = _world.Float(*record, "atomRuleScale").value_or(1.0f);
		atom.stretch = _world.Float(*record, "atomStretch").value_or(1.0f);
		atom.position = PointOf(_world, *record, "atomPosition");
		// The colour is kept as blue, green, red and alpha
		const auto colour = _world.Word(*record, "atomColour").value_or(0xFFFFFFFFU);
		atom.rgba = {static_cast<uint8_t>(colour >> 16U), static_cast<uint8_t>(colour >> 8U), static_cast<uint8_t>(colour),
		             static_cast<uint8_t>(colour >> 24U)};
		const auto specular = _world.Word(*record, "atomSpecular").value_or(0);
		atom.specular = {static_cast<uint8_t>(specular >> 16U), static_cast<uint8_t>(specular >> 8U),
		                 static_cast<uint8_t>(specular)};
		const auto flags = static_cast<uint8_t>(_world.Half(*record, "atomFlags").value_or(0));
		atom.drawOnFirstUpdate = (flags & k_AtomDrawOnFirstUpdate) != 0;
		atom.steps = static_cast<uint8_t>((flags >> k_AtomStepsShift) & k_AtomStepsMask);
		atom.deflected = (flags & k_AtomDeflected) != 0;
		atom.visible = (flags & k_AtomVisible) != 0;
		atom.previous = DrawStateOf(previous, atom.stretch, _world.Float(*record, "atomPreviousFrame").value_or(0.0f));
		atom.current = DrawStateOf(current, atom.stretch, _world.Float(*record, "atomFrame").value_or(0.0f));
		atom.frameRate = FloatAt(animation, 0);
		atom.playAnim = animation[8] != 0;
		atom.gravity = _world.Float(*record, "atomGravity").value_or(1.0f);
		atom.random = _world.Byte(*record, "atomRandom").value_or(0);
		const auto* render = _world.Record(_world.Ref(*record, "atomRender").value_or(0));
		if (render == nullptr)
		{
			return Fail(*record, "the atom has no look");
		}
		if (render->Is(VanillaType::SpriteParticle))
		{
			atom.look = particles::RestoredLook::Sprite;
			atom.creator = static_cast<int>(_world.Word(*render, "spriteCreator").value_or(0));
		}
		else if (render->Is(VanillaType::GoldenShowerParticle) || render->Is(VanillaType::ObjectRefParticle))
		{
			atom.look = render->Is(VanillaType::GoldenShowerParticle) ? particles::RestoredLook::GoldenShower
			                                                          : particles::RestoredLook::ObjectRef;
			const auto object = _world.Ref(*render, "objectRef").value_or(0);
			atom.object = object != 0 ? _entityOf(object) : entt::null;
		}
		else if (!render->Is(VanillaType::PointParticle))
		{
			return Fail(*render, "the import does not know this look of an atom");
		}
		const auto collections = _world.Refs(*record, "atomCollections");
		for (const auto sub : std::views::reverse(collections))
		{
			auto collection = Collection(sub, depth + 1);
			if (!collection.has_value())
			{
				return std::nullopt;
			}
			atom.collections.push_back(std::move(*collection));
		}
		return atom;
	}

	const VanillaWorld& _world;
	const particle_import::EntityOf& _entityOf;
	std::string _why;
};

} // namespace

particle_import::ReadResult particle_import::ReadEffect(const VanillaWorld& world, uint32_t system, const EntityOf& entityOf)
{
	EffectReader reader(world, entityOf);
	auto effect = reader.Effect(system);
	return {.effect = std::move(effect), .why = reader.Error()};
}

particle_import::ContainerResult particle_import::ReadContainer(const VanillaWorld& world, uint32_t number,
                                                                const EntityOf& entityOf)
{
	const auto* record = world.Record(number);
	if (record == nullptr || !record->Is(VanillaType::ParticleContainer))
	{
		return {.why = "object " + std::to_string(number) + " is not a particle container"};
	}
	ImportedContainer container {.number = number};
	if (const auto bytes = world.Raw(*record, "position"); bytes.size() == sizeof(container.position))
	{
		std::memcpy(&container.position, bytes.data(), sizeof(container.position));
	}
	container.singleSort = world.Byte(*record, "containerSingleSort").value_or(0) == 1;
	container.follows = world.Ref(*record, "containerFollows").value_or(0);
	container.turns = static_cast<int32_t>(world.Word(*record, "containerTurns").value_or(0xFFFFFFFFU));
	container.scale = world.Float(*record, "containerScale").value_or(1.0f);
	auto read = ReadEffect(world, world.Ref(*record, "containerSystem").value_or(0), entityOf);
	if (!read.effect.has_value())
	{
		return {.why = Why(*record, read.why)};
	}
	container.effect = std::move(*read.effect);
	return {.container = std::move(container)};
}

particle_import::Containers particle_import::ReadContainers(const VanillaWorld& world, const EntityOf& entityOf)
{
	Containers result;
	if (world.Records().empty())
	{
		return result;
	}
	const auto lists = world.Fields(world.Game(), "lists");
	if (lists.size() <= k_ParticleContainerList)
	{
		return result;
	}
	// The game keeps the newest container first, and steps the effects from the oldest
	const auto numbers = world.Refs(*lists[k_ParticleContainerList]);
	for (const auto number : std::views::reverse(numbers))
	{
		auto read = ReadContainer(world, number, entityOf);
		if (read.container.has_value())
		{
			result.containers.push_back(std::move(*read.container));
		}
		else
		{
			result.skipped.push_back(std::move(read.why));
		}
	}
	return result;
}

particle_import::VorticesIn particle_import::ReadVorticesIn(const VanillaWorld& world, const EntityOf& entityOf)
{
	VorticesIn result;
	for (const auto* record : world.OfType(VanillaType::LandscapeVortexIn))
	{
		ImportedVortexIn vortex {.number = record->number};
		vortex.centre = PointOf(world, *record, "vortexCentre");
		vortex.state = static_cast<VortexStateType>(world.Word(*record, "vortexState").value_or(1));
		vortex.altitude = world.Float(*record, "vortexAltitude").value_or(0.0f);
		vortex.groundAverage = world.Float(*record, "vortexGroundAverage").value_or(0.0f);
		for (const auto* field : world.Fields(*record, "vortexGround"))
		{
			vortex.groundHeights.push_back(world.Bytes()[field->at]);
		}
		if (const auto* writer = world.Record(world.Ref(*record, "vortexCrossing").value_or(0));
		    writer != nullptr && writer->Is(VanillaType::VortexCrossingWriter))
		{
			const auto text = world.Raw(*writer, "file");
			vortex.crossing = std::vector<uint8_t>(text.begin(), text.end());
		}
		if (const auto mover = world.Ref(*record, "vortexMover").value_or(0); mover != 0)
		{
			auto read = ReadEffect(world, mover, entityOf);
			if (!read.effect.has_value())
			{
				result.skipped.push_back(Why(*record, read.why));
				continue;
			}
			vortex.mover = std::move(*read.effect);
		}
		result.vortices.push_back(std::move(vortex));
	}
	return result;
}
