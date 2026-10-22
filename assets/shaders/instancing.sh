#ifndef INSTANCING_SH
#define INSTANCING_SH

// The transform of an instance, whose rows i_data0 to i_data3 the shader puts into a matrix by index. bgfx 1.166 has
// no such helper. This one does the same as the helper of the earlier bgfx in each shader language.

#if BGFX_SHADER_LANGUAGE_GLSL
vec3 instMul(vec3 _vec, mat3 _mtx) { return mul(_vec, _mtx); }
vec3 instMul(mat3 _mtx, vec3 _vec) { return mul(_mtx, _vec); }
vec4 instMul(vec4 _vec, mat4 _mtx) { return mul(_vec, _mtx); }
vec4 instMul(mat4 _mtx, vec4 _vec) { return mul(_mtx, _vec); }
#else
vec3 instMul(vec3 _vec, mat3 _mtx) { return mul(_mtx, _vec); }
vec3 instMul(mat3 _mtx, vec3 _vec) { return mul(_vec, _mtx); }
vec4 instMul(vec4 _vec, mat4 _mtx) { return mul(_mtx, _vec); }
vec4 instMul(mat4 _mtx, vec4 _vec) { return mul(_vec, _mtx); }
#endif // BGFX_SHADER_LANGUAGE_GLSL

#endif // INSTANCING_SH
