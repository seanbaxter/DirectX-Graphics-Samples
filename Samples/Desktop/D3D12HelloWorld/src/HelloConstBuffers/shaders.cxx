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

////////////////////////////////////////////////////////////////////////////////
// Ray march algorithms

struct trace_result_t {
  bool hit;
  int steps;
  float t;
};

struct sphere_tracer_t {
  template<typename scene_t>
  trace_result_t trace(const scene_t& scene, vec3 o, vec3 dir, float ra, 
    float rb, int max_steps) {

    float t = ra;
    int i = 0;
    bool hit = false;
    float k = scene.KGlobal();

    while(i < max_steps) {
      ++i;

      vec3 p = o + t * dir;
      float v = scene.Object(p);

      if(v > 0) {
        // Hit.
        hit = true;
        break;
      }

      // Move along ray.
      t += max(epsilon, abs(v) / k);

      // Break if ray has escaped.
      if(t > rb)
        break;
    }

    return { hit, i, t  };
  }

  [[.imgui::range_float {0, .3 }]] float epsilon = .1;
};

struct segment_tracer_t {
  template<typename scene_t>
  trace_result_t trace(const scene_t& scene, vec3 o, vec3 dir, float ra,
    float rb, int max_steps) {

    float t = ra;
    bool hit = false;
    float candidate = 1;

    int i = 0;
    while(i < max_steps) {
      ++i;

      vec3 p = o + t * dir;
      float v = scene.Object(p);

      if(v > 0) {
        // Hit.
        hit = true;
        break;
      }

      // Lipschitz constant on a segment.
      float lipschitzSeg = scene.KSegment(p, o + (t + candidate) * dir);

      // Lipschitz marching distance.
      float step = abs(v) / lipschitzSeg;

      // No further than the segment length.
      step = min(step, candidate);

      // But at least, Epsilon.
      step = max(epsilon, step);

      // Move along ray.
      t += step;

      // Escape marched far away.
      if(t > rb)
        break;

      candidate = kappa * step;
    }

    return { hit, i, t };
  }

  [[.imgui::range_float { 0, .3 }]] float epsilon = .1;
  [[.imgui::range_float { 0, 5 }]] float kappa = 2.0;
};

////////////////////////////////////////////////////////////////////////////////
// Scene distance fields

struct blobs_t {

  float Falloff(float x, float R) const {
    float xx = clamp(x / R, 0.f, 1.f);
    float y = 1 - xx * xx;
    return y * y * y;
  }

  float Vertex(vec3 p, vec3 c, float R, float e) const {
    return e * Falloff(length(p - c), R);
  }

  float Object(vec3 p) const {
    float I = 0;
    I += Vertex(p, vec3(-radius / 2,      0, 0), radius, 1);
    I += Vertex(p, vec3( radius / 2,      0, 0), radius, 1);
    I += Vertex(p, vec3( radius / 3, radius, 0), radius, 1);
    return I - T;
  }

  float FalloffK(float e, float R) const {
    return e * 1.72f * abs(e) / R;
  }

  float FalloffK(float a, float b, float R, float e) const {
    float x = 0;
    if(a <= R) {
      // There's a Circle SPIR-V codegen bug preventing this from being 
      // written with multiple return statements. Am investigating.

      if(b < R / 5) {
        float t = 1 - b / R;
        x = 6 * abs(e) * (sqrt(b) / R) * (t * t);

      } else if(a > (R * R) / 5) {
        float t = 1 - a / R;
        x = 6 * abs(e) * (sqrt(a) / R) * (t * t);

      } else {
        x = FalloffK(e, R);
      }
    }

    return x;
  }

  float VertexKSegment(vec3 c, float R, float e, vec3 a, vec3 b) const {
    vec3 axis = normalize(b - a);
    float l = dot(c - a, axis);
    float kk = 0;

    if(l < 0) {
      kk = FalloffK(length(c - a), length(c - b), R, e);

    } else if(length(b - a) < l) {
      kk = FalloffK(length(c - b), length(c - a), R, e);

    } else {
      float dd = length(c - a) - l * l;
      vec3 pc = a + axis * l;
      kk = FalloffK(dd, max(length(c - b), length(c - a)), R, e);
    }

    float grad = max(
      abs(dot(axis, normalize(c - a))), 
      abs(dot(axis, normalize(c - b)))
    );

    return kk * grad;
  }

  float KSegment(vec3 a, vec3 b) const {
    float K = 0;
    K += VertexKSegment(vec3(-radius / 2,      0, 0), radius, 1, a, b);
    K += VertexKSegment(vec3( radius / 2,      0, 0), radius, 1, a, b);
    K += VertexKSegment(vec3( radius / 3, radius, 0), radius, 1, a, b);
    return K;
  }

  float KGlobal() const {
    return FalloffK(1, radius) * 3;
  }

  vec3 ObjectNormal(vec3 p) const {
    vec2 e(0, .001);
    float v = Object(p);
    vec3 n(
      Object(p + e.yxx) - v,
      Object(p + e.xyx) - v,
      Object(p + e.xxy) - v
    );
    return normalize(n);
  }

  [[.imgui::range_float {1, 20 }]] float radius = 8; // Distance between blobs.
  [[.imgui::range_float {0,  1 }]] float T = .5;     // Surface epsilon. 
};

////////////////////////////////////////////////////////////////////////////////

template<const char title[], typename tracer_t, typename scene_t>
struct [[
  .imgui::title=title,
  .imgui::url="https://www.shadertoy.com/view/WdVyDW"
]] tracer_engine_t {

  vec3 RotateY(vec3 p, float a) {
    float sa = sin(a);
    float ca = cos(a);
    return vec3(ca * p.x + sa * p.z, p.y, -sa * p.x + ca * p.z);    
  }

  vec3 Background(vec3 rd) {
    return mix(BackgroundColor1, BackgroundColor2, rd.y + .25);
  }

  vec3 Shade(vec3 p, vec3 n) {
    vec3 l1 = normalize(vec3(-2, -1, -1));
    vec3 l2 = normalize(vec3(2, 0, 1));
    float d1 = pow(.5f * (1 + dot(n, l1)), 2.f);
    float d2 = pow(.5f * (1 + dot(n, l2)), 2.f);
    return vec3(.6) + .2f * (d1 + d2) * Background(n);
  }

  vec3 ShadeSteps(int n) {
    float t = (float)n / MaxSteps;
    return t < .5f ? 
      mix(ShadeColor1, ShadeColor2, 2 * t) :
      mix(ShadeColor2, ShadeColor3, 2 * t - 1);
  }

  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    vec2 pixel = 2 * (frag_coord / u.resolution) - 1;
    vec2 mouse = 2 * (u.mouse.xy / u.resolution.xy) - 1;

    float asp = u.resolution.x / u.resolution.y;
    vec3 rd = normalize(vec3(asp * pixel.x, pixel.y - 1.5f, -4.f));
    vec3 ro(0, 18, 40);

    float a = .25f * u.time;
    ro = RotateY(ro, a);
    rd = RotateY(rd, a);

    // Shade this object.
    vec3 color = Background(rd);

    trace_result_t result { };

    constexpr bool is_dual = @is_class_template(tracer_t, std::pair);
    if constexpr(is_dual)
      result = (pixel.x < mouse.x) ?
        tracer.first.trace(scene, ro, rd, 20, 60, MaxSteps) :
        tracer.second.trace(scene, ro, rd, 20, 60, MaxSteps);
    else
      result = tracer.trace(scene, ro, rd, 20, 60, MaxSteps);

    // Render the window.
    if(pixel.y > mouse.y) {
      if(result.hit) {
        vec3 pos = ro + result.t * rd;
        vec3 n = scene.ObjectNormal(pos);
        color = Shade(pos, n);
      }

    } else
      color = ShadeSteps(result.steps);

    // Draw a horizontal line to mark the render vs the step count.
    color *= smoothstep(1.f, 2.f, abs(pixel.y - mouse.y) / (2 / u.resolution.y));

    // Draw a vertical line to mark the sphere vs segment tracer.
    if constexpr(is_dual)
      color *= smoothstep(1.f, 2.f, abs(pixel.x - mouse.x) / (2 / u.resolution.x));

    return vec4(color, 1);
  }

  [[.imgui::range_int {1, 300 }]] int MaxSteps = 150;

  tracer_t tracer;
  scene_t scene;

  [[.imgui::color3]] vec3 BackgroundColor1 = vec3(.8, .8, .9);
  [[.imgui::color3]] vec3 BackgroundColor2 = vec3(.6, .8, 1.0);

  [[.imgui::color3]] vec3 ShadeColor1 = vec3(97, 130, 234) / 255;
  [[.imgui::color3]] vec3 ShadeColor2 = vec3(221, 220, 219) / 255;
  [[.imgui::color3]] vec3 ShadeColor3 = vec3(220, 94, 75) / 255;
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

// The MIT License
// Copyright © 2013 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// A list of useful distance function to simple primitives. All
// these functions (except for ellipsoid) return an exact
// euclidean distance, meaning they produce a better SDF than
// what you'd get if you were constructing them from boolean
// operations.
//
// More info here:
//
// https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm

inline float sq(float x) noexcept { return x * x; }
inline float dot2(vec2 v) noexcept { return dot(v, v); }
inline float dot2(vec3 v) noexcept { return dot(v, v); }
inline float ndot(vec2 a, vec2 b) noexcept { return a.x * b.x - a.y * b.y; }

struct sphere_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    return length(p) - s;
  }

  vec3 pos;
  float s;
};

struct box_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.f) + length(max(d, 0.f));
  }

  vec3 pos;
  vec3 b;
};

struct bounding_box_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    p = abs(p) - b;
    vec3 q = abs(p + e) - e;
    return min(min(
      length(max(vec3(p.x,q.y,q.z),0.f))+min(max(p.x,max(q.y,q.z)),0.f),
      length(max(vec3(q.x,p.y,q.z),0.f))+min(max(q.x,max(p.y,q.z)),0.f)),
      length(max(vec3(q.x,q.y,p.z),0.f))+min(max(q.x,max(q.y,p.z)),0.f));
  }

  vec3 pos;
  vec3 b;
  float e;
};

struct ellipsoid_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1) / k1;
  }

  vec3 pos;
  vec3 r;
};

struct torus_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    return length(vec2(length(p.xz) - t.x, p.y)) - t.y;
  }

  vec3 pos;
  vec2 t;
};

struct capped_torus_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    p.x = abs(p.x);
    float k = (sc.y * p.x > sc.x * p.y) ? dot(p.xy, sc) : length(p.xy);
    return sqrt(dot2(p) + ra * ra - 2 * ra * k) - rb;
  }

  vec3 pos;
  vec2 sc;
  float ra, rb;
};

struct hex_prism_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    vec3 q = abs(p);

    const vec3 k = vec3(-0.8660254, 0.5, 0.57735);
    p = q;

    p.xy -= 2 * min(dot(k.xy, p.xy), 0.f) * k.xy;
    vec2 d(
      length(p.xy - vec2(clamp(p.x, -k.z*h.x, k.z*h.x), h.x)) * sign(p.y - h.x),
      p.z - h.y
    );
    return min(max(d.x, d.y), 0.f) + length(max(d, 0.f));
  }

  vec3 pos;
  vec2 h;
};

struct octagon_prism_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    const vec3 k = vec3(
      -0.9238795325,   // sqrt(2+sqrt(2))/2 
       0.3826834323,   // sqrt(2-sqrt(2))/2
       0.4142135623    // sqrt(2)-1 
    ); 

    // reflections
    p = abs(p);
    p.xy -= 2 * min(dot(vec2( k.x, k.y), p.xy), 0.f) * vec2( k.x, k.y);
    p.xy -= 2 * min(dot(vec2(-k.x, k.y), p.xy), 0.f) * vec2(-k.x, k.y);

    // polygon side
    p.xy -= vec2(clamp(p.x, -k.z * r, k.z * r), r);
    vec2 d(length(p.xy) * sign(p.y), p.z - h);
    return min(max(d.x, d.y), 0.f) + length(max(d, 0.f));
  }

  vec3 pos;
  float r, h;
};

struct capsule_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot2(ba), 0.f, 1.f);
    return length(pa - ba * h) - r;
  }

  vec3 pos;
  vec3 a;
  vec3 b;
  float r;
};

struct round_cone_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    vec2 q(length(p.xz), p.y);

    float b = (r1 - r2) / h;
    float a = sqrt(1 - b * b);
    float k = dot(q, vec2(-b, a));

    float dist = 0;
    if(k < 0) 
      dist = length(q) - r1;
    else if(k > a * h) 
      dist = length(q - vec2(0, h)) - r2;
    else 
      dist = dot(q, vec2(a, b)) - r1;
    return dist;
  }

  vec3 pos;
  float r1, r2, h;
};

struct round_cone2_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    // sampling independent computations (only depend on shape)
    vec3 ba = b - a;
    float l2 = dot2(ba);
    float rr = r1 - r2;
    float a2 = l2 - rr * rr;
    float il2 = 1 / l2;

    // sampling dependant computations
    vec3 pa = p - a;
    float y = dot(pa, ba);
    float z = y - l2;
    float x2 = dot2(pa * l2 - ba * y);
    float y2 = y * y * l2;
    float z2 = z * z * l2;

    // single square root!
    float k = sign(rr) * rr * rr * x2;

    float dist = 0;
    if(sign(z) * a2 * z2 > k)
      dist = sqrt(x2 + z2) *il2 - r2;
    else if(sign(y) * a2 * y2 < k)
      dist = sqrt(x2 + y2) *il2 - r1;
    else
      dist = (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
    return dist;
  }

  vec3 pos;
  vec3 a, b;
  float r1, r2;
};

struct tri_prism_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    const float k = sqrt(3.f);
    vec2 h2 = h;

    h2.x *= .5f * k;
    p.xy /= h2.x;
    p.x = abs(p.x) - 1;
    p.y = p.y + 1 / k;
    if(p.x + k * p.y > 0)
      p.xy = vec2(p.x - k * p.y, -k * p.x - p.y) / 2;
    p.x -= clamp(p.x, -2.f, 0.f);
    float d1 = length(p.xy) * sign(-p.y) * h2.x;
    float d2 = abs(p.z) - h2.y;
    return length(max(vec2(d1, d2), 0.f)) + min(max(d1, d2), 0.f);
  }

  vec3 pos;
  vec2 h;
};

struct cylinder_t {
  float sd(vec3 p) const noexcept {
    p -= pos;
    vec2 d = abs(vec2(length(p.xz), p.y)) - h;
    return min(max(d.x, d.y), 0.f) + length(max(d, 0.f));
  }

  vec3 pos;
  vec2 h;
};

struct cylinder2_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    vec3 pa = p - a;
    vec3 ba = b - a;
    float baba = dot2(ba);
    float paba = dot(pa, ba);

    float x = length(pa * baba - ba * paba) - r * baba;
    float y = abs(paba - baba * .5f) - baba * .5f;
    float x2 = x * x;
    float y2 = y * y * baba;
    float d = max(x, y) < 0 ? 
      -min(x2, y2) : 
      (x > 0 ? x2 : 0.f) + (y > 0 ? y2 : 0.f);
    return sign(d) * sqrt(abs(d)) / baba;
  }

  vec3 pos;
  vec3 a, b;
  float r;
};

struct cone_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    vec2 q = h * vec2(c.x, -c.y) / c.y;
    vec2 w(length(p.xz), p.y);

    vec2 a = w - q * clamp(dot(w, q) / dot2(q), 0.f, 1.f);
    vec2 b = w - q * vec2(clamp(w.x / q.x, 0.f, 1.f), 1.f);
    float k = sign(q.y);
    float d = min(dot2(a), dot2(b));
    float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
    return sqrt(d) * sign(s);
  }

  vec3 pos;
  vec2 c;
  float h;
};

struct capped_cone_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    vec2 q(length(p.xz), p.y);

    vec2 k1(r2, h);
    vec2 k2(r2 - r1, 2 * h);
    vec2 ca(q.x - min(q.x, q.y < 0 ? r1 : r2), abs(q.y) - h);
    vec2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot2(k2), 0.f, 1.f);

    float s = cb.x < 0 & ca.y < 0 ? -1.f : 1.f;
    return s * sqrt(min(dot2(ca), dot2(cb)));
  }

  vec3 pos;
  float h, r1, r2;
};

struct capped_cone2_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    float rba = rb - ra;
    float baba = dot2(b - a);
    float papa = dot2(p - a);
    float paba = dot(p - a, b - a) / baba;

    float x = sqrt(papa - paba * paba * baba);

    float cax = max(0.f, x - (paba < .5f ? ra : rb));
    float cay = abs(paba - .5f) - .5f;

    float k = rba * rba + baba;
    float f = clamp((rba * (x - ra) + paba * baba) / k, 0.f, 1.f);

    float cbx = x - ra - f * rba;
    float cby = paba - f;

    float s = cbx < 0 & cay < 0 ? -1.f : 1.f;
    return s * sqrt(min(cax * cax + cay * cay * baba, 
      cbx * cbx + cby * cby * baba));
  }

  vec3 pos;
  vec3 a, b;
  float ra, rb;
};

struct solid_angle_t {  
  float sd(vec3 p) const noexcept {
    p -= pos;
    vec2 p2(length(p.xz), p.y);
    float l = length(p2) - ra;
    float m = length(p2 - c * clamp(dot(p2, c), 0.f, ra));
    return max(l, m * sign(c.y * p2.x - c.x * p2.y));
  }

  vec3 pos;
  vec2 c;
  float ra;
};

struct octahedron_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    p = abs(p);
    return (p.x + p.y + p.z - s) * 0.57735027f;
  }

  vec3 pos;
  float s;
};

struct pyramid_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    float m2 = sq(h) + .25f;

    // symmetry
    p.xz = abs(p.xz);
    p.xz = p.z > p.x ? p.zx : p.xz;
    p.xz -= .5f;

    // project into face plane (2D)
    vec3 q(p.z, h * p.y - .5f * p.x, h * p.x + .5f * p.y);
    float s = max(-q.x, 0.f);
    float t = clamp((q.y - .5f * p.z) / (m2 + .25f), 0.f, 1.f);

    float a = m2 * (q.x + s) * (q.x + s) + q.y * q.y;
    float b = m2 * (q.x + .5f * t) * (q.x + .5f * t) + sq(q.y - m2 * t);
    float d2 = min(q.y, -q.x * m2 - q.y * .5f) > 0 ? 0.f : min(a, b);

    // recover 3D and scale, and add sign
    return sqrt((d2 + sq(q.z)) / m2) * sign(max(q.z, -p.y));
  }

  vec3 pos;
  float h;
};

struct rhombus_t {
  float sd(vec3 p) const noexcept {
    p -= pos;

    p = abs(p);
    vec2 b(la, lb);
    float f = clamp((ndot(b, b - 2 * p.xz)) / dot2(b), -1.f, 1.f);
    vec2 q(
      length(p.xz - .5f * b * vec2(1 - f, 1 + f)) * 
        sign(p.x * b.y + p.z * b.x - b.x * b.y) - ra,
      p.y - h
    );

    return min(max(q.x, q.y), 0.f) + length(max(q, 0.f));
  }

  vec3 pos;
  float la, lb, h, ra;
};

struct [[
  .imgui::title="Raymarching - primitives",
  .imgui::url="https://www.shadertoy.com/view/Xds3zN"
]] raymarch_prims_t {

  vec2 opU(vec2 d1, vec2 d2) const noexcept {
    return d1.x < d2.x ? d1 : d2;
  }

  vec2 map(vec3 pos) const noexcept {
    vec2 res(1e10, 0);

    res = opU(res, vec2(sphere.sd(pos), 26.9));

    // Row 0
    res = opU(res, vec2(bounding_box.sd(pos), 16.9));
    res = opU(res, vec2(torus.sd(pos), 25.0));
    res = opU(res, vec2(cone.sd(pos), 55.0));
    res = opU(res, vec2(capped_cone.sd(pos), 13.67));
    res = opU(res, vec2(solid_angle.sd(pos), 49.13));
    
    // Row 1
    res = opU(res, vec2(capped_torus.sd(pos), 8.5));
    res = opU(res, vec2(box.sd(pos), 3.0));
    res = opU(res, vec2(capsule.sd(pos), 31.9));
    res = opU(res, vec2(cylinder.sd(pos), 8.0));
    res = opU(res, vec2(hex_prism.sd(pos), 18.4));
    
    // Row 2
    res = opU(res, vec2(pyramid.sd(pos), 13.56));
    res = opU(res, vec2(octahedron.sd(pos), 23.56));
    res = opU(res, vec2(tri_prism.sd(pos), 43.5));
    res = opU(res, vec2(ellipsoid.sd(pos), 43.17));
    res = opU(res, vec2(rhombus.sd(pos), 17.0));

    // Row 3
    res = opU(res, vec2(octagon_prism.sd(pos), 51.8));
    res = opU(res, vec2(cylinder2.sd(pos), 32.1));
    res = opU(res, vec2(capped_cone2.sd(pos), 46.1));
    res = opU(res, vec2(round_cone.sd(pos), 37.0));
    res = opU(res, vec2(round_cone2.sd(pos), 51.7));

    return res;
  }

  // http://iquilezles.org/www/articles/boxfunctions/boxfunctions.htm
  vec2 iBox(vec3 ro, vec3 rd, vec3 rad) const noexcept {
    vec3 m = 1 / rd;
    vec3 n = m * ro;
    vec3 k = abs(m) * rad;
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;
    return vec2(
      max(max(t1.x, t1.y), t1.z),
      min(min(t2.x, t2.y), t2.z)
    );
  }

  float calcAO(vec3 pos, vec3 nor) const noexcept {
    float occ = 0.0;
    float sca = 1.0;
    for(int i = 0; i < 5; ++i) {
      float h = 0.01f + 0.12f * i / 4;
      float d = map(pos + h * nor).x;
      occ += (h - d) * sca;
      sca *= 0.95;
      if(occ > 0.35) 
        break;
    }
    return clamp(1 - 3 * occ, 0.f, 1.f) * (0.5f + 0.5f * nor.y);
  }

  // http://iquilezles.org/www/articles/checkerfiltering/checkerfiltering.htm
  float checkersGradBox(vec2 p, vec2 dpdx, vec2 dpdy) const noexcept {
    // filter kernel
    vec2 w = abs(dpdx) + abs(dpdy) + 0.001f;
    // analytical integral (box filter)
    vec2 i = 2 * (
      abs(fract((p - 0.5f * w) * 0.5f) - 0.5f) -
      abs(fract((p + 0.5f * w) * 0.5f) - 0.5f)
    ) / w;

    // xor pattern
    return 0.5f - 0.5f * i.x * i.y;                  
  }

  // http://iquilezles.org/www/articles/rmshadows/rmshadows.htm
  float calcSoftshadow(vec3 ro, vec3 rd, float tmin, float tmax) const noexcept {
    // bounding volume
    float tp = (0.8f - ro.y) / rd.y; 
    if(tp > 0) tmax = min(tmax, tp);

    float res = 1.0;
    float t = tmin;
    for(int i = 0; i < 24; ++i) {
      float h = map(ro + rd * t).x;
      float s = clamp(8 * h / t, 0.f, 1.f);
      res = min(res, s * s * (3 - 2 * s));
      t += clamp(h, 0.02f, 0.2f);
      if(res < 0.004f | t > tmax) 
        break;
    }
    return clamp(res, 0.f, 1.f);
  }

  vec3 calcNormal(vec3 pos) const noexcept {
    vec2 e = vec2(1, -1) * 0.5773f * 0.0005f;
    return normalize(
      e.xyy * map(pos + e.xyy).x + 
      e.yyx * map(pos + e.yyx).x + 
      e.yxy * map(pos + e.yxy).x + 
      e.xxx * map(pos + e.xxx).x
    );
  }

  vec2 raycast(vec3 ro, vec3 rd) const noexcept {
    vec2 res(-1);
    float tmin = 1;
    float tmax = 20;

    // raytrace floor plane
    float tp1 = (0 - ro.y) / rd.y;
    if(tp1 > 0) {
      tmax = min(tmax, tp1);
      res.x = tp1;
      res.y = 1;
    }

    // raymarch primitives
    vec2 tb = iBox(ro - vec3(0, 0.4, -.5), rd, vec3(2.5, 0.41, 3.0));
    if(tb.x < tb.y & tb.y > 0 & tb.x < tmax) {
      tmin = max(tb.x, tmin);
      tmax = max(tb.y, tmax);

      float t = tmin;
      // Use & as workaround for sturcture CFG bug.
      for(int i = 0; i < 70 & t < tmax; ++i) {
        vec2 h = map(ro + rd * t);
        if(abs(h.x) < .0001f * t) {
          res = vec2(t, h.y);
          break;
        }
        t += h.x;
      }
    }
    return res;
  }

  vec3 render(vec3 ro, vec3 rd, vec3 rdx, vec3 rdy) const noexcept {
    // background
    vec3 col = vec3(.7, .7, .9) - max(rd.y, 0.f) * .3f;

    // raycast scene
    vec2 res = raycast(ro, rd);
    float t = res.x;
    float m = res.y;
    if(m > -.5f) {
      vec3 pos = ro + t * rd;
      vec3 nor = m < 1.5f ? vec3(0, 1, 0) : calcNormal(pos);
      vec3 ref = reflect(rd, nor);

      // material
      col = 0.2f + 0.2f * sin(2 * m + vec3(0, 1, 2));
      float ks = 1;

      if(m < 1.5f) {
        // project pixel footprint into the plane
        vec3 dpdx = ro.y * (rd / rd.y - rdx / rdx.y);
        vec3 dpdy = ro.y * (rd / rd.y - rdy / rdy.y);

        float f = checkersGradBox(3 * pos.xz, 3 * dpdx.xz, 3 * dpdy.xz);
        col = .15f + f * vec3(0.05);
        ks = .4;
      }

      // lighting
      float occ = calcAO(pos, nor);

      vec3 lin { };

      // sun
      {
        vec3 lig = normalize(vec3(-.5, .4, -.6));
        vec3 hal = normalize(lig - rd);
        float dif = clamp(dot(nor, lig), 0.f, 1.f);

        dif *= calcSoftshadow(pos, lig, 0.02, 2.5);
        float spe = pow(clamp(dot(nor, hal), 0.f, 1.f), 16.f);
        spe *= dif;
        spe *= 0.04f + 0.96f * pow(clamp(1 - dot(hal, lig), 0.f, 1.f), 5.f);
        lin += col * 2.2f * dif * vec3(1.3, 1, 0.7);
        lin +=       5.0f * spe * vec3(1.3, 1, 0.7) * ks;
      }

      // sky
      {
        float dif = sqrt(clamp(.5f + .5f * nor.y, 0.f, 1.f));
        dif *= occ;
        float spe = smoothstep(-.2f, .2f, ref.y);
        spe *= dif;
        spe *= 0.04f + 0.96f * pow(clamp(1 + dot(nor, rd), 0.f, 1.f), 5.f);
        spe *= calcSoftshadow(pos, ref, .02, 2.5);
        lin += col * 0.6f * dif * vec3(0.4, 0.6, 1.15);
        lin +=       2.0f * spe * vec3(0.4, 0.6, 1.30) * ks;
      }

      // back
      {
        float dif = clamp(dot(nor, normalize(vec3(0.5, 0.0, 0.6))), 0.f, 1.f) *
          clamp(1 - pos.y, 0.f, 1.f);
        dif *= occ;
        lin += col * 0.55f * dif * vec3(.25, .25, .25);
      }

      // sss
      {
        float dif = pow(clamp(1 + dot(nor, rd), 0.f, 1.f), 2.f);
        dif *= occ;
        lin += col * .25f * dif * vec3(1);
      }

      col = lin;
      col = mix(col, vec3(0.7, 0.7, 0.9), 1 - exp(-.0001f * t * t * t));
    }

    return vec3(clamp(col, 0.f, 1.f));
  }

  mat3 setCamera(vec3 ro, vec3 ta, float cr) const noexcept {
    vec3 cw = normalize(ta - ro);
    vec3 cp = vec3(sin(cr), cos(cr), 0);
    vec3 cu = normalize( cross(cw,cp) );
    vec3 cv =          ( cross(cu,cw) );
    return mat3( cu, cv, cw );
  }


  vec4 render(vec2 frag_coord, shadertoy_uniforms_t u) {
    vec2 mo = u.mouse.xy / u.resolution.xy;
    float time = 32 + u.time * 1.5f;

    // Camera.
    vec3 ta(.5, -.5, -.6);
    vec3 ro = ta + vec3(
      4.5f * cos(.1f * time + 7 * mo.x),
      1.3f + 2 * mo.y,
      4.5f * sin(.1f * time + 7 * mo.x)
    );
    ro = normalize(ro) * distance;
    mat3 ca = setCamera(ro, ta, 0);

    vec3 tot { };

    // TODO: MSAA here.

    vec2 p = (2 * frag_coord - u.resolution.xy) / u.resolution.y;

    // ray direction.
    vec3 rd = ca * normalize(vec3(p, 2.5));

    // ray differentials.
    vec2 px = (2 * (frag_coord + vec2(1, 0)) - u.resolution.xy) / u.resolution.y;
    vec2 py = (2 * (frag_coord + vec2(0, 1)) - u.resolution.xy) / u.resolution.y;
    vec3 rdx = ca * normalize(vec3(px, 2.5f));
    vec3 rdy = ca * normalize(vec3(py, 2.5f));

    // render
    vec3 col = render(ro, rd, rdx, rdy);

    col = pow(col, vec3(0.4545));
    tot += col;

    return vec4(tot, 1);
  }

  float distance = 5;
  
  sphere_t sphere = { vec3(-2.0, 0.25, 0.0), .25 };

  // Row 0.
  torus_t        torus          = { vec3( 0, 0.30,  1), vec2(.25, .05) };
  bounding_box_t bounding_box   = { vec3( 0, 0.25,  0), vec3(.3, .25, .2), .025 };
  cone_t         cone           = { vec3( 0, 0.45, -1), vec2(.6, .8), .45f };
  capped_cone_t  capped_cone    = { vec3( 0, 0.25, -2), .25f, .25f, .1f };
  solid_angle_t  solid_angle    = { vec3( 0, 0.00, -3), vec2(3, 4) / 5, 0.4f };
   
  // Row 1.   
  capped_torus_t capped_torus   = { vec3( 1, 0.30,  1), vec2(.866025, -.5), .25f, .05f };
  box_t          box            = { vec3( 1, 0.25,  0), vec3(.3, .25, .1) };
  capsule_t      capsule        = { vec3( 1, 0.00, -1), vec3(-.1, .1, -.1), vec3(0.2, .4, .2), .1 };
  cylinder_t     cylinder       = { vec3( 1, 0.25, -2), vec2(.15, .25) };
  hex_prism_t    hex_prism      = { vec3( 1, 0.20, -3), vec2(.2, .05) };
   
  // Row 2.  
  pyramid_t      pyramid        = { vec3(-1, -0.6, -3), 1.f };
  octahedron_t   octahedron     = { vec3(-1, 0.35, -2), .35f };
  tri_prism_t    tri_prism      = { vec3(-1, 0.15, -1), vec2(.3, .05) };
  ellipsoid_t    ellipsoid      = { vec3(-1, 0.25,  0), vec3(.2, .25, .05) };
  rhombus_t      rhombus        = { vec3(-1, 0.34,  1), .15f, .25f, .04f, .08f };

  // Row 3.
  octagon_prism_t octagon_prism = { vec3( 2, 0.20, -3), 0.2f, 0.05f };
  cylinder2_t     cylinder2     = { vec3( 2, 0.15, -2), vec3(.1, -.1, 0), vec3(-.2, .35, .1), .08f };
  capped_cone2_t  capped_cone2  = { vec3( 2, 0.10, -1), vec3(.1, 0, 0), vec3(-.2, .4, .1), .15f, .05f };
  round_cone_t    round_cone    = { vec3( 2, 0.15,  1), .2f, .1f, .3f };
  round_cone2_t   round_cone2   = { vec3( 2, 0.15,  0), vec3(.1, 0, 0), vec3(-.1, .35, .1), .15f, .05f };
};


template void frag_shader<devil_egg_t>() asm("devil");
template void frag_shader<keep_up_square_t>() asm("square");
template void frag_shader<modulation_t>() asm("modulation");
template void frag_shader<hypno_bands_t>() asm("bands");
template void frag_shader<paint_t>() asm("paint");
template void frag_shader<menger_journey_t>() asm("menger");
template void frag_shader<hypercomplex_t>() asm("hypercomplex");
template void frag_shader<band_limited1_t>() asm("band1");
template void frag_shader<band_limited2_t>() asm("band2");
template void frag_shader<tracer_engine_t<"sphere tracer", sphere_tracer_t, blobs_t>>() asm("sphere");
template void frag_shader<tracer_engine_t<"segment tracer", segment_tracer_t, blobs_t>>() asm("segment");
template void frag_shader<tracer_engine_t<"comparison", std::pair<sphere_tracer_t, segment_tracer_t>, blobs_t>>() asm("comparison");
template void frag_shader<raymarch_prims_t>() asm("raymarch");