$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);

// x: the alpha test reference, from 0 to 255. A pixel with less alpha does not show. A reference of 0 lets all pixels
// through. The test takes a margin of 1/128 off the reference, because the float product of two alphas can be a very
// small amount lower than the exact product (see graphics::alpha_test::Shows).
uniform vec4 u_alphaTestRef;

void main()
{
	vec4 colour = texture2D(s_texture, v_texcoord0.xy) * v_color0;
	if (colour.a * 255.0f < u_alphaTestRef.x - 1.0f / 128.0f)
	{
		discard;
	}
	gl_FragColor = colour;
}
