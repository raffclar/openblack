$input v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>
#include "shadow.sh"

SAMPLER2D(s_dynamicShadow, 5);    // the shadow's texture: alpha n / 15, the fade baked in, CLAMP
uniform vec4 u_dynamicShadowBox;  // x0, z0, 1 / (x1 - x0), 1 / (z1 - z0): the shadow's box
uniform vec4 u_dynamicShadowCull; // x, y: the caster minus the light in x, z; z: the least k

// A projected shadow on an object: the object drawn again with ZFUNC EQUAL, the shadow
// texture projected straight down (shadow.sh), render mode 6 (black, SRCALPHA / INVSRCALPHA), vertex
// colour white. The texture's empty ring and CLAMP make it 0 outside the box. The code 0x400 (k below the least k) per
// fragment (aproximado: the original drops the triangles whose three vertices have it)
void main()
{
	if (ShadowKept(v_position.xz, u_dynamicShadowCull) < 0.5f)
	{
		discard;
	}
	vec2 shadowUv = ObjectShadowUv(v_position.xz, u_dynamicShadowBox);
	gl_FragColor = vec4(0.0f, 0.0f, 0.0f, texture2D(s_dynamicShadow, shadowUv).r);
}
