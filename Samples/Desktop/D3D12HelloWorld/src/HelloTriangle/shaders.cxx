template<int location, typename type_t>
[[using spirv: in, location(location)]]
type_t shader_in;

template<int location, typename type_t>
[[using spirv: out, location(location)]]
type_t shader_out;


[[spirv::vert]]
void vert_main() asm("vert") {
	glvert_Output.Position = shader_in<0, vec4>;
  shader_out<0, vec4> = shader_in<1, vec4>;
}

[[spirv::frag]]
void frag_main() asm("frag") {
  shader_out<0, vec4> = shader_in<0, vec4>;
}