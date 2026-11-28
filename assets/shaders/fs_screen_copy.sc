$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_screenCopy, 0);
// x, y: the size of the window in pixels
uniform vec4 u_screenCopy;

// The frame that a picture drew aside, copied into the window as it is
void main()
{
	gl_FragColor = vec4(texture2DLod(s_screenCopy, gl_FragCoord.xy / u_screenCopy.xy, 0.0).rgb, 1.0);
}
