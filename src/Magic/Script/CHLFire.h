/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The CHL fire natives, called from CHLApi.cpp. They pop and push the VM stack themselves (the handlers' order).

namespace openblack::magic::script
{
/// 170 IS_ON_FIRE: the thing's IsOnFire
void IsOnFire();
/// 171 IS_FIRE_NEAR: FindNearForScript with a predicate (on fire and within the radius of its position, a worship
/// site's totem)
void IsFireNear();
/// 174 SET_TEMPERATURE: the object's temperature, with no source
void SetTemperature();
/// 175 SET_ON_FIRE: SetOnFire(speed), or back to the ambient temperature
void SetOnFire();
/// 321 SET_HURT_BY_FIRE: not hurt by fire = !enable
void SetHurtByFire();
/// 426 SET_SET_ON_FIRE: cannot be set on fire = !enable
void SetSetOnFire();
} // namespace openblack::magic::script
