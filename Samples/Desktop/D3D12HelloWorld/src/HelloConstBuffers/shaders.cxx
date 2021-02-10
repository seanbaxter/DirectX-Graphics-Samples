template<int location, typename type_t>
[[using spirv: in, location(location)]]
type_t shader_in;

template<int location, typename type_t>
[[using spirv: out, location(location)]]
type_t shader_out;

[[using spirv: uniform, binding(0)]]
vec4 offset;

extern "C" [[spirv::vert]]
void vert() {
  glvert_Output.Position = offset + shader_in<0, vec4>;
  shader_out<0, vec4> = shader_in<1, vec4>;
}

extern "C" [[spirv::frag]]
void frag() {
  shader_out<0, vec4> = shader_in<0, vec4>;
}