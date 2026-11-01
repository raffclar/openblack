/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include <array>
#include <span>
#include <string_view>

#include "ComponentReflection.h"

namespace openblack::ecs::reflection
{

void RegisterFields_Components_a(entt::meta_ctx& context);
void RegisterFields_Components_b(entt::meta_ctx& context);
void RegisterFields_Components_ca(entt::meta_ctx& context);
void RegisterFields_Components_ch(entt::meta_ctx& context);
void RegisterFields_Components_cl(entt::meta_ctx& context);
void RegisterFields_Components_co(entt::meta_ctx& context);
void RegisterFields_Components_creature(entt::meta_ctx& context);
void RegisterFields_Components_creaturea(entt::meta_ctx& context);
void RegisterFields_Components_creatureb(entt::meta_ctx& context);
void RegisterFields_Components_creaturec(entt::meta_ctx& context);
void RegisterFields_Components_creatured(entt::meta_ctx& context);
void RegisterFields_Components_creaturee(entt::meta_ctx& context);
void RegisterFields_Components_creaturef(entt::meta_ctx& context);
void RegisterFields_Components_creatureg(entt::meta_ctx& context);
void RegisterFields_Components_creatureh(entt::meta_ctx& context);
void RegisterFields_Components_creaturel(entt::meta_ctx& context);
void RegisterFields_Components_creaturem(entt::meta_ctx& context);
void RegisterFields_Components_creaturen(entt::meta_ctx& context);
void RegisterFields_Components_creatureo(entt::meta_ctx& context);
void RegisterFields_Components_creaturep(entt::meta_ctx& context);
void RegisterFields_Components_creatures(entt::meta_ctx& context);
void RegisterFields_Components_creaturet(entt::meta_ctx& context);
void RegisterFields_Components_creaturev(entt::meta_ctx& context);
void RegisterFields_Components_d(entt::meta_ctx& context);
void RegisterFields_Components_f(entt::meta_ctx& context);
void RegisterFields_Components_g(entt::meta_ctx& context);
void RegisterFields_Components_h(entt::meta_ctx& context);
void RegisterFields_Components_i(entt::meta_ctx& context);
void RegisterFields_Components_l(entt::meta_ctx& context);
void RegisterFields_Components_m(entt::meta_ctx& context);
void RegisterFields_Components_n(entt::meta_ctx& context);
void RegisterFields_Components_o(entt::meta_ctx& context);
void RegisterFields_Components_p(entt::meta_ctx& context);
void RegisterFields_Components_r(entt::meta_ctx& context);
void RegisterFields_Components_s(entt::meta_ctx& context);
void RegisterFields_Components_t(entt::meta_ctx& context);
void RegisterFields_Components_u(entt::meta_ctx& context);
void RegisterFields_Components_v(entt::meta_ctx& context);
void RegisterFields_Components_w(entt::meta_ctx& context);
void RegisterFields_Values_l(entt::meta_ctx& context);
void RegisterFields_Values_z(entt::meta_ctx& context);
void RegisterFields_Values_a(entt::meta_ctx& context);
void RegisterFields_Values_ca(entt::meta_ctx& context);
void RegisterFields_Values_co(entt::meta_ctx& context);
void RegisterFields_Values_creaturea(entt::meta_ctx& context);
void RegisterFields_Values_creatureb(entt::meta_ctx& context);
void RegisterFields_Values_creatured(entt::meta_ctx& context);
void RegisterFields_Values_creaturee(entt::meta_ctx& context);
void RegisterFields_Values_creaturef(entt::meta_ctx& context);
void RegisterFields_Values_creatureg(entt::meta_ctx& context);
void RegisterFields_Values_creatureh(entt::meta_ctx& context);
void RegisterFields_Values_creaturel(entt::meta_ctx& context);
void RegisterFields_Values_creaturem(entt::meta_ctx& context);
void RegisterFields_Values_creatureo(entt::meta_ctx& context);
void RegisterFields_Values_creaturep(entt::meta_ctx& context);
void RegisterFields_Values_creaturer(entt::meta_ctx& context);
void RegisterFields_Values_creatures(entt::meta_ctx& context);
void RegisterFields_Values_creaturet(entt::meta_ctx& context);
void RegisterFields_Values_creaturew(entt::meta_ctx& context);
void RegisterFields_Values_creaturem_2(entt::meta_ctx& context);
void RegisterFields_Values_e(entt::meta_ctx& context);
void RegisterFields_Values_f(entt::meta_ctx& context);
void RegisterFields_Values_g(entt::meta_ctx& context);
void RegisterFields_Values_h(entt::meta_ctx& context);
void RegisterFields_Values_l_2(entt::meta_ctx& context);
void RegisterFields_Values_m(entt::meta_ctx& context);
void RegisterFields_Values_p(entt::meta_ctx& context);
void RegisterFields_Values_s(entt::meta_ctx& context);
void RegisterFields_Values_t(entt::meta_ctx& context);
void RegisterFields_Values_v(entt::meta_ctx& context);

} // namespace openblack::ecs::reflection

void openblack::ecs::reflection::RegisterComponentFields(entt::meta_ctx& context)
{
	RegisterFields_Components_a(context);
	RegisterFields_Components_b(context);
	RegisterFields_Components_ca(context);
	RegisterFields_Components_ch(context);
	RegisterFields_Components_cl(context);
	RegisterFields_Components_co(context);
	RegisterFields_Components_creature(context);
	RegisterFields_Components_creaturea(context);
	RegisterFields_Components_creatureb(context);
	RegisterFields_Components_creaturec(context);
	RegisterFields_Components_creatured(context);
	RegisterFields_Components_creaturee(context);
	RegisterFields_Components_creaturef(context);
	RegisterFields_Components_creatureg(context);
	RegisterFields_Components_creatureh(context);
	RegisterFields_Components_creaturel(context);
	RegisterFields_Components_creaturem(context);
	RegisterFields_Components_creaturen(context);
	RegisterFields_Components_creatureo(context);
	RegisterFields_Components_creaturep(context);
	RegisterFields_Components_creatures(context);
	RegisterFields_Components_creaturet(context);
	RegisterFields_Components_creaturev(context);
	RegisterFields_Components_d(context);
	RegisterFields_Components_f(context);
	RegisterFields_Components_g(context);
	RegisterFields_Components_h(context);
	RegisterFields_Components_i(context);
	RegisterFields_Components_l(context);
	RegisterFields_Components_m(context);
	RegisterFields_Components_n(context);
	RegisterFields_Components_o(context);
	RegisterFields_Components_p(context);
	RegisterFields_Components_r(context);
	RegisterFields_Components_s(context);
	RegisterFields_Components_t(context);
	RegisterFields_Components_u(context);
	RegisterFields_Components_v(context);
	RegisterFields_Components_w(context);
	RegisterFields_Values_l(context);
	RegisterFields_Values_z(context);
	RegisterFields_Values_a(context);
	RegisterFields_Values_ca(context);
	RegisterFields_Values_co(context);
	RegisterFields_Values_creaturea(context);
	RegisterFields_Values_creatureb(context);
	RegisterFields_Values_creatured(context);
	RegisterFields_Values_creaturee(context);
	RegisterFields_Values_creaturef(context);
	RegisterFields_Values_creatureg(context);
	RegisterFields_Values_creatureh(context);
	RegisterFields_Values_creaturel(context);
	RegisterFields_Values_creaturem(context);
	RegisterFields_Values_creatureo(context);
	RegisterFields_Values_creaturep(context);
	RegisterFields_Values_creaturer(context);
	RegisterFields_Values_creatures(context);
	RegisterFields_Values_creaturet(context);
	RegisterFields_Values_creaturew(context);
	RegisterFields_Values_creaturem_2(context);
	RegisterFields_Values_e(context);
	RegisterFields_Values_f(context);
	RegisterFields_Values_g(context);
	RegisterFields_Values_h(context);
	RegisterFields_Values_l_2(context);
	RegisterFields_Values_m(context);
	RegisterFields_Values_p(context);
	RegisterFields_Values_s(context);
	RegisterFields_Values_t(context);
	RegisterFields_Values_v(context);
}
