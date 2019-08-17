#pragma once
#include <Entities/Components/Component.h>

namespace OpenBlack
{
namespace Entities
{
enum class VillageEthnicities
{
	GLOBAL,
	AZTEC,
	CELTIC,
	EGYPTIAN,
	GREEK,
	INDIAN,
	JAPANESE,
	NORSE,
	TIBETAN
};

enum class VillagerTypes
{
	FARMER,
	FISHERMAN,
	FORESTER,
	BREEDER,
	SHEPHERD,
	MISSIONARY
};

enum class VillagerTasks
{
	IDLE
};

class Villager: Component
{
public:
	explicit Villager(EntityId id, VillageEthnicities ethnicity, VillagerTypes role, VillagerTasks task, uint32_t age, uint32_t health, uint32_t hunger)
		: Component(id),
		  _ethnicity(ethnicity),
		  _role(role),
		  _task(task),
		  _age(age),
		  _health(health),
		  _hunger(hunger)
	{
	}

private:
	VillageEthnicities _ethnicity;
	VillagerTypes _role;
	VillagerTasks _task;

	uint32_t _age;
	uint32_t _health;
	uint32_t _hunger;
};
} // namespace Entities
} // namespace OpenBlack
