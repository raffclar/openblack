$input v_texcoord0

#include <bgfx_shader.sh>

#include "screen_glow.sh"

// The frame with the glow of the good sky added, copied into the window
void main()
{
	vec3 scene = texture2DLod(s_screenGlowScene, gl_FragCoord.xy / u_screenGlow.xy, 0.0).rgb;
	gl_FragColor = vec4(WithScreenGlow(scene, gl_FragCoord.xy), 1.0);
}
