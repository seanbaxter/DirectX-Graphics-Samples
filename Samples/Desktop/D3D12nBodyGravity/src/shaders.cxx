template<int location, typename type_t>
[[using spirv: in, location(location)]]
type_t shader_in;

template<int location, typename type_t>
[[using spirv: out, location(location)]]
type_t shader_out;

template<int binding, typename type_t>
[[using spirv: buffer, readonly, binding(binding)]]
type_t StructuredBuffer[];

template<int binding, typename type_t>
[[using spirv: buffer, binding(binding)]]
type_t RWStructuredBuffer[];

struct uniforms_t {
  int num_particles;
  int num_tiles;
  float dt;
  float damping;
  float G;    // gravitational constant
  float m;    // mass of each particle
  float softening = .00125;
  float padding;
};

// The uniform buffer is bound for integration and rendering.
[[using spirv: uniform, binding(0)]]
uniforms_t uniforms;

struct particle_t {
  vec4 pos, vel;
};

// Return the force on a from the influence of b.
inline vec3 interaction(vec3 a, vec3 b, float mass) {
  vec3 r = b.xyz - a;

  float softeningSq = uniforms.softening * uniforms.softening;
  float dist2 = dot(r, r) + softeningSq;
  float invDist = inversesqrt(dist2);
  float invDistCube = invDist * invDist * invDist;

  float s = mass * invDistCube;
  return s * r;
}

const int NT = 128;

extern "C" [[using spirv: comp, local_size(NT)]] 
void integrate_shader() {
  int tid = glcomp_LocalInvocationID.x;
  int gid = glcomp_GlobalInvocationID.x;

  // Load the position for this thread.
  particle_t p0;
  if(gid < uniforms.num_particles)
    p0 = StructuredBuffer<0, particle_t>[gid];

  // Compute the total acceleration on pos.
  vec3 pos = p0.pos.xyz;
  vec3 acc { };
  for(int tile = 0; tile < uniforms.num_tiles; ++tile) {
    [[spirv::shared]] vec4 cache[NT];
    int index2 = NT * tile + tid;
    cache[tid] = index2 < uniforms.num_particles ? 
      StructuredBuffer<0, particle_t>[index2].pos : vec4();
    glcomp_barrier();

    // Use @meta for to unroll all NT number of particle interactions.
    @meta for(int j = 0; j < NT; ++j)
      acc += interaction(pos.xyz, cache[tid].xyz, uniforms.m);    

     glcomp_barrier();
  }

  if(gid < uniforms.num_particles) {
    // Load the velocity for this thread.
    vec3 vel = p0.vel.xyz;
 
    // Update the velocity and position.
    // Draw the particle back to the center.
    vel += uniforms.dt * acc;
    vel *= uniforms.damping;
 
    pos += uniforms.dt * vel;
 
    // Store the updated position and velocity.
    RWStructuredBuffer<0, particle_t>[gid] = { vec4(pos, 0), vec4(vel, 0) };
  }
}

////////////////////////////////////////////////////////////////////////////////


extern "C" [[spirv::vert]]
void vert() {
  // Write the buffer object's particle position.
  particle_t particle = StructuredBuffer<0, particle_t>[glvert_VertexIndex];
  shader_out<0, vec4> = particle.pos;
  // glvert_Output.Position = particle.pos;
}

struct gs_uniforms_t {
  mat4 worldViewProj;
  mat4 invView;
};
[[using spirv: uniform, binding(0)]]
gs_uniforms_t gs_uniforms;

extern "C" [[spirv::geom(points, triangle_strip, 4)]]
void geom() {

  static constexpr vec3 corners[] {
    -1,  1, 0,
     1,  1, 0,
    -1, -1, 0,
     1, -1, 0
  };
  static constexpr vec2 uv[] {
    0, 0, 
    1, 0,
    0, 1,
    1, 1
  };
  float radius = 10;

  // vec3 center = glgeom_Input[0].Position.xyz;
  vec3 center = shader_in<0, vec4[1]>[0].xyz;

  @meta for(int i = 0; i < 4; ++i) {{
    // Write a billboard corner.
    vec3 pos = center + (mat3)gs_uniforms.invView * (radius * corners[i]);
    glgeom_Output.Position = gs_uniforms.worldViewProj * vec4(pos, 1);

    glgeom_EmitVertex();
  }}

  glgeom_EndPrimitive();
}

extern "C" [[spirv::frag]]
void frag() {
  shader_out<0, vec4> = vec4(1, 0, 0, 1);
}
