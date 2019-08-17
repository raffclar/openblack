#include "EntityBuilder.h"

namespace OpenBlack::Entities
{
EntityBuilder& EntityBuilder::SetBuilding(VillageEthnicities ethnicity)
{
	return *this;
}

EntityBuilder& EntityBuilder::SetVillager(VillageEthnicities ethnicity, VillagerTypes type, uint32_t age)
{
	//auto villager = Villager();

	return *this;
}

EntityBuilder& EntityBuilder::SetPosition()
{
	return *this;
}
} // namespace OpenBlack::Entities
