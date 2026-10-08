/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Renderer side of the PSys draw paths (Graphics/Renderer.cpp DrawPass) against the original's queue: a Sorted
// effect has no Z object, each sprite is one at its own point and each chain one at its joint n / 2, so a Queued
// effect (one Z object at its origin) can be drawn between two sprites of the same Sorted effect. The
// entries are made as DrawPass makes them (zsort::Key, (x^2 + y^2) + z^2, then zsort::Queue).

#include <cmath>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/ZSort.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::psys;

namespace
{
std::string Zeros(int from)
{
	std::string zeros;
	for (int i = from; i < 25; ++i)
	{
		zeros += " 0";
	}
	return zeros;
}

std::string CreateRule(const char* name, int group, const char* creator)
{
	return std::string("BEGINCLASS CreateRuleAnAtom ") + name + "\nBEGINPROPERTIES\n" + "PROPERTY Group INTEGER " +
	       std::to_string(group) + "\nPROPERTY NextGroups ARRAY SIZE 0\nPROPERTY PCreator PERSIS_PNTR " + creator +
	       "\nENDPROPERTIES\nENDCLASS\n";
}

/// Group 0: a sprite A, a sprite E with CentreAtBase (raised by height x size x 0.5) and two chain
/// joints B, C; one atom each at the effect's origin
std::shared_ptr<const File> SortedFile()
{
	std::string text = "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 1\nPROPERTY Hierarchies ARRAY SIZE 25 0" + Zeros(1) +
	                   "\nPROPERTY InitiallyCreated ARRAY SIZE 25 1" + Zeros(1) +
	                   "\nPROPERTY MaxSpellAge FLOAT 100\nENDPROPERTIES\n"
	                   "BEGINCLASS ParticleSpriteCreator SpriteA\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                   "ENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS ParticleSpriteCreator SpriteE\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 2\n"
	                   "PROPERTY CentreAtBase BOOL 1\nENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS ParticleChainCreator Joint\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT 1\n"
	                   "ENDPROPERTIES\nENDCLASS\n";
	text += CreateRule("A", 0, "SpriteA");
	text += CreateRule("E", 0, "SpriteE");
	text += CreateRule("B", 0, "Joint");
	text += CreateRule("C", 0, "Joint");
	auto file = File::Parse(text, "test_sorted_queue");
	EXPECT_TRUE(file.has_value());
	return file.has_value() ? std::make_shared<const File>(std::move(*file)) : nullptr;
}

uint32_t StartStepped(glm::vec3 origin)
{
	const auto id = manager::Start(SortedFile(), origin, 1.0f);
	EXPECT_NE(id, 0u);
	if (auto* effect = manager::Find(id); effect != nullptr)
	{
		effect->Step(0.1f);
	}
	return id;
}

/// What a drained entry is (Renderer.cpp's ZObject, reduced)
struct Entry
{
	char kind;     ///< 's' a Sorted sprite, 'c' a Sorted chain, 'q' a Queued effect
	float initial; ///< the sprite's creator's InitialScale (which sprite it is)
	uint32_t effect;
};

/// The effects a test starts (with their draw paths) go after it, also when an ASSERT stops it before its Delete
class PSysSortedQueue: public ::testing::Test
{
protected:
	void TearDown() override { manager::Clear(); }
};
} // namespace

TEST_F(PSysSortedQueue, SpritesAndChainsKeyedOnTheirOwn)
{
	const glm::vec3 camera(0.0f);
	const auto sorted = StartStepped(glm::vec3(0.0f, 0.0f, 10.0f));
	const auto frame = manager::CollectSorted();

	zsort::Queue<Entry> queue;
	queue.Begin();
	int sprites = 0;
	float keyA = 0.0f;
	float keyE = 0.0f;
	for (const auto& atom : frame.sprites)
	{
		if (atom.effect != sorted)
		{
			continue;
		}
		++sprites;
		// the sprite's own point: the atom's position raised with CentreAtBase
		glm::vec3 point = atom.atom.position;
		if (atom.atom.creator->centreAtBase)
		{
			point.y += atom.atom.stretch * std::max(atom.atom.scale, 1e-4f) * 0.5f;
		}
		EXPECT_EQ(atom.key, point);
		const float key = zsort::Key(atom.key, camera);
		EXPECT_FLOAT_EQ(key, (atom.key.x * atom.key.x + atom.key.y * atom.key.y) + atom.key.z * atom.key.z);
		(atom.atom.creator->centreAtBase ? keyE : keyA) = key;
		queue.Submit({'s', atom.atom.creator->initialScale, atom.effect}, key);
	}
	ASSERT_EQ(sprites, 2); // one Z object per sprite, none for the effect
	ASSERT_GT(keyE, keyA); // E raised: farther from a camera below it
	int chains = 0;
	for (const auto& chain : frame.chains)
	{
		if (chain.effect == sorted)
		{
			++chains;
			// the middle joint, n / 2 rounded towards zero
			ASSERT_EQ(chain.chain.joints.size(), 2u);
			EXPECT_EQ(chain.key, chain.chain.joints[1].position);
			queue.Submit({'c', 0.0f, chain.effect}, zsort::Key(chain.key, camera));
		}
	}
	EXPECT_EQ(chains, 1);

	// a Queued effect whose origin lies between the two sprites: its single Z object goes between
	// them, which one Z object for the whole Sorted effect could not give
	const float between = std::sqrt((keyA + keyE) * 0.5f);
	const auto queued = StartStepped(glm::vec3(0.0f, 0.0f, between));
	manager::SetDrawPath(queued, DrawPath::Queued);
	int queuedEntries = 0;
	for (const auto& effect : manager::CollectQueued())
	{
		if (effect.effect == queued)
		{
			++queuedEntries;
			queue.Submit({'q', 0.0f, effect.effect}, zsort::Key(effect.origin, camera));
		}
	}
	EXPECT_EQ(queuedEntries, 1);
	// the hand's effects are not in the queue (the hand draws them inside its own entry)
	for (const auto& effect : manager::HandEffects())
	{
		EXPECT_NE(effect.effect, sorted);
		EXPECT_NE(effect.effect, queued);
	}

	// far to near: E, then the Queued effect, then A and the chain at the effect's origin (equal keys: the
	// order they came in)
	std::vector<Entry> order;
	for (const auto& entry : queue.Drain())
	{
		order.push_back(*entry.item);
	}
	ASSERT_EQ(order.size(), 4u);
	EXPECT_EQ(order[0].kind, 's');
	EXPECT_FLOAT_EQ(order[0].initial, 2.0f); // E
	EXPECT_EQ(order[1].kind, 'q');
	EXPECT_EQ(order[1].effect, queued);
	EXPECT_EQ(order[2].kind, 's');
	EXPECT_FLOAT_EQ(order[2].initial, 1.0f); // A
	EXPECT_EQ(order[3].kind, 'c');

	manager::Delete(sorted);
	manager::Delete(queued);
}

namespace
{
/// Group 0: sprites A (InitialScale 1) and B (2), each with a child collection of group 1, sprite C (3)
std::shared_ptr<const File> NestedFile()
{
	std::string text = "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 1\nPROPERTY Hierarchies ARRAY SIZE 25 0" + Zeros(1) +
	                   "\nPROPERTY InitiallyCreated ARRAY SIZE 25 1" + Zeros(1) +
	                   "\nPROPERTY MaxSpellAge FLOAT 100\nENDPROPERTIES\n";
	for (const auto& [name, scale] : {std::pair {"SpriteA", "1"}, std::pair {"SpriteB", "2"}, std::pair {"SpriteC", "3"}})
	{
		text += std::string("BEGINCLASS ParticleSpriteCreator ") + name + "\nBEGINPROPERTIES\nPROPERTY InitialScale FLOAT " +
		        scale + "\nENDPROPERTIES\nENDCLASS\n";
	}
	for (const auto& [name, group, next, creator] :
	     {std::tuple {"A", 0, "1 1", "SpriteA"}, std::tuple {"B", 0, "1 1", "SpriteB"}, std::tuple {"C", 1, "0", "SpriteC"}})
	{
		text += std::string("BEGINCLASS CreateRuleAnAtom ") + name + "\nBEGINPROPERTIES\nPROPERTY Group INTEGER " +
		        std::to_string(group) + "\nPROPERTY NextGroups ARRAY SIZE " + next + "\nPROPERTY PCreator PERSIS_PNTR " +
		        creator + "\nENDPROPERTIES\nENDCLASS\n";
	}
	auto file = File::Parse(text, "test_nested_order");
	EXPECT_TRUE(file.has_value());
	return file.has_value() ? std::make_shared<const File>(std::move(*file)) : nullptr;
}
} // namespace

// A collection's atoms are drawn first, then each atom's children: A, B, then the two C
// (pre-order would give A, C, B, C)
TEST_F(PSysSortedQueue, CollectWalksAtomsBeforeTheirChildren)
{
	const auto id = manager::Start(NestedFile(), glm::vec3(0.0f, 0.0f, 10.0f), 1.0f);
	ASSERT_NE(id, 0u);
	auto* effect = manager::Find(id);
	ASSERT_NE(effect, nullptr);
	effect->Step(0.1f);
	std::vector<Effect::DrawAtom> atoms;
	effect->Collect(1.0f, atoms);
	std::vector<float> order;
	for (const auto& atom : atoms)
	{
		order.push_back(atom.creator->initialScale);
	}
	EXPECT_EQ(order, (std::vector<float> {1.0f, 2.0f, 3.0f, 3.0f}));
	manager::Delete(id);
}
