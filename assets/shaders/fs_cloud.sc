$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0); // Data\Textures\smoke.raw
SAMPLER2D(s_alpha, 1);   // smokea.raw

uniform vec4 u_cloudSpecular; // rgb: the object's specular 0..1 (the land cells' colour plus the haze colour;
                              // zero for the clouds and the shrinking mists)

// The mist material: render mode 6, colour = texture x diffuse, alpha = texture alpha x
// diffuse alpha, SRCALPHA / INVSRCALPHA, two-sided
void main()
{
	// the base level only, like the original: lower mips of the atlas bleed the neighbour cells into a hard disc
	vec4 texel = vec4(texture2DLod(s_diffuse, v_texcoord0.xy, 0.0f).rgb, texture2DLod(s_alpha, v_texcoord0.xy, 0.0f).r);
	// D3DRS_SPECULARENABLE: the specular is added after the texture stages and does not touch the alpha
	gl_FragColor = vec4(texel.rgb * v_color0.rgb + u_cloudSpecular.rgb, texel.a * v_color0.a);
}
