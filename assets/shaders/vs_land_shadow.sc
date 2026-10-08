$input a_position, a_color3
$output v_texcoord0, v_shoreFade

#include <bgfx_shader.sh>
#include "land_position.sh"
#include "shadow.sh"

uniform vec4 u_blockPositionAndSize;
uniform vec4 u_shadowLight; // xyz: the light, w: t' (one per shadow)
uniform vec4 u_shadowBox;   // x0, z0, 1 / (x1 - x0), 1 / (z1 - z0): the shadow's box
uniform vec4 u_shadowCull;  // x, y: the caster minus the light in x, z; z: the least k

// One shadow over one land block, the block's own vertices (land_position.sh); per vertex the uv of the
// shadow box (shadow.sh), the diffuse 0 at altitude 1 or less (a_color3, as vs_terrain's
// v_shoreFade) and the code 0x400 in z
void main()
{
	vec3 world = LandWorldPosition(a_position.xyz, u_blockPositionAndSize.xy);
	v_texcoord0 = vec4(LandShadowUv(world.xz, u_shadowLight, u_shadowBox), ShadowKept(world.xz, u_shadowCull), 0.0f);
	v_shoreFade = a_color3;
	gl_Position = LandClipPosition(LandViewPosition(world));
}
