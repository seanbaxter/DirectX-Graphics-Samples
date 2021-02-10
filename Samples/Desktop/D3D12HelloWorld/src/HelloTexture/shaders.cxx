template<int location, typename type_t>
[[using spirv: in, location(location)]]
type_t shader_in;

template<int location, typename type_t>
[[using spirv: out, location(location)]]
type_t shader_out;

extern "C" [[spirv::vert]]
void vert_main() {
  // Write out vec4 position.
  glvert_Output.Position = shader_in<0, vec4>;

  // Pass-through vec2 uv.
  shader_out<0, vec2> = shader_in<1, vec2>;
}

[[using spirv: uniform, binding(0)]] sampler   samp0;   // s0
[[using spirv: uniform, binding(0)]] texture2D tex0;    // t0

extern "C" [[spirv::frag]]
void frag_main() {
  vec2 uv = shader_in<0, vec2>;
  shader_out<0, vec4> = vec4(uv, 0, 1) * texture(combine(tex0, samp0), uv);
}
