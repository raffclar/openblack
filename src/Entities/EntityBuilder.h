#include <Entities/Components/Component.h>
#include <Entities/Components/Villager.h>

namespace OpenBlack
{
namespace Entities
{

class EntityBuilder
{
  public:
	EntityBuilder SetBuilding(VillageEthnicities ethnicity);
	EntityBuilder SetVillager(VillageEthnicities ethnicity, VillagerType type, uint32_t age);
	EntityBuilder SetPosition();
	EntityId Create();
  private:
	EntityManager _entityManager;
};
} // namespace Entities
} // namespace OpenBlack
