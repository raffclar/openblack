$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

// The hair texture's colours and its alpha
SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_alpha, 1);

// x: the alpha, 0 to 255, below which a texel of a strand is not drawn
uniform vec4 u_hairAlphaCutOff;

// A strand of hair in its vertices' colour, multiplied by the texture. The texels with an alpha under the cut-off are
// not drawn. The other texels write their depth and blend over the scene by the texture's alpha.
void main()
{
	float alpha = texture2D(s_alpha, v_texcoord0.xy).r * v_color0.a;
	if (floor(alpha * 255.0f + 0.5f) < u_hairAlphaCutOff.x)
	{
		discard;
	}
	vec3 colour = texture2D(s_diffuse, v_texcoord0.xy).rgb * v_color0.rgb;
	gl_FragColor = vec4(colour, alpha);
}
