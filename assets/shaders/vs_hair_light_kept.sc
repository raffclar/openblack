$input a_position, a_texcoord0
$output v_color0

#include <bgfx_shader.sh>

#include "haze.sh"

uniform vec4 u_islandExtent;
#include "land_light.sh"
#include "hair_light.sh"

// The view of the camera that drew the hair
uniform mat4 u_hairView;

// Keeps the colours of a hair for a later frame, a texel for each colour: the light where the texture coordinate's x is
// 0, the added colour where it is 1. The colours are 0 to 255, thus the texels keep them exactly.
void main()
{
	vec3 origin = u_hairOrigin.xyz;
	vec3 light;
	vec3 added;
	HairLightAt(origin, mul(u_hairView, vec4(origin, 1.0f)).z, light, added);
	v_color0 = vec4((a_texcoord0.x < 0.5f ? light : added) / 255.0f, 1.0f);
	gl_Position = mul(u_viewProj, vec4(a_position.xy, 0.0f, 1.0f));
}
