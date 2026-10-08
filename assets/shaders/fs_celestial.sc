$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_alpha, 1);    // the "<name>a.raw" alpha of the texture, when u_celestial.w > 0
uniform vec4 u_colour;    // the D3D vertex colour (diffuse rgb, alpha)
uniform vec4 u_celestial; // see vs_celestial

// COLOROP / ALPHAOP = MODULATE(TEXTURE, DIFFUSE); the blend (additive SRCALPHA / ONE for mode 13) is the render state
void main()
{
	vec4 texel = texture2D(s_diffuse, v_texcoord0.xy);
	if (u_celestial.w > 0.0f)
	{
		texel.a = texture2D(s_alpha, v_texcoord0.xy).r;
	}
	gl_FragColor = texel * u_colour;
}
