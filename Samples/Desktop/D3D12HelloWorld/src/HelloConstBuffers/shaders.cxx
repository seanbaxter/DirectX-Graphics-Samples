#include <cmath>

namespace imgui {
  // imgui attribute tags.
  using color3   [[attribute]] = void;
  using color4   [[attribute]] = void;
  
  template<typename type_t>
  struct minmax_t {
    type_t min, max;
  };
  using range_float [[attribute]] = minmax_t<float>;
  using range_int   [[attribute]] = minmax_t<int>;

  // Resolution for drag.
  using drag_float [[attribute]] = float;

  using title    [[attribute]] = const char*;
  using url      [[attribute]] = const char*;
}


template<int location, typename type_t>
[[using spirv: in, location(location)]]
type_t shader_in;

template<int location, typename type_t>
[[using spirv: out, location(location)]]
type_t shader_out;

struct shadertoy_uniforms_t {
  // shader-specific parameters.
  vec4 mouse      = vec4(.5, .5, .5, .5); // .xy is current or last drag position.
                                          // .zw is current or last click.
                                          // .zw is negative if mouse button is up.
  vec2 resolution = vec2(1280, 720);      // width and height of viewport in pixels.
  float time      = 0;                    // seconds since simulation started.
};

[[using spirv: uniform, binding(0)]]
shadertoy_uniforms_t uniforms;

extern "C" [[spirv::vert]]
void vert() {
  glvert_Output.Position = vec4(shader_in<0, vec2>, 0, 1);
}

template<typename shader_t>
[[spirv::frag]] void frag_shader() {
  shader_t shader { };
  shadertoy_uniforms_t uniforms { };

  // D3D gives us coordinates of top-left, but we'd rather have bottom-left
  // coordinates.
  vec2 uv = glfrag_FragCoord.xy;
  uv.y = uniforms.resolution.y - uv.y;
  shader_out<0, vec4> = shader.render(uv, uniforms);
}

inline vec2 rot(vec2 p, vec2 pivot, float a) {
  p -= pivot;
  p = vec2(
    p.x * cos(a) - p.y * sin(a), 
    p.x * sin(a) + p.y * cos(a)
  );
  p += pivot;

  return p;
}

inline vec2 rot(vec2 p, float a) {
  return rot(p, vec2(), a);
}

////////////////////////////////////////////////////////////////////////////////

struct [[
  .imgui::title="Devil Egg",
  .imgui::url="https://www.shadertoy.com/view/3tXXRX"
]] devil_egg_t {

  vec3 hash32(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz+19.19f);
    return fract((p3.xxy+p3.yzz)*p3.zyx);
  }

  float opUnion(float d1, float d2) { return min(d1, d2); }
  float opSubtraction(float d1, float d2) { return max(-d1, d2); }

  float sdShape(vec2 p, float r) {
    return length(p + .5f * r) - r;
  }
 
  vec2 egg(vec2 sd, vec2 uv, float t, float r, float a) {
    if(r > 0) {
      vec2 uv2 = rot(uv, a);
      float yolk = sdShape(11 / sqrt(2.f) * uv2, r);
      float white = sdShape(5.2f / sqrt(2.f) * uv2, r);
      sd = vec2(opUnion(sd.x, white), opUnion(sd.y, yolk));
    }
    return sd;
  }
 
  float under(vec2 N, float t) {
    float sz = .001;
    float range = 1.8;
    vec2 p1 = vec2(sin(-1.66666f * t), cos(t)) * range;
    vec2 p2 = vec2(sin(t), sin(1.3333f * t)) * range;
    float b = min(length(p1 - N) - sz, length(p2 - N) - sz);
    return b;
  }

  vec2 smallEggField(vec2 sd, vec2 uv, vec2 uvbig, float t) {
    vec2 uvsmall = mod(uv + vec2(.3 * t, 0), Span) - .5f * Span;
    vec2 uvsmall_q = uv - uvsmall;
    
    // Find the dist to the big egg, quantizing big egg's coords to small 
    // egg coords.
    vec2 sdquant = egg(1e6, uvsmall_q, t, BigSize, under(uvsmall_q, t));
    return egg(sd, uvsmall, t, SmallSize * smoothstep(0.f, .8f, sdquant.x - .5f),
      under(10 * uvbig, t));
  }

  vec3 color(vec2 sd, float fact) {
    vec3 o = 1 - smoothstep(0.f, fact * vec3(.06, .03, .02), sd.x); 
    o += BackgroundColor;
    if(sd.x < 0)
      o -= sd.x * .6f;
    o = clamp(o, 0.f, 1.f);

    vec3 ayolk = 1 - smoothstep(0, fact * vec3(.2, .1, .2), sd.yyy);
    o = mix(o, YolkColor, ayolk);
    if(sd.y < 0)
      o -= sd.y * .1f;

    o = clamp(o, 0.f, 1.f);
    return o;
  }

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    constexpr float PI2 = 2 * M_PIf32;
    vec2 uv = frag_coord / u.resolution - .5f;
    vec2 N = uv;
    uv.x *= u.resolution.x / u.resolution.y;
    uv.y -= .1f;
    uv *= Zoom;

    float t = u.time;
    vec2 uvbig = uv;

    // Big egg.
    vec2 sd = egg(1.0e6, uv, t, BigSize, under(uvbig, t));

    // Small eggs.
    vec2 sdsmall = smallEggField(1.0e6, uv, uvbig, t);
    uv -= Span * .5f;
    sdsmall = smallEggField(sdsmall, uv, uvbig, t);

    float weight = step(.1f, sd.x);
    vec3 o = mix(color(sd, 2), color(sdsmall, .2f), step(.1f, sd.x));

    o = pow(o, .5f);
    // o += (hash32(frag_coord + t) - .5f) * FilmGrain;
    o = clamp(o, 0.f, 1.f);
    o *= 1 - length(13 * pow(abs(N), DarkEdge));

    return vec4(o, 1);
  }

  [[.imgui::range_float {  .5,  5 }]] float Zoom = 1;
  [[.imgui::range_float {  .2,  2 }]] float BigSize = .8;
  [[.imgui::range_float { .05,  1 }]] float SmallSize = .09;
  [[.imgui::range_float { .05,  2 }]] float Span = .1;
  [[.imgui::range_float { .01, .5 }]] float FilmGrain = .1;
  [[.imgui::range_float {   1,  5 }]] float DarkEdge = 4;

  [[.imgui::color3]] vec3 YolkColor = vec3(.5, .5, 0);
  [[.imgui::color3]] vec3 BackgroundColor = vec3(.1, .5, .1);
};

struct [[
  .imgui::title="Neon Hypno Bands",
  .imgui::url="https://www.shadertoy.com/view/MdcGW4"]] 
hypno_bands_t {

  float rand(float n){
    return fract(cos(n * 89.42f) * 343.42f);
  }
  float roundx(float x, float p) {
      return floor((x + (p * .5f)) / p) * p;
  }
  float dtoa(float d, float amount) {
      return clamp(1 / (clamp(d, 1 / amount, 1.f) * amount), 0.f, 1.f);
  }
  float sdSegment1D(float uv, float a, float b) {
    return max(max(a - uv, 0.f), uv - b);
  }

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    constexpr float PI2 = 2 * M_PIf32;
    vec2 uv = frag_coord / u.resolution - .5f;
    uv.x *= u.resolution.x / u.resolution.y;
    uv *= Zoom;

    float t = u.time;

    if(Warp) {
      vec2 oldUV = uv;
      uv = pow(abs(uv), sin(t * vec2(WarpX, WarpY)) * .35f + 1.2f) * sign(oldUV);
    }

    float bandRadius = roundx(length(uv), BandSpacing);
    vec3 bandID = vec3(
      rand(bandRadius + 0),
      rand(bandRadius + 1),
      rand(bandRadius + 2)
    );

    float distToLine = sdSegment1D(
      length(uv), 
      bandRadius - (LineSize * .5f), 
      bandRadius + (LineSize * .5f)
    );
    float bandA = dtoa(distToLine, 400.f);// alpha around this band.
    
    float bandSpeed = .1f / max(0.05f, bandRadius);// outside = slower
    float r = -t + PI2 * bandID.x;
    r *= bandSpeed;
    uv *= rot(uv, r);

    float angle = mod(atan2(uv.x, uv.y), PI2);// angle, animated
    float arcLength = bandRadius * angle;
    
    float color = sign(mod(arcLength, 2 * SegmentLength) - SegmentLength);

    return vec4(vec3(bandID * color * bandA), 1);
  }

  [[.imgui::range_float { .5,  8 }]] float Zoom = 1.5;
  [[.imgui::range_float { .1, .5 }]] float BandSpacing = .05;
  [[.imgui::range_float {  0, .5 }]] float LineSize = .008;
  [[.imgui::range_float {  0,  6 }]] float SegmentLength = .5;
  [[.imgui::range_float {  0,  1 }]] float WarpX = .2;
  [[.imgui::range_float {  0,  1 }]] float WarpY = .7;
                                      bool Warp = true;
};

////////////////////////////////////////////////////////////////////////////////
// https://www.shadertoy.com/view/wsVyzw

struct [[
  .imgui::title="Star Amplitude Modulation", 
  .imgui::url="https://www.shadertoy.com/view/wsVyzw"
]] modulation_t {

  // Signed distance to a n-star polygon with external angle en.
  float sdStar(vec2 p, float r, int n, float m) {
    float an = M_PIf32 / n;
    float en = M_PIf32 / m;
    vec2 acs(cos(an), sin(an));
    vec2 ecs(cos(en), sin(en));

    // reduce to first sector.
    float bn = mod(atan2(p.x, p.y), 2 * an) - an;
    p = length(p) * vec2(cos(bn), abs(sin(bn)));

    // line sdf
    p -= r * acs;
    p += ecs * clamp(-dot(p, ecs), 0.0f, r * acs.y / ecs.y);
    return length(p) * sign(p.x);
  }

  float sdShape(vec2 uv, float t) {
    float angle = -t * StarRotationSpeed;
    return sdStar(rot(uv, angle), StarSize, StarPoints, StarWeight);
  }

  vec3 dtoa(float d, vec3 amount) {
    return 1 / clamp(d * amount, 1, amount);
  }

  // https://www.shadertoy.com/view/3t23WG
  // Distance to y(x) = a + b*cos(cx+d)
  float udCos(vec2 p, float a, float b, float c, float d) {
    p = c * (p - vec2(d, a));
    
    // Reduce to principal half cycle.
    p.x = mod(p.x, 2 * M_PIf32);
    if(p.x > M_PIf32)
      p.x = 2 * M_PIf32 - p.x;

    // Fine zero of derivative (minimize distance).
    float xa = 0, xb = 2 * M_PIf32;
    for(int i = 0; i < 7; ++i) { // bisection, 7 bits more or less.
      float  x = .5f * (xa + xb);
      float si = sin(x);
      float co = cos(x);
      float  y = x - p.x + b * c * si * (p.y - b * c * co);
      if(y < 0)
        xa = x;
      else
        xb = x;
    }

    float x = .5f * (xa + xb);
    for(int i = 0; i < 4; ++i) { // Newton-Raphson, 28 bits more or less.
      float si = sin(x);
      float co = cos(x);
      float  f = x - p.x + b * c * (p.y * si - b * c * si * co);
      float df = 1       + b * c * (p.y * co - b * c * (2 * co * co - 1));
      x = x - f / df;
    }

    // Compute distance.
    vec2 q(x, b * c * cos(x));
    return length(p - q) / c;
  }

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    vec2 N = frag_coord / u.resolution - .5f;
    vec2 uv = N;
    uv.x *= u.resolution.x / u.resolution.y;

    uv *= Zoom;
    float t = u.time * PhaseSpeed;

    vec2 uvsq = uv;
    float a = sdShape(uv, t);

    float sh = mix(100.f, 1000.f, Sharpness);

    float a2 = 1.5;
    for(int i = -3; i <= 3; ++i) {
      vec2 uvwave(uv.x, uv.y + i * WaveSpacing);
      float b = smoothstep(1.f, -1.f, a) * WaveAmp + WaveAmpOffset;
      a2 = min(a2, udCos(uvwave, 0.f, b, WaveFreq, t));
    }

    vec3 o = dtoa(mix(a2, a-LineWeight + 4, .03f), sh * Tint);
    if(!InvertColors)
      o = 1 - o;

    o *= 1 - dot(N, N * 2);
    return vec4(clamp(o, 0, 1), 1);
  }

  [[.imgui::range_float {   1,  10 }]] float Zoom = 5;
  [[.imgui::range_float {   0,  10 }]] float LineWeight = 4.3;
  [[.imgui::range_int   {   2,  10 }]] int   StarPoints = 5;
  [[.imgui::range_float {  .1,   3 }]] float Sharpness = .2;
  [[.imgui::range_float {  -5,   5 }]] float StarRotationSpeed = -.5;
  [[.imgui::range_float {   0,   5 }]] float StarSize = 2;
  [[.imgui::range_float {   2,  10 }]] float StarWeight = 6;
  [[.imgui::range_float {   0,   4 }]] float WaveSpacing = .8;
  [[.imgui::range_float {   0,   5 }]] float WaveAmp = 2;
  [[.imgui::range_float {   0,  50 }]] float WaveFreq = 25;
  [[.imgui::range_float {  -2,   2 }]] float PhaseSpeed = .33;
  [[.imgui::range_float { -.1,  .1 }]] float WaveAmpOffset = .01;
  [[.imgui::color3]]                     vec3 Tint = vec3(.15, .5, .8);
  bool InvertColors = false; 
};

////////////////////////////////////////////////////////////////////////////////
// Keep up little square

struct [[
  .imgui::title="Keep Up Little Square",
  .imgui::url="https://www.shadertoy.com/view/wlsXD2"
]] keep_up_square_t {

  vec3 hash32(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz + 19.19f);
    return fract((p3.xxy + p3.yzz) * p3.zyx);
  }

  float dtoa(float d, float amount) {
    return 1 / clamp(d * amount, 1.f, amount);
  }

  float sdSquare(vec2 p, vec2 pos, vec2 origin, float a, float s) {
    vec2 d = abs((rot(p - pos, -a)) - origin) - s;
    return min(max(d.x, d.y), 0.f) + length(max(d, 0.f));
  }

  // returns { x:orig, y:height, z:position 0-1 within this cell }
  vec3 cell(float x) {
    float pos = fract(x / XScale);
    float orig = (x / XScale - pos) * XScale;
    float h = hash32(vec2(orig)).r;
    return vec3(orig, h * YScale, pos);
  }

  vec3 scene(vec2 N, vec2 uv, float t, float xpos, float xcam) {
    vec3 sqCellPrev = cell(xpos - XScale);
    
    // Ground.
    vec3 bigCellThis = cell(uv.x);
    vec3 bigCellNext = cell(uv.x + XScale);
    float bigHeightThis = mix(bigCellThis.y, bigCellNext.y, bigCellThis.z);
    float sd = uv.y - bigHeightThis;

    // Walking square; interpolate between positions.
    vec3 sqCellThis = cell(xpos);
    vec3 sqCellNext = cell(xpos + XScale);
    float aThis = atan2(sqCellPrev.y - sqCellThis.y, 
      sqCellPrev.x - sqCellThis.x);
    float aNext = atan2(sqCellThis.y - sqCellNext.y, 
      sqCellThis.x - sqCellNext.x) - M_PIf32 / 2;

    if(aNext > aThis + M_PIf32) aNext -= 2 * M_PIf32;
    if(aNext < aThis - M_PIf32) aNext += 2 * M_PIf32;

    float szThis = distance(sqCellPrev.xy, sqCellThis.xy);
    float szNext = distance(sqCellThis.xy, sqCellNext.xy);

    float param = pow(sqCellNext.z, Sluggishness);
    float asq = mix(aThis,  aNext,  param);
    float sz  = mix(szThis, szNext, param);

    float sdsq = sdSquare(uv, sqCellThis.xy, sz * vec2(-.5f, .5f), 
      asq + M_PIf32, .5f * sz);

    vec3 o { };
    vec2 uvtemp = uv;

    for(int i = 1; i <= 9; ++i) {
      uvtemp.x -= xpos;
      uvtemp *= vec2(2, 1.8);

      uvtemp.y -= .3;
      uvtemp.x += xpos + 1000;

      vec3 cellThis = cell(uvtemp.x);
      vec3 cellNext = cell(uvtemp.x + XScale);
      float heightThis = mix(cellThis.y, cellNext.y, cellThis.z);
      float sd = uvtemp.y - heightThis;
      o = max(o, dtoa(sd, 1000) * .2f / i);

      float amt = 25 + heightThis * 500;
      o = max(o, dtoa(abs(sd) + .01f, amt) * .4f / i);
    }

    o += .8f - uv.y * 1.1f;
    o.g *= o.r;
    o.r *= .6f;
    o.b += N.y;

    // square
    float alphasq = dtoa(sdsq, 1000);
    o = mix(o, SquareColor, alphasq);

    // ground
    float alphagr = dtoa(sd, 300);
    o = mix(o, GroundColor, alphagr);

    // snow
    float alphasn = dtoa(abs(sd + .01f), 25 + bigHeightThis * 500);
    o = mix(o, SnowColor, alphasn);

    return o;
  }

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    vec2 N = frag_coord / u.resolution - .5f;
    N.x *= u.resolution.x / u.resolution.y;
    vec2 uv = N;

    float t = .16f * u.time + 100;
    float xpos = t * XSpeed;
    float xcam = sin(9 * t) * CamShakiness;

    uv = vec2(xpos + xcam, .3f) + Zoom * N;

    // Calc scene twice and motion blur.
    vec3 sqCellPrev = cell(xpos - XScale);
    float bounce = abs(sin(sqCellPrev.z * 26)) * 
      pow(1 - sqCellPrev.z, .7f) * Bounce;

    vec3 o1 = scene(N, uv + vec2(0, bounce), t, xpos, xcam);
    vec3 o2 = scene(N, uv                  , t, xpos, xcam);

    vec3 o = .5f * (o1 + o2);
    o.b *= .9;
    o += (hash32(frag_coord + t) - .5f) * FilmGrain;

    o *= 1.1f - dot(N, N);
    o = clamp(o, 0, 1);

    return vec4(o, 1);
  }

  [[.imgui::range_float {  .1,  5 }]] float Zoom = 1.5;
  [[.imgui::range_float {  .1,  1 }]] float XScale = .3;
  [[.imgui::range_float {   0, .5 }]] float YScale = .2;
  [[.imgui::range_float {   0, .1 }]] float Bounce = .01;
  [[.imgui::range_float {  -3,  3 }]] float XSpeed = 1;
  [[.imgui::range_float {   0, .2 }]] float CamShakiness = .1;
  [[.imgui::range_float {   1, 10 }]] float Sluggishness = 7;
  [[.imgui::range_float { .01, .5 }]] float FilmGrain = .15;
  [[.imgui::color3]] vec3 SquareColor = vec3(0.9, 0.1, 0.1);
  [[.imgui::color3]] vec3 GroundColor = vec3(.3, .6, .1);
  [[.imgui::color3]] vec3 SnowColor   = vec3(0.8, 0.9, 1.0);
};

////////////////////////////////////////////////////////////////////////////////

struct [[
  .imgui::title="Metallic Paint Stir",
  .imgui::url="https://www.shadertoy.com/view/3sG3Rd"
]] paint_t {

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    constexpr float PI2 = 2 * M_PIf32;
    vec2 N = frag_coord / u.resolution - .5f;
    vec2 uv = N;
    uv.x *= u.resolution.x / u.resolution.y;
    uv *= Zoom;

    float t = .2f * u.time;

    // get radius and angle
    float l = sqrt(length(uv));
    float a = atan2(uv.x, uv.y) + sin(l * PI2) * PI2;

    // distort uv by length, animated wave over time
    float ex = mix(MinPow, MaxPow, .5f * sin(l*PI2+a+t*PI2) + .5f);
    uv = sign(uv)*pow(abs(uv), vec2(ex));
    
    float d = abs(fract(length(uv)-t) - .5f);// dist to ring centers
    float c = 1 / max(((2 - l) * 6) * d, .1f);// dist to grayscale amt
    vec4 o(c);
    vec3 col = vec3(
      // generate correlated colorants 0-1 from length, angle, exponent
      clamp(l * l * l, 0.f, 1.f), 
      .5f * sin(a) + .5f,
      (ex - MinPow) / (MaxPow - MinPow)
    );

    col = 1 - mix(vec3(col.r + col.g + col.b) / 3, col, Saturation);

    o.rgb *= col;
    o *= Fade - l;// fade edges (vignette)

    return o;
  }

  [[.imgui::range_float { .1, 10 }]] float Zoom = 4.3;
  [[.imgui::range_float {  0,  4 }]] float MinPow = .6;
  [[.imgui::range_float {  0,  4 }]] float MaxPow = 2;
  [[.imgui::range_float {  0,  4 }]] float Fade = 1.8;
  [[.imgui::range_float {  0,  1 }]] float Saturation = .5;
};

////////////////////////////////////////////////////////////////////////////////

struct [[
  .imgui::title="Menger Journey",
  .imgui::url="https://www.shadertoy.com/view/Mdf3z7"
]] menger_journey_t {

  vec2 rotate(vec2 v, float a) {
    return vec2(cos(a) * v.x + sin(a) * v.y, -sin(a) * v.x + cos(a) * v.y);
  }

  // Two light sources. No specular 
  vec3 getLight(vec3 color, vec3 normal, vec3 dir) {
    vec3 light_dir = normalize(vec3(1, 1, 1));
    vec3 light_dir2 = normalize(vec3(1, -1, 1));

    float diffuse = max(0.f, dot(-normal, light_dir));
    float diffuse2 = max(0.f, dot(-normal, light_dir2));

    return Diffuse * color * (diffuse * LightColor + diffuse2 * LightColor2);
  }

  // For more info on KIFS, see:
  // http://www.fractalforums.com/3d-fractal-generation/kaleidoscopic-%28escape-time-ifs%29/
  float DE(vec3 z, float angle) {
    // Folding 'tiling' of 3D space;
    z = abs(1 - mod(z, 2.f));

    const vec3 Offset(0.92858,0.92858,0.32858);

    float d = 1000.0;
    for (int n = 0; n < NumIterations; n++) {
      z.xy = rotate(z.xy, angle);
      z = abs(z);
      if(z.x < z.y) z.xy = z.yx;
      if(z.x < z.z) z.xz = z.zx;
      if(z.y < z.z) z.yz = z.zy;

      z = Scale * z - Offset * (Scale - 1);
      if(z.z < -0.5f * Offset.z * (Scale - 1))
        z.z += Offset.z * (Scale - 1);

      d = min(d, length(z) * pow(Scale, -n - 1));
    }
    
    return d - 0.001f;
  }

  // Finite difference normal
  vec3 getNormal(vec3 pos, float angle) {
    vec2 e(0, NormalDistance);

    return normalize(vec3(
      DE(pos + e.yxx, angle) - DE(pos - e.yxx, angle),
      DE(pos + e.xyx, angle) - DE(pos - e.xyx, angle),
      DE(pos + e.xxy, angle) - DE(pos - e.xxy, angle)
    ));
  }

  // Solid color 
  vec3 getColor(vec3 normal, vec3 pos) {
    return vec3(1.0);
  }

  // Pseudo-random number
  // From: lumina.sourceforge.net/Tutorials/Noise.html
  float rand(vec2 co) {
    return fract(cos(dot(co, vec2(4.898, 7.23))) * 23421.631f);
  }

  vec3 rayMarch(vec3 from, vec3 dir, vec2 fragCoord, float time) {
    // Add some noise to prevent banding
    float totalDistance = Jitter * rand(fragCoord + time);
    float angle = 4 + 2 * cos(time / 8);

    vec3 dir2 = dir;
    float distance;
    int steps = 0;
    vec3 pos;
    for (int i = 0; i < MaxSteps; i++) {
      // Non-linear perspective applied here.
      dir.zy = rotate(
        dir2.zy,
        totalDistance * cos(time / 4) * NonLinearPerspective
      );
      
      pos = from + totalDistance * dir;
      distance = DE(pos, angle) * FudgeFactor;
      totalDistance += distance;
      if (distance < MinimumDistance) 
        break;

      steps = i;
    }
    
    // 'AO' is based on number of steps.
    // Try to smooth the count, to combat banding.
    float smoothStep = steps + distance / MinimumDistance;
    float ao = 1.1f - smoothStep / float(MaxSteps);
    
    // Since our distance field is not signed,
    // backstep when calc'ing normal
    vec3 normal = getNormal(pos - 3 * dir * NormalDistance, angle);
    
    vec3 color = getColor(normal, pos);
    vec3 light = getLight(color, normal, dir);
    return (color * Ambient + light) * ao;
  }

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    // Camera position (eye), and camera target
    float time = u.time;

    vec3 camPos = 0.5f * time * vec3(1, 0, 0);
    vec3 target = camPos + vec3(1, 0, 0);
    vec3 camUp(0, 1, 0);
    
    // Calculate orthonormal camera reference system
    vec3 camDir = normalize(target - camPos); // direction for center ray
    camUp = normalize(camUp - dot(camDir, camUp) * camDir); // orthogonalize
    vec3 camRight = normalize(cross(camDir, camUp));
    
    vec2 coord = -1 + 2 * frag_coord / u.resolution.xy;
    coord.x *= u.resolution.x / u.resolution.y;
    
    // Get direction for this pixel
    vec3 rayDir = normalize(camDir + (coord.x*camRight + coord.y*camUp)* FOV);
    
    vec3 color = rayMarch(camPos, rayDir, frag_coord, time);
    return vec4(color, 1);
  }

  [[.imgui::range_int { 1, 100 }]] int MaxSteps = 30;
  [[.imgui::range_int { 1, 20 }]]  int NumIterations = 7;

  [[.imgui::range_float { 0, .01}]] float MinimumDistance = .0009;
  [[.imgui::range_float { 0, .01 }]] float NormalDistance = .0002;

  [[.imgui::range_float { 1, 10 }]] float Scale = 3;
  [[.imgui::range_float {.01, 10 }]] float FOV = 1;

  [[.imgui::range_float {-.5, .5 }]] float Jitter = .05;
  [[.imgui::range_float { .1, 1 }]] float FudgeFactor = .7;

  [[.imgui::range_float {-5, 5 }]] float NonLinearPerspective = 2;
  [[.imgui::range_float { 0, 1 }]] float Ambient = .32184;
  [[.imgui::range_float { 0, 1 }]] float Diffuse = .5;

  [[.imgui::color3]] vec3 LightColor = vec3(1, 1, .858);
  [[.imgui::color3]] vec3 LightColor2 = vec3(0, .3333, 1);
};


////////////////////////////////////////////////////////////////////////////////

struct [[
  .imgui::title="Hypercomplex",
  .imgui::url="https://www.shadertoy.com/view/XslSWl"
]] hypercomplex_t {
  // "Hypercomplex" by Alexander Alekseev aka TDM - 2014
  // License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

  float diffuse(vec3 n, vec3 l, float p) {
    return pow(dot(n, l) * .4f + .6f, p);
  }

  float specular(vec3 n, vec3 l, vec3 e, float s) {
    float nrm = (s + 8) / (M_PIf32 * 8);
    return pow(max(dot(reflect(e, n), l), 0.f), s) * nrm;
  }

  float specular(vec3 n, vec3 e, float s) {
    float nrm = (s + 8) / (M_PIf32 * 8);
    return pow(max(1 - abs(dot(n, e)), 0.f), s) * nrm;
  }

  float julia(vec3 p, vec4 q) {
    vec4 z(p, 0.f);
    float z2 = dot(p, p);
    float md2 = 1;

    vec4 nz;
    for(int i = 0; i < 11; ++i) {
      md2 *= 4 * z2;
      nz.x = z.x * z.x - dot(z.yzw, z.yzw);
      nz.y = 2 * (z.x * z.y + z.w * z.z);
      nz.z = 2 * (z.x * z.z + z.w * z.y);
      nz.w = 2 * (z.x * z.w - z.y * z.z);
      z = nz + q;
      z2 = dot(z, z);

      if(z2 > 4)
        break;
    }

    return .25f * sqrt(z2 / md2) * log(z2);
  }

  float rsq(float x) { 
    x = sin(x);
    return pow(abs(x), 3.0f) * sign(x);
  }

  float map(vec3 p, float time) {
    time += 2 * rsq(.5f * time);
    return julia(p, vec4(
      Amplitudes.x * M * sin(time * Frequencies.x),
      Amplitudes.y * M * cos(time * Frequencies.y),
      Amplitudes.z * M * sin(time * Frequencies.z),
      Amplitudes.w * M * cos(time * Frequencies.w)
    ));
  }

  vec3 getNormal(vec3 p, float time) {
    vec2 e(0, Epsilon);

    vec3 n(
      map(p + e.yxx, time),
      map(p + e.xyx, time),
      map(p + e.xxy, time)
    );
    return normalize(n - map(p, time));
  }

  float getAO(vec3 p, vec3 n, float time) {
    const float R = 3.0;
    const float D = 0.8;
    float r = 0;
    for(int i = 0; i < AOSamples; ++i) {
      float f = (float)i / AOSamples;
      float h = 0.1f + f * R;
      float d = map(p + n * h, time);
      r += clamp(h * D - d, 0.f, 1.f) * (1 - f);
    }
    return clamp(1 - r, 0.f, 1.f);
  }

  float spheretracing(vec3 ori, vec3 dir, float time, vec3& p) {
    float t = 0;
    for(int i = 0; i < NumSteps; ++i) {
      p = ori + dir * t;
      float d = map(p, time);

      // Workaround for structurization bug.
      if(d <= 0 | t > 2)
        break;

      t += max(d * .3f, Epsilon);
    }
    return step(t, 2.f);
  }

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    vec2 uv = frag_coord / u.resolution;
    uv = 2 * uv - 1;
    uv.x *= u.resolution.x / u.resolution.y;
    vec2 sc = vec2(sin(RotationSpeed * u.time), cos(RotationSpeed * u.time));

    // tracing of distance map
    vec3 ori(0, 0, Zoom);
    vec3 dir = normalize(vec3(uv, -1));
    ori.xz = vec2(ori.x * sc.y - ori.z * sc.x, ori.x * sc.x + ori.z * sc.y);
    dir.xz = vec2(dir.x * sc.y - dir.z * sc.x, dir.x * sc.x + dir.z * sc.y);

    float time = MorphSpeed * u.time;
    vec3 p;
    float mask = spheretracing(ori, dir, time, p);
    vec3 n = getNormal(p, time);
    float ao = pow(getAO(p, n, time), 2.2f);
    ao *= .5f * n.y + .5f;

    // bg
    vec3 bg = mix(
      mix(0, BG, smoothstep(-1.f, 1.f, uv.y)),
      mix(.5f * BG, 0, smoothstep(-1.f, 1.f, uv.x)),
      smoothstep(-1.f, 1.f, uv.x)
    );
    bg *= 0.8f + 0.2f * smoothstep(0.1f, 0.0f, sin((uv.x - uv.y) * 40));

    // color
    vec3 l0(0, 0, -1);
    vec3 l1 = normalize(vec3(.3f, .5f, .5f));
    vec3 l2(0, 1, 0);

    vec3 color = Red * .4f;
    color += specular(n, l0, dir, 1.f) * Red;
    color += specular(n, l1, dir, 1.f) * Orange * 1.1f;
    color *= 4 * ao;

    color = mix(bg, color, mask);
    
    color = pow(color, .4545f);
    return vec4(color, 1);
  }

  static constexpr float Epsilon = 1e-5f;

  [[.imgui::range_float { -.5, .5 }]] float RotationSpeed = .1;
  [[.imgui::range_float { -1, 1 }]] float MorphSpeed = .53;
  [[.imgui::range_float { .8, 2 }]] float Zoom = 1.3;
  [[.imgui::range_float {.1, 1 }]] float M = .68;
  [[.imgui::range_float {0, 2 }]] vec4 Amplitudes = vec4(.451, .435, .396, .425);
  [[.imgui::range_float {0, 2 }]] vec4 Frequencies = vec4(.96456, .59237, .73426, .42379);

  [[.imgui::range_int {8, 256}]] int NumSteps = 128;
  [[.imgui::range_int {0, 16}]] int AOSamples = 3;

  [[.imgui::color3]] vec3 Red = vec3(.6, .03, .08);
  [[.imgui::color3]] vec3 Orange = vec3(.3, .1, .1);
  [[.imgui::color3]] vec3 BG = vec3(0.05, 0.05, 0.075);
};

////////////////////////////////////////////////////////////////////////////////


struct palette_t {
  
  vec3 fcos(vec3 x) const {
    vec3 w = x;

    // Take the derivative of this term. Only available in SPIR-V builds.
    if codegen(__is_spirv_target)
      w = glfrag_fwidth(x);

    return Approximate ?
      cos(x) * smoothstep(PI2, 0.f, w) :
      cos(x) * sin(.5f * w) / (.5f * w);
  }

  vec3 mcos(vec3 x, bool mode) const {
    return mode ? cos(x) : fcos(x);
  }

  vec3 get_color(float t, bool mode) const {
    vec3 col = color0;
    col += 0.14f * mcos(PI2 *   1.0f * t + color1, mode);
    col += 0.13f * mcos(PI2 *   3.1f * t + color2, mode);
    col += 0.12f * mcos(PI2 *   5.1f * t + color3, mode);
    col += 0.11f * mcos(PI2 *   9.1f * t + color4, mode);
    col += 0.10f * mcos(PI2 *  17.1f * t + color5, mode);
    col += 0.09f * mcos(PI2 *  31.1f * t + color6, mode);
    col += 0.08f * mcos(PI2 *  65.1f * t + color7, mode);
    col += 0.07f * mcos(PI2 * 131.1f * t + color8, mode);
    return col;
  }

  static constexpr float PI2 = 2 * M_PIf32;

  bool Approximate = true;
  [[.imgui::drag_float(.01)]] vec3 color0 = vec3(0.5, 0.5, 0.4);
  [[.imgui::drag_float(.01)]] vec3 color1 = vec3(0.0, 0.5, 0.6);
  [[.imgui::drag_float(.01)]] vec3 color2 = vec3(0.5, 0.6, 1.0);
  [[.imgui::drag_float(.01)]] vec3 color3 = vec3(0.1, 0.7, 1.1);
  [[.imgui::drag_float(.01)]] vec3 color4 = vec3(0.1, 0.5, 1.2);
  [[.imgui::drag_float(.01)]] vec3 color5 = vec3(0.0, 0.3, 0.9);
  [[.imgui::drag_float(.01)]] vec3 color6 = vec3(0.1, 0.5, 1.3);
  [[.imgui::drag_float(.01)]] vec3 color7 = vec3(0.1, 0.5, 1.3);
  [[.imgui::drag_float(.01)]] vec3 color8 = vec3(0.3, 0.2, 0.8);
};

struct [[
  .imgui::title="Band limited synthesis 1",
  .imgui::url="https://www.shadertoy.com/view/WtScDt"
]] band_limited1_t {

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) const {
    vec2 q = (2 * frag_coord - u.resolution) / u.resolution.y;
    float as = u.resolution.x / u.resolution.y;

    // Separation.
    float th = Sweep ? 
      sin(.5f * u.time) * as :
      (2 * u.mouse.x - u.resolution.x) / u.resolution.y;
    bool mode = q.x < th;

    // Deformation.
    vec2 p = 2 * q / dot(q, q);

    // Animation
    p += .05f * u.time;

    // Texture
    vec3 col = min(
      palette.get_color(p.x, mode),
      palette.get_color(p.y, mode)
    );

    // Vignetting.
    col *= 1.5f - .02f * length(q);

    // Separation.
    col *= smoothstep(.005f, .010f, abs(q.x - th));
  
    // Palette
    if(q.y < -.9f) 
      col = palette.get_color(frag_coord.x / u.resolution.x, mode);

    return vec4(col, 1);
  }

  bool Sweep = true;
  palette_t palette;
};

struct [[
  .imgui::title="Band limited synthesis 2",
  .imgui::url="https://www.shadertoy.com/view/wtXfRH"
]] band_limited2_t {

  vec2 deform(vec2 p, float time) {
    // deform 1
    p *= .25f;
    p = .5f * p / dot(p, p);
    p.x += Shift * time;

    // deform 2
    if(!Tubularity) {
      p += .2f * cos(1.5f * p.yx + .03f * 1.0f * time + vec2(0.1, 1.1));
      p += .2f * cos(2.4f * p.yx + .03f * 1.6f * time + vec2(4.5, 2.6));
      p += .2f * cos(3.3f * p.yx + .03f * 1.2f * time + vec2(3.2, 3.4));
      p += .2f * cos(4.2f * p.yx + .03f * 1.7f * time + vec2(1.8, 5.2));
      p += .2f * cos(9.1f * p.yx + .03f * 1.1f * time + vec2(6.3, 3.9));
    }
    return p;
  }

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    vec2 p = (2 * frag_coord - u.resolution) / u.resolution.y;
    float as = u.resolution.x / u.resolution.y;
    vec2 w = p;

    // separation
    float th = Sweep ? 
      sin(.5f * u.time) * as :
      (2 * u.mouse.x - u.resolution.x) / u.resolution.y;
    bool mode = w.x < th;

    // deformation
    p = deform(p, u.time);

    // base color pattern
    vec3 col = palette.get_color(.5f * length(p), mode);

    // lighting
    col *= 1.4f - .14f / length(w);

    // separation
    col *= smoothstep(.005f, .010f, abs(w.x - th));

    // palette
    if(w.y < -0.9f) 
      col = palette.get_color(frag_coord.x / u.resolution.x, mode);

    return vec4(col, 1);
  }

  [[.imgui::range_float { 0, 1 }]] float Shift = .1f;
  bool Sweep = true;
  bool Tubularity = false;
  palette_t palette;
};





// template void frag_shader<devil_egg_t>() asm("frag");
// template void frag_shader<keep_up_square_t>() asm("frag");


// template void frag_shader<paint_t>() asm("paint_stir");

// template void frag_shader<modulation_t>() asm("frag");

// template void frag_shader<hypno_bands_t>() asm("frag");
// template void frag_shader<menger_journey_t>() asm("frag");
// template void frag_shader<hypercomplex_t>() asm("frag");
// template void frag_shader<band_limited1_t>() asm("frag");
// template void frag_shader<band_limited2_t>() asm("frag");