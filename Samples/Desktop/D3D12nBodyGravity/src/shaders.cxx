
struct uniforms_t {
  int num_particles;
  int num_tiles;
  float dt;
  float damping;
  float G;    // gravitational constant
  float m;    // mass of each particle
  float softening = .00125;
};

// The uniform buffer is bound for integration and rendering.
[[using spirv: uniform, binding(0)]]
uniforms_t uniforms;

struct particle_t {
  vec4 pos, vel;
};

// t0
[[using spirv: buffer, readonly, binding(0)]]
particle_t old_particles[];

// u0
[[using spirv: buffer, writeonly, binding(0)]]
particle_t new_particles[];

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
    p0 = old_particles[gid];

  // Compute the total acceleration on pos.
  vec3 pos = p0.pos.xyz;
  
  vec3 acc { };
  for(int i = 0; i < uniforms.num_particles; ++i) {
    vec3 pos2 = old_particles[i].pos.xyz;
    acc += interaction(pos, pos2, uniforms.m);
  }
  /*
  for(int tile = 0; tile < uniforms.num_tiles; ++tile) {
    // Buffer the next NT particles through shared memory.
    [[spirv::shared]] vec4 cache[NT];
    int index2 = NT * tile + tid;
  //  cache[tid] = index2 < uniforms.num_particles ? 
  //    old_particles[index2].pos : vec4();
  //  glcomp_barrier();

    // Use @meta for to unroll all NT number of particle interactions.
    for(int j = 0; j < NT; ++j)
      acc += interaction(pos.xyz, old_particles[]);    

    // Once all threads complete, go to the next tile.
   //  glcomp_barrier();
  }*/

  if(gid < uniforms.num_particles) {
    // Load the velocity for this thread.
    vec3 vel = p0.vel.xyz;
 
    // Update the velocity and position.
    // Draw the particle back to the center.
    vel += uniforms.dt * acc;
    vel *= uniforms.damping;
 
    pos += uniforms.dt * vel;
 
    // Store the updated position and velocity.
    new_particles[gid] = { vec4(pos, 0), vec4(vel, 0) };
  }
}
