#pragma once
#include <3D/SkinnedModel.h>
#include <Entities/Components/Component.h>

namespace OpenBlack
{
namespace Entities
{
class Model: public Component
{
public:
	explicit Model(EntityId entityId, std::shared_ptr<SkinnedModel> model):
		Component(entityId), _model(model)
	{
	}

	std::shared_ptr<SkinnedModel> GetModel()
	{
		return _model;
	}

private:
	std::shared_ptr<SkinnedModel> _model;
};
} // namespace Entities
} // namespace OpenBlack
