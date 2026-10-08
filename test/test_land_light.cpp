/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// LandLightTable::Build against tables generated from the original's rules and its haze rules, with the overcast cap
// and the lightning flash, on a synthetic palette.raw. Build takes the original's sky type T (sky_type); the generated
// cases are in openblack's old convention S = 2 - T. The palette comes as a resource (LandLightPalette), and the
// game's copy of the last table is kept by the render frame system.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/DayNightClock.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/SkyType.h"
#include "ECS/Systems/Implementations/RenderFrameSystem.h"
#include "ECS/WaterRings.h"
#include "Locator.h"

using namespace openblack;

namespace
{
struct Haze
{
	float nearDistance;
	float farDistance;
	float k;
	std::array<float, 3> colour;
};

// generated from the original's rules
// case 0: sky type 2.00, alignment 0.00, overcast 0.80, flash 128; base 7cb22872
constexpr Haze k_Haze0 {18.57584f, 398.73457f, 156.0f, {155.00000f, 143.60000f, 149.70000f}};
constexpr std::array<uint32_t, 256> k_Table0 = {
    0xFFFFACDBu, 0xFFFFA9D8u, 0xFFFFA7D5u, 0xFFFFA4D3u, 0xFFFFA1D0u, 0xFFFE9ECDu, 0xFFF79BCAu, 0xFFF099C8u, 0xFFF198C8u,
    0xFFF298C7u, 0xFFF298C7u, 0xFFF398C7u, 0xFFF398C7u, 0xFFF498C7u, 0xFFF497C6u, 0xFFF597C6u, 0xFFF597C6u, 0xFFF697C6u,
    0xFFF697C6u, 0xFFF797C6u, 0xFFF896C5u, 0xFFF896C5u, 0xFFF996C5u, 0xFFF996C5u, 0xFFFA96C5u, 0xFFFA96C5u, 0xFFFB96C4u,
    0xFFFB95C4u, 0xFFFC95C4u, 0xFFFC95C4u, 0xFFFD95C4u, 0xFFFE95C4u, 0xFFFE95C3u, 0xFFFF94C3u, 0xFFFF94C3u, 0xFFFF94C3u,
    0xFFFF94C3u, 0xFFFF94C3u, 0xFFFF94C2u, 0xFFFF94C2u, 0xFFFF93C2u, 0xFFFF93C2u, 0xFFFF93C2u, 0xFFFF93C2u, 0xFFFF93C1u,
    0xFFFF93C1u, 0xFFFF92C1u, 0xFFFF92C1u, 0xFFFFA9D7u, 0xFFFFA8D7u, 0xFFFFA8D7u, 0xFFFFA8D7u, 0xFFFFA8D7u, 0xFFFFA8D7u,
    0xFFFFA8D7u, 0xFFFFA8D7u, 0xFFFFA8D7u, 0xFFFFA8D7u, 0xFFFFA8D6u, 0xFFFFA8D6u, 0xFFFFA8D6u, 0xFFFFA8D6u, 0xFFFFA7D6u,
    0xFFFFA7D6u, 0xFFFFA7D6u, 0xFFFFA7D6u, 0xFFFFA7D6u, 0xFFFFA7D6u, 0xFFFFA7D6u, 0xFFFFA7D6u, 0xFFFFA7D6u, 0xFFFFA7D5u,
    0xFFFFA7D5u, 0xFFFFA7D5u, 0xFFFFA7D5u, 0xFFFFA6D5u, 0xFFFFA6D5u, 0xFFFFA6D5u, 0xFFFFA6D5u, 0xFFFFA6D5u, 0xFFFFA6D5u,
    0xFFFFA6D5u, 0xFFFFA6D5u, 0xFFFFA6D5u, 0xFFFFA6D5u, 0xFFFFA6D4u, 0xFFFFA6D4u, 0xFFFFA6D4u, 0xFFFFA5D4u, 0xFFFFA5D4u,
    0xFFFFA5D4u, 0xFFFFA5D4u, 0xFFFFA5D4u, 0xFFFFA5D4u, 0xFFFFA5D4u, 0xFFFFA5D4u, 0xFFFFA5D4u, 0xFFFFA5D4u, 0xFFFFA5D3u,
    0xFFFFA5D3u, 0xFFFFA5D3u, 0xFFFFA4D3u, 0xFFFFA4D3u, 0xFFFFA4D3u, 0xFFFFA4D3u, 0xFFFFA4D3u, 0xFFFFA4D3u, 0xFFFFA4D3u,
    0xFFFFA4D3u, 0xFFFFA4D3u, 0xFFFFA4D3u, 0xFFFFA4D2u, 0xFFFFA4D2u, 0xFFFFA4D2u, 0xFFFFA3D2u, 0xFFFFA3D2u, 0xFFFFA3D2u,
    0xFFFFA3D2u, 0xFFFFA3D2u, 0xFFFFA3D2u, 0xFFFFA3D2u, 0xFFFFA3D2u, 0xFFFFA3D2u, 0xFFFFA3D2u, 0xFFFFA3D2u, 0xFFFFA3D1u,
    0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D1u,
    0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D1u, 0xFFFFA2D0u, 0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1D0u,
    0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1D0u, 0xFFFFA1CFu, 0xFFFFA0CFu,
    0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFFA0CFu,
    0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFFA0CFu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu,
    0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9FCEu, 0xFFFF9ECDu, 0xFFFF9ECDu,
    0xFFFF9ECDu, 0xFFFF9ECDu, 0xFFFE9ECDu, 0xFFFE9ECDu, 0xFFFE9ECDu, 0xFFFE9ECDu, 0xFFFE9ECDu, 0xFFFD9ECDu, 0xFFFD9ECDu,
    0xFFFD9ECDu, 0xFFFD9ECDu, 0xFFFD9DCCu, 0xFFFC9DCCu, 0xFFFC9DCCu, 0xFFFC9DCCu, 0xFFFC9DCCu, 0xFFFC9DCCu, 0xFFFB9DCCu,
    0xFFFB9DCCu, 0xFFFB9DCCu, 0xFFFB9DCCu, 0xFFFB9DCCu, 0xFFFB9DCCu, 0xFFFA9DCCu, 0xFFFA9CCCu, 0xFFFA9CCBu, 0xFFFA9CCBu,
    0xFFFA9CCBu, 0xFFF99CCBu, 0xFFF99CCBu, 0xFFF99CCBu, 0xFFF99CCBu, 0xFFF99CCBu, 0xFFF89CCBu, 0xFFF89CCBu, 0xFFF89CCBu,
    0xFFF89CCBu, 0xFFF89BCBu, 0xFFF89BCAu, 0xFFF79BCAu, 0xFFF79BCAu, 0xFFF79BCAu, 0xFFF79BCAu, 0xFFF79BCAu, 0xFFF69BCAu,
    0xFFF69BCAu, 0xFFF69BCAu, 0xFFF69BCAu, 0xFFF69BCAu, 0xFFF59BCAu, 0xFFF59ACAu, 0xFFF59AC9u, 0xFFF59AC9u, 0xFFF59AC9u,
    0xFFF49AC9u, 0xFFF49AC9u, 0xFFF49AC9u, 0xFFF49AC9u, 0xFFF49AC9u, 0xFFF49AC9u, 0xFFF39AC9u, 0xFFF39AC9u, 0xFFF39AC9u,
    0xFFF399C9u, 0xFFF399C9u, 0xFFF299C8u, 0xFFF299C8u, 0xFFF299C8u, 0xFFF299C8u, 0xFFF299C8u, 0xFFF199C8u, 0xFFF199C8u,
    0xFFF199C8u, 0xFFF199C8u, 0xFFF199C8u, 0xFFF099C8u,
};
// case 1: sky type 1.25, alignment 0.50, overcast 0.00, flash 0; base 2f7c658d
constexpr Haze k_Haze1 {148.83721f, 840.87667f, 126.0f, {41.00000f, 33.00000f, 47.00000f}};
constexpr std::array<uint32_t, 256> k_Table1 = {
    0xDD8E48A5u, 0xDD8F4BA6u, 0xDD904EA7u, 0xDD9151A8u, 0xDD9254A8u, 0xDD9358A9u, 0xDD935BAAu, 0xDD945EABu, 0xDD9561ABu,
    0xDD9664ACu, 0xDD9767ADu, 0xDD986AAEu, 0xDD996EAFu, 0xDD9971AFu, 0xDD9A74B0u, 0xDD9B77B1u, 0xDD9C7AB2u, 0xDD9D7DB3u,
    0x2F9E80B3u, 0x2FA182B1u, 0x2FA484AFu, 0x2FA786ADu, 0x2FAA88AAu, 0x2FAD89A8u, 0x2FB18BA6u, 0x2FB48DA4u, 0x2FB78FA2u,
    0x2FBA91A0u, 0x2FBD939Du, 0x2FC0949Bu, 0x2FC49699u, 0x2FC79897u, 0x2FCA9A94u, 0x2FCE9C92u, 0x2FD19E90u, 0x2FD49F8Eu,
    0x2FD7A18Bu, 0x2FDAA389u, 0x2FDDA587u, 0x2FE1A785u, 0x2FE4A883u, 0x2FE7AA80u, 0x2FEAAC7Eu, 0x2FEDAE7Cu, 0x2FF0B07Au,
    0x2FF4B277u, 0x2FF7B375u, 0x2FFAB573u, 0xDD9153A8u, 0xDD9153A8u, 0xDD9153A8u, 0xDD9153A8u, 0xDD9154A8u, 0xDD9154A8u,
    0xDD9254A8u, 0xDD9254A8u, 0xDD9254A8u, 0xDD9255A8u, 0xDD9255A8u, 0xDD9255A8u, 0xDD9255A9u, 0xDD9256A9u, 0xDD9256A9u,
    0xDD9256A9u, 0xDD9256A9u, 0xDD9256A9u, 0xDD9257A9u, 0xDD9257A9u, 0xDD9257A9u, 0xDD9257A9u, 0xDD9358A9u, 0xDD9358A9u,
    0xDD9358A9u, 0xDD9358A9u, 0xDD9358A9u, 0xDD9359A9u, 0xDD9359A9u, 0xDD9359A9u, 0xDD9359AAu, 0xDD935AAAu, 0xDD935AAAu,
    0xDD935AAAu, 0xDD935AAAu, 0xDD935AAAu, 0xDD935BAAu, 0xDD935BAAu, 0xDD935BAAu, 0xDD945BAAu, 0xDD945CAAu, 0xDD945CAAu,
    0xDD945CAAu, 0xDD945CAAu, 0xDD945CAAu, 0xDD945DAAu, 0xDD945DAAu, 0xDD945DAAu, 0xDD945DABu, 0xDD945EABu, 0xDD945EABu,
    0xDD945EABu, 0xDD945EABu, 0xDD945EABu, 0xDD945FABu, 0xDD945FABu, 0xDD955FABu, 0xDD955FABu, 0xDD955FABu, 0xDD9560ABu,
    0xDD9560ABu, 0xDD9560ABu, 0xDD9560ABu, 0xDD9561ABu, 0xDD9561ABu, 0xDD9561ABu, 0xDD9561ACu, 0xDD9561ACu, 0xDD9562ACu,
    0xDD9562ACu, 0xDD9562ACu, 0xDD9562ACu, 0xDD9663ACu, 0xDD9663ACu, 0xDD9663ACu, 0xDD9663ACu, 0xDD9663ACu, 0xDD9664ACu,
    0xDD9664ACu, 0xDD9664ACu, 0xDD9664ACu, 0xDD9665ACu, 0xDD9665ACu, 0xDD9665ACu, 0xDD9665ADu, 0xDD9665ADu, 0xDD9666ADu,
    0xDD9666ADu, 0xDD9666ADu, 0xDD9766ADu, 0xDD9767ADu, 0xDD9767ADu, 0xDD9767ADu, 0xDD9767ADu, 0xDD9767ADu, 0xDD9768ADu,
    0xDD9768ADu, 0xDD9768ADu, 0xDD9768ADu, 0xDD9769ADu, 0xDD9769ADu, 0xDD9769ADu, 0xDD9769AEu, 0xDD9769AEu, 0xDD976AAEu,
    0xDD976AAEu, 0xDD986AAEu, 0xDD986AAEu, 0xDD986AAEu, 0xDD986BAEu, 0xDD986BAEu, 0xDD986BAEu, 0xDD986BAEu, 0xDD986CAEu,
    0xDD986CAEu, 0xDD986CAEu, 0xDD986CAEu, 0xDD986CAEu, 0xDD986DAEu, 0xDD986DAEu, 0xDD986DAEu, 0xDD986DAFu, 0xDD996EAFu,
    0xDD996EAFu, 0xDD996EAFu, 0xDD996EAFu, 0xDD996EAFu, 0xDD996FAFu, 0xDD996FAFu, 0xDD996FAFu, 0xDD996FAFu, 0xDD9970AFu,
    0xDD9970AFu, 0xDD9970AFu, 0xDD9970AFu, 0xDD9970AFu, 0xDD9971AFu, 0xDD9971AFu, 0xDD9971AFu, 0xDD9A71B0u, 0xDD9A72B0u,
    0xDD9A72B0u, 0xDD9A72B0u, 0xDD9A72B0u, 0xDD9A72B0u, 0xDD9A73B0u, 0xDD9A73B0u, 0xDD9A73B0u, 0xDD9A73B0u, 0xDD9A74B0u,
    0xDD9A74B0u, 0xDD9A74B0u, 0xDD9A74B0u, 0xDD9A74B0u, 0xDD9A75B0u, 0xDD9A75B0u, 0xDD9B75B0u, 0xDD9B75B1u, 0xDD9B75B1u,
    0xDD9B76B1u, 0xDD9B76B1u, 0xDD9B76B1u, 0xDD9B76B1u, 0xDD9B77B1u, 0xDD9B77B1u, 0xDD9B77B1u, 0xDD9B77B1u, 0xDD9B77B1u,
    0xDD9B78B1u, 0xDD9B78B1u, 0xDD9B78B1u, 0xDD9B78B1u, 0xDD9C79B1u, 0xDD9C79B1u, 0xDD9C79B1u, 0xDD9C79B2u, 0xDD9C79B2u,
    0xDD9C7AB2u, 0xDD9C7AB2u, 0xDD9C7AB2u, 0xDD9C7AB2u, 0xDD9C7BB2u, 0xDD9C7BB2u, 0xDD9C7BB2u, 0xDD9C7BB2u, 0xDD9C7BB2u,
    0xDD9C7CB2u, 0xDD9C7CB2u, 0xDD9C7CB2u, 0xDD9D7CB2u, 0xDD9D7DB2u, 0xDD9D7DB2u, 0xDD9D7DB2u, 0xDD9D7DB3u, 0xDD9D7DB3u,
    0xDD9D7EB3u, 0xDD9D7EB3u, 0xDD9D7EB3u, 0xDD9D7EB3u, 0xDD9D7FB3u, 0xDD9D7FB3u, 0xDD9D7FB3u, 0xDD9D7FB3u, 0xDD9D7FB3u,
    0xDD9D80B3u, 0xDD9D80B3u, 0xDD9E80B3u, 0xDD9E80B3u,
};
// case 2: sky type 0.25, alignment -0.75, overcast 1.20, flash 40; base b08b5e77
constexpr Haze k_Haze2 {14.99999f, 350.00035f, 80.0f, {81.18750f, 76.12500f, 78.65625f}};
constexpr std::array<uint32_t, 256> k_Table2 = {
    0xFFC5FF4Fu, 0xFFC4FF54u, 0xFFC4FF59u, 0xFFC3FC5Eu, 0xFFC3F463u, 0xFFC3EC68u, 0xFFC2E46Eu, 0xFFC2DC73u, 0xFFC1D378u,
    0xFFC1CC7Du, 0xFFC0C482u, 0xFFBFBC88u, 0xFFBFB48Du, 0xFFBEAC93u, 0xFFBEA498u, 0xFFBE9C9Du, 0xFFBD94A2u, 0xFFBD8CA7u,
    0xFFBB8FA9u, 0xFFBA92ACu, 0xFFB995AEu, 0xFFB898B1u, 0xFFB79BB3u, 0xFFB69DB5u, 0xFFB4A1B8u, 0xFFB3A3BAu, 0xFFB3A7BDu,
    0xFFB1A9BFu, 0xFFB0ADC1u, 0xFFAEAFC3u, 0xFFAEB3C6u, 0xFFADB5C9u, 0xFFABB8CBu, 0xFFAABBCEu, 0xFFA9BED0u, 0xFFA8C1D2u,
    0xFFA7C4D4u, 0xFFA6C7D7u, 0xFFA4CAD9u, 0xFFA3CDDCu, 0xFFA3D0DEu, 0xFFA1D3E1u, 0xFFA0D6E3u, 0xFF9ED9E5u, 0xFF9DDCE8u,
    0xFF9DDFEAu, 0xFF9BE2EDu, 0xFF9AE4EEu, 0xFFC3FA5Fu, 0xFFC3F960u, 0xFFC3F960u, 0xFFC3F961u, 0xFFC3F861u, 0xFFC3F861u,
    0xFFC3F762u, 0xFFC3F662u, 0xFFC3F662u, 0xFFC3F562u, 0xFFC3F462u, 0xFFC3F463u, 0xFFC3F463u, 0xFFC3F464u, 0xFFC3F364u,
    0xFFC3F265u, 0xFFC3F265u, 0xFFC3F165u, 0xFFC3F066u, 0xFFC3F066u, 0xFFC3EF67u, 0xFFC3EF67u, 0xFFC3EE67u, 0xFFC3EE67u,
    0xFFC3EE67u, 0xFFC3ED68u, 0xFFC3EC68u, 0xFFC3EC68u, 0xFFC3EB69u, 0xFFC3EA69u, 0xFFC3EA6Au, 0xFFC3E96Au, 0xFFC3E96Au,
    0xFFC3E96Bu, 0xFFC3E86Bu, 0xFFC3E86Cu, 0xFFC3E76Cu, 0xFFC3E66Du, 0xFFC2E66Du, 0xFFC2E56Du, 0xFFC2E56Du, 0xFFC2E46Du,
    0xFFC2E46Eu, 0xFFC2E46Eu, 0xFFC2E36Eu, 0xFFC2E26Fu, 0xFFC2E26Fu, 0xFFC2E170u, 0xFFC2E170u, 0xFFC2E070u, 0xFFC2DF71u,
    0xFFC2DF71u, 0xFFC2DE72u, 0xFFC2DE72u, 0xFFC2DE72u, 0xFFC2DD72u, 0xFFC2DC72u, 0xFFC2DC73u, 0xFFC2DB73u, 0xFFC2DB74u,
    0xFFC2DA74u, 0xFFC2D974u, 0xFFC2D975u, 0xFFC1D975u, 0xFFC1D876u, 0xFFC1D876u, 0xFFC1D776u, 0xFFC1D777u, 0xFFC1D677u,
    0xFFC1D578u, 0xFFC1D578u, 0xFFC1D478u, 0xFFC1D378u, 0xFFC1D378u, 0xFFC1D379u, 0xFFC1D379u, 0xFFC1D27Au, 0xFFC1D17Au,
    0xFFC1D17Au, 0xFFC1D07Bu, 0xFFC1CF7Bu, 0xFFC1CF7Cu, 0xFFC1CE7Cu, 0xFFC1CE7Cu, 0xFFC1CE7Du, 0xFFC1CD7Du, 0xFFC1CD7Du,
    0xFFC1CC7Du, 0xFFC0CB7Du, 0xFFC0CB7Eu, 0xFFC0CA7Eu, 0xFFC0C97Fu, 0xFFC0C97Fu, 0xFFC0C97Fu, 0xFFC0C980u, 0xFFC0C880u,
    0xFFC0C781u, 0xFFC0C781u, 0xFFC0C682u, 0xFFC0C582u, 0xFFC0C582u, 0xFFC0C482u, 0xFFC0C482u, 0xFFC0C383u, 0xFFC0C383u,
    0xFFC0C383u, 0xFFC0C284u, 0xFFC0C184u, 0xFFC0C185u, 0xFFC0C085u, 0xFFC0BF85u, 0xFFC0BF86u, 0xFFC0BE86u, 0xFFBFBE87u,
    0xFFBFBE87u, 0xFFBFBD88u, 0xFFBFBD88u, 0xFFBFBC88u, 0xFFBFBB88u, 0xFFBFBB88u, 0xFFBFBA89u, 0xFFBFBA89u, 0xFFBFB989u,
    0xFFBFB88Au, 0xFFBFB88Au, 0xFFBFB88Bu, 0xFFBFB78Bu, 0xFFBFB78Bu, 0xFFBFB68Cu, 0xFFBFB58Cu, 0xFFBFB58Du, 0xFFBFB48Du,
    0xFFBFB48Du, 0xFFBFB38Du, 0xFFBFB38Du, 0xFFBFB38Eu, 0xFFBFB28Eu, 0xFFBFB18Fu, 0xFFBEB18Fu, 0xFFBEB08Fu, 0xFFBEB090u,
    0xFFBEAF90u, 0xFFBEAE91u, 0xFFBEAE91u, 0xFFBEAE91u, 0xFFBEAD92u, 0xFFBEAD92u, 0xFFBEAC93u, 0xFFBEAC93u, 0xFFBEAB93u,
    0xFFBEAA93u, 0xFFBEAA93u, 0xFFBEA994u, 0xFFBEA894u, 0xFFBEA894u, 0xFFBEA895u, 0xFFBEA795u, 0xFFBEA796u, 0xFFBEA696u,
    0xFFBEA697u, 0xFFBEA597u, 0xFFBEA497u, 0xFFBEA498u, 0xFFBEA398u, 0xFFBEA398u, 0xFFBEA398u, 0xFFBEA298u, 0xFFBEA299u,
    0xFFBEA199u, 0xFFBEA09Au, 0xFFBEA09Au, 0xFFBE9F9Au, 0xFFBE9E9Bu, 0xFFBE9E9Bu, 0xFFBE9D9Cu, 0xFFBE9D9Cu, 0xFFBE9D9Du,
    0xFFBE9C9Du, 0xFFBE9C9Du, 0xFFBE9B9Du, 0xFFBE9A9Du, 0xFFBE9A9Eu, 0xFFBE999Eu, 0xFFBE989Eu, 0xFFBE989Fu, 0xFFBE989Fu,
    0xFFBE98A0u, 0xFFBE97A0u, 0xFFBD96A0u, 0xFFBD96A1u, 0xFFBD95A1u, 0xFFBD94A2u, 0xFFBD94A2u, 0xFFBD93A2u, 0xFFBD93A3u,
    0xFFBD93A3u, 0xFFBD92A3u, 0xFFBD92A3u, 0xFFBD91A4u, 0xFFBD90A4u, 0xFFBD90A4u, 0xFFBD8FA5u, 0xFFBD8FA5u, 0xFFBD8EA6u,
    0xFFBD8DA6u, 0xFFBD8DA6u, 0xFFBD8DA7u, 0xFFBD8CA7u,
};

std::vector<uint8_t> Palette()
{
	std::vector<uint8_t> palette(4096);
	for (size_t i = 0; i < palette.size(); ++i)
	{
		palette[i] = static_cast<uint8_t>((i * 73 + (i >> 7) * 29 + 11) & 255);
	}
	return palette;
}

/// The synthetic palette.raw as the palette resource
LandLightPalette PaletteResource()
{
	return LandLightPalette(Palette());
}

/// What the old table's Load made of palette.raw: each texel's red, green, blue and alpha bytes as 0xAARRGGBB
std::vector<uint32_t> OldLoad(const std::vector<uint8_t>& raw)
{
	std::vector<uint32_t> palette(32 * 32);
	for (size_t i = 0; i < palette.size(); ++i)
	{
		palette[i] = static_cast<uint32_t>(raw[i * 4 + 0]) << 16 | static_cast<uint32_t>(raw[i * 4 + 1]) << 8 |
		             static_cast<uint32_t>(raw[i * 4 + 2]) | static_cast<uint32_t>(raw[i * 4 + 3]) << 24;
	}
	return palette;
}

void Check(float skyType, float alignment, float overcast, uint8_t flash, const std::array<uint32_t, 256>& expected,
           const Haze& haze)
{
	LandLightTable table;
	table.Build(PaletteResource(), 2.0f - skyType, alignment, overcast, flash);
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(table.GetRaw(i), expected[i]) << "entry " << i;
	}
	const auto& h = table.GetHaze();
	EXPECT_NEAR(h.nearDistance, haze.nearDistance, haze.nearDistance * 1e-4f);
	EXPECT_NEAR(h.farDistance, haze.farDistance, haze.farDistance * 1e-4f);
	EXPECT_FLOAT_EQ(h.k, haze.k);
	for (int c = 0; c < 3; ++c)
	{
		EXPECT_NEAR(h.colour[c], haze.colour[c], 1e-3f) << "haze channel " << c;
	}
}

/// LandLightTable::Build before the sky_type hook-up, in openblack's old convention S = 2 - T, fed by
/// the forwarder Sky::GetCurrentSkyType = 2 - sky_type::Frame(): kept here as the reference of what changed
struct OldTable
{
	std::array<uint32_t, 256> table {};
	float nearDistance {0.0f};
	float farDistance {0.0f};
	float k {0.0f};
	std::array<float, 3> colour {};
};

uint32_t OldLerp(uint32_t a, uint32_t b, uint32_t t)
{
	const uint32_t r = (((((b & 0xFF0000u) - (a & 0xFF0000u)) * t) >> 8) + (a & 0xFFFF0000u)) & 0xFF0000u;
	const uint32_t g = (((((b & 0xFF00u) - (a & 0xFF00u)) * t) >> 8) + (a & 0xFFFFFF00u)) & 0xFF00u;
	const uint32_t bl = (((((b & 0xFFu) - (a & 0xFFu)) * t) >> 8) + a) & 0xFFu;
	return r | g | bl | (b & 0xFF000000u);
}

uint32_t OldRamp(uint32_t a, uint32_t b, uint32_t t)
{
	uint32_t result = a & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const uint32_t value = (((a >> shift) & 0xFFu) * (255 - t) + ((b >> shift) & 0xFFu) * t) / 200;
		result |= std::min(255u, value) << shift;
	}
	return result;
}

OldTable OldBuild(const std::vector<uint8_t>& raw, float skyType, float alignment, float overcast, uint8_t flash)
{
	const auto palette = OldLoad(raw);
	OldTable out;
	const float timeColumn = std::clamp(skyType, 0.0f, 2.0f) * 6.0f * 2.5f;
	const float x = std::clamp(1.0f - alignment, 0.0f, 2.0f);
	const float alignColumn = x * 15.0f;
	std::array<uint32_t, 8> colours {};
	for (size_t row = 0; row < colours.size(); ++row)
	{
		const float column = row < 3 ? timeColumn : alignColumn;
		const auto index = std::min(static_cast<size_t>(column), size_t {30});
		const auto t = static_cast<uint32_t>((column - static_cast<float>(index)) * 256.0f);
		colours[row] = OldLerp(palette[row * 32 + index], palette[row * 32 + index + 1], t);
	}
	const auto k = static_cast<int>(x * 255.0f);
	uint32_t base = x < 1.0f ? OldLerp(colours[0], colours[1], static_cast<uint32_t>(k))
	                         : OldLerp(colours[1], colours[2], static_cast<uint32_t>(k - 256));
	const auto limit = static_cast<int32_t>(255.0f - overcast * 96.0f);
	uint32_t capped = base & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const auto channel = static_cast<int32_t>((base >> shift) & 0xFFu);
		capped |= static_cast<uint32_t>(std::clamp(std::min(channel, limit), 0, 255)) << shift;
	}
	base = capped;
	const uint32_t r = (base >> 16) & 0xFFu;
	const uint32_t g = (base >> 8) & 0xFFu;
	const uint32_t b = base & 0xFFu;
	out.k = static_cast<float>(std::min(255u, (r + 4 * g + 3 * b) / 8 + 8));
	std::array<float, 3> colour = {static_cast<float>(r / 3), static_cast<float>(g / 3), static_cast<float>(b / 3)};
	const float v = 1.0f - std::abs(std::clamp(skyType, 0.0f, 2.0f) - 1.0f);
	float nearInverse = 0.0025f + 0.0075f * v * v;
	float farInverse = 0.00111111f + 0.000138889f * v * v;
	if (overcast > 0.0f)
	{
		const float w = std::min(overcast, 1.0f);
		const std::array<float, 3> storm = {static_cast<float>((r >> 3) + 32), static_cast<float>((g >> 3) + 32),
		                                    static_cast<float>((b >> 3) + 32)};
		for (size_t c = 0; c < 3; ++c)
		{
			colour.at(c) += (storm.at(c) - colour.at(c)) * w;
		}
		out.k = out.k + static_cast<float>(static_cast<int32_t>((48.0f - out.k) * w));
		nearInverse += (0.0666667f - nearInverse) * w;
		farInverse += (0.00285714f - farInverse) * w;
	}
	if (flash != 0)
	{
		const float f = static_cast<float>(flash);
		for (auto& c : colour)
		{
			c += (255.0f - c) * f * 0.00390625f;
		}
		const auto kk = static_cast<int32_t>(out.k);
		out.k = static_cast<float>(kk + ((255 - kk) * static_cast<int32_t>(flash)) / 256);
	}
	out.nearDistance = 1.0f / nearInverse;
	out.farDistance = 1.0f / farInverse;
	out.colour = colour;
	const uint32_t n = (g * 48) >> 8;
	for (uint32_t i = 0; i < n; ++i)
	{
		out.table.at(i) = OldRamp(colours[3], base, (i * 256) / n);
	}
	for (uint32_t i = n; i < 48; ++i)
	{
		out.table.at(i) = OldRamp(base, colours[6], ((i - n) * 256) / (48 - n));
	}
	for (uint32_t i = 48; i < 256; ++i)
	{
		out.table.at(i) = OldRamp(colours[3], base, i);
	}
	if (flash != 0)
	{
		for (auto& c : out.table)
		{
			c = OldLerp(c, 0xFFFFFFFFu, flash) | 0xFF000000u;
		}
	}
	return out;
}

/// The haze distances of the original's Build with its exact floats, on the original's T
std::array<float, 2> OriginalHazeDistances(float skyType, float overcast)
{
	const float v = skyType > 1.0f ? 2.0f - skyType : skyType;
	const float v2 = v * v;
	float nearInverse = std::bit_cast<float>(0x3B23D70Au); // 0.0025, 1 / 400
	float farInverse = std::bit_cast<float>(0x3A91A2B4u);  // about 1 / 900
	if (v2 > 0.0f)
	{
		nearInverse = v2 * std::bit_cast<float>(0x3BF5C28Fu) + nearInverse; // 0.0075
		farInverse = v2 * std::bit_cast<float>(0x3911A2B0u) + farInverse;   // about 1 / 7200
	}
	if (overcast > 0.0f)
	{
		const float w = std::min(overcast, 1.0f);
		nearInverse += (std::bit_cast<float>(0x3D888889u) - nearInverse) * w; // about 1 / 15
		farInverse += (std::bit_cast<float>(0x3B3B3EE7u) - farInverse) * w;   // about 1 / 350
	}
	return {1.0f / nearInverse, 1.0f / farInverse};
}
} // namespace

TEST(LandLightTable, NoonStormWithFlash)
{
	Check(2.0f, 0.0f, 0.8f, 128, k_Table0, k_Haze0);
}

TEST(LandLightTable, DuskClear)
{
	Check(1.25f, 0.5f, 0.0f, 0, k_Table1, k_Haze1);
}

TEST(LandLightTable, NightOverOneOvercastSmallFlash)
{
	Check(0.25f, -0.75f, 1.2f, 40, k_Table2, k_Haze2);
}

TEST(LandLightTable, FlashZeroLeavesTheTable)
{
	const auto palette = PaletteResource();
	LandLightTable a;
	LandLightTable b;
	a.Build(palette, 1.0f, 0.2f, 0.3f);
	b.Build(palette, 1.0f, 0.2f, 0.3f, 0);
	for (size_t i = 0; i < LandLightTable::k_Size; ++i)
	{
		EXPECT_EQ(a.GetRaw(i), b.GetRaw(i));
	}
}

TEST(LandLightTable, RingColourFixedAtCreation)
{
	// a ring of the landscape light (the hand's splash, table[255]) keeps the colour of the frame it was made in. The
	// rings read the game's copy of the table, which the renderer hands to the render frame system after each build
	const auto palette = PaletteResource();
	auto& frame = Locator::renderFrameSystem::value();
	LandLightTable table;
	table.Build(palette, 0.75f, 0.5f, 0.0f);
	frame.SetLandLightTable(table);
	const uint32_t dusk = table.GetRaw(255) & 0x00FFFFFFu;
	const ecs::WaterRing ring {.argb = 0xB0000000u, .seaLight = true};
	const auto before = ecs::GetWaterRings().size();
	ASSERT_TRUE(ecs::AddWaterRing(ring));
	ASSERT_EQ(ecs::GetWaterRings().size(), before + 1);
	table.Build(palette, 0.0f, 0.0f, 0.8f, 128);
	frame.SetLandLightTable(table);
	ASSERT_NE(table.GetRaw(255) & 0x00FFFFFFu, dusk);
	const auto& added = ecs::GetWaterRings().back();
	EXPECT_EQ(added.argb, 0xB0000000u | dusk);
	EXPECT_FALSE(added.seaLight);
}

TEST(LandLightTable, MatchesTheOldConventionEveryHour)
{
	// The default campaign cycle (1700 / 0.083 / 0.07): A..D = 0.786 / 1.206 / 1.626 / 2.046 visual hours
	DayNightClock clock;
	clock.SetCycle(DayNightClock::k_DefaultDuration, DayNightClock::k_DefaultNight, DayNightClock::k_DefaultChange);
	const auto palette = Palette();
	const LandLightPalette resource(palette);
	struct Weather
	{
		float alignment;
		float overcast;
		uint8_t flash;
	};
	const std::array<Weather, 4> weathers = {{{0.0f, 0.0f, 0}, {0.5f, 0.0f, 0}, {-0.75f, 1.2f, 40}, {0.3f, 0.4f, 0}}};
	int hazeDifferences = 0;
	for (int step = 0; step <= 24 * 64; ++step)
	{
		// every 1/64 visual hour, through sky_type::SampleFrame like DrawSky
		const float hour = static_cast<float>(step) / 64.0f;
		sky_type::SampleFrame(hour);
		const float t = sky_type::Frame();
		// the same time column bit for bit, (2 - T) * 6 * 2.5 either way (the old clamp and the fold never act)
		ASSERT_EQ(sky_type::LightColumn(t), std::clamp(2.0f - t, 0.0f, 2.0f) * 6.0f * 2.5f) << "hour " << hour;
		for (const auto& weather : weathers)
		{
			LandLightTable table;
			table.Build(resource, t, weather.alignment, weather.overcast, weather.flash);
			// what the forwarder 2 - Frame() fed the old Build
			const auto old = OldBuild(palette, 2.0f - t, weather.alignment, weather.overcast, weather.flash);
			for (size_t i = 0; i < LandLightTable::k_Size; ++i)
			{
				ASSERT_EQ(table.GetRaw(i), old.table.at(i)) << "hour " << hour << " entry " << i;
			}
			const auto& haze = table.GetHaze();
			EXPECT_EQ(haze.k, old.k) << "hour " << hour;
			for (int c = 0; c < 3; ++c)
			{
				EXPECT_EQ(haze.colour[c], old.colour.at(static_cast<size_t>(c))) << "hour " << hour;
			}
			// near / far: the original's floats and order exactly; the old ones only within their last bits
			const auto original = OriginalHazeDistances(t, weather.overcast);
			EXPECT_EQ(haze.nearDistance, original[0]) << "hour " << hour;
			EXPECT_EQ(haze.farDistance, original[1]) << "hour " << hour;
			EXPECT_NEAR(haze.nearDistance, old.nearDistance, old.nearDistance * 4e-6f) << "hour " << hour;
			EXPECT_NEAR(haze.farDistance, old.farDistance, old.farDistance * 4e-6f) << "hour " << hour;
			hazeDifferences += haze.nearDistance != old.nearDistance || haze.farDistance != old.farDistance ? 1 : 0;
		}
	}
	// the old constants (0.00111111 for 1 / 900, 0.0666667 for 1 / 15, ...) were off in their last bits
	EXPECT_GT(hazeDifferences, 0);
}

TEST(LandLightPalette, SameColoursAsTheOldLoad)
{
	const auto raw = Palette();
	const LandLightPalette palette(raw);
	const auto old = OldLoad(raw);
	for (size_t row = 0; row < LandLightPalette::k_Side; ++row)
	{
		for (size_t column = 0; column < LandLightPalette::k_Side; ++column)
		{
			ASSERT_EQ(palette.At(row, column), old.at(row * LandLightPalette::k_Side + column)) << row << ", " << column;
		}
	}
}

TEST(LandLightPalette, WrongSizeThrows)
{
	// the old Load refused it and left the land unlit; the resource is not made, with the same result
	EXPECT_THROW(LandLightPalette(std::vector<uint8_t>(4095)), std::runtime_error);
	EXPECT_THROW(LandLightPalette(std::vector<uint8_t> {}), std::runtime_error);
}

TEST(LandLightPalette, TheFilesBytesGiveTheSameTables)
{
	// the palette made from the file's bytes, as the resource loader makes it, builds the generated tables
	const auto raw = Palette();
	const LandLightPalette palette {std::span<const uint8_t>(raw)};
	const std::array<std::pair<const std::array<uint32_t, 256>*, std::array<float, 3>>, 3> cases = {{
	    {&k_Table0, {2.0f, 0.0f, 0.8f}},
	    {&k_Table1, {1.25f, 0.5f, 0.0f}},
	    {&k_Table2, {0.25f, -0.75f, 1.2f}},
	}};
	const std::array<uint8_t, 3> flashes = {128, 0, 40};
	for (size_t c = 0; c < cases.size(); ++c)
	{
		const auto& [expected, inputs] = cases.at(c);
		LandLightTable table;
		table.Build(palette, 2.0f - inputs[0], inputs[1], inputs[2], flashes.at(c));
		for (size_t i = 0; i < LandLightTable::k_Size; ++i)
		{
			ASSERT_EQ(table.GetRaw(i), expected->at(i)) << "case " << c << " entry " << i;
		}
	}
}

TEST(RenderFrameSystem, KeepsTheLastLandLightTable)
{
	ecs::systems::RenderFrameSystem frame;
	// before the first build every light is white, with a white land colour and the default haze
	for (size_t i = 0; i < LandLightTable::k_Size; ++i)
	{
		ASSERT_EQ(frame.GetLandLightTable().GetRaw(i), 0xFFFFFFFFu) << "entry " << i;
	}
	EXPECT_EQ(frame.GetLandLightTable().GetLandColour(), 0xFFFFFFFFu);
	EXPECT_EQ(frame.GetLandLightTable().GetHaze().nearDistance, 400.0f);
	EXPECT_EQ(frame.GetLandLightTable().GetHaze().farDistance, 900.0f);
	EXPECT_EQ(frame.GetLandLightTable().GetHaze().k, 256.0f);

	LandLightTable table;
	table.Build(PaletteResource(), 0.0f, 0.0f, 0.8f, 128);
	frame.SetLandLightTable(table);
	for (size_t i = 0; i < LandLightTable::k_Size; ++i)
	{
		ASSERT_EQ(frame.GetLandLightTable().GetRaw(i), k_Table0.at(i)) << "entry " << i;
	}
	EXPECT_EQ(frame.GetLandLightTable().GetLandColour(), table.GetLandColour());
	EXPECT_EQ(frame.GetLandLightTable().GetHaze().k, k_Haze0.k);
	// a later build of the table alone leaves the copy as it was
	table.Build(PaletteResource(), 1.25f, 0.5f, 0.0f);
	EXPECT_EQ(frame.GetLandLightTable().GetRaw(255), k_Table0.at(255));
}
