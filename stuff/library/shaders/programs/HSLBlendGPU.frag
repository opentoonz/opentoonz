#ifdef GL_ES
precision mediump float;
#endif


uniform mat3      worldToOutput;

uniform sampler2D inputImage[2];
uniform mat3      outputToInput[2];

uniform bool bhue;    // Blend HUE?
uniform bool bsat;    // Blend Saturation?
uniform bool blum;    // Blend Luminosity?
uniform float balpha; // Blending Alpha
uniform bool bmask;   // Base mask?

// ---------------------------
// Blending calculations from:
// https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_blend_equation_advanced.txt

float minv3(vec3 c)
{
  return min(min(c.r, c.g), c.b);
}

float maxv3(vec3 c)
{
  return max(max(c.r, c.g), c.b);
}

float lumv3(vec3 c)
{
  return dot(c, vec3(0.30, 0.59, 0.11));
}

float satv3(vec3 c)
{
  return maxv3(c) - minv3(c);
}

// If any color components are outside [0,1], adjust the color to get the components in range.
vec3 ClipColor(vec3 color)
{
  float lum = lumv3(color);
  float mincol = minv3(color);
  float maxcol = maxv3(color);
  if (mincol < 0.0) {
    color = lum + ((color-lum) * lum) / (lum-mincol);
  }
  if (maxcol > 1.0) {
    color = lum + ((color-lum) * (1.0 -lum)) / (maxcol-lum);
  }
  return color;
}

// Take the base RGB color <cbase> and override its luminosity
// with that of the RGB color <clum>.
vec3 SetLum(vec3 cbase, vec3 clum)
{
  float lbase = lumv3(cbase);
  float llum = lumv3(clum);
  float ldiff = llum - lbase;
  vec3 color = cbase + vec3(ldiff);
  return ClipColor(color);
}

// Take the base RGB color <cbase> and override its saturation with
// that of the RGB color <csat>.  The override the luminosity of the
// result with that of the RGB color <clum>.
vec3 SetLumSat(vec3 cbase, vec3 csat, vec3 clum)
{
  float minbase = minv3(cbase);
  float sbase = satv3(cbase);
  float ssat = satv3(csat);
  vec3 color;
  if (sbase > 0.0) {
    // Equivalent (modulo rounding errors) to setting the
    // smallest (R,G,B) component to 0, the largest to <ssat>,
    // and interpolating the "middle" component based on its
    // original value relative to the smallest/largest.
    color = (cbase - minbase) * ssat / sbase;
  } else {
    color = vec3(0.0);
  }
  return SetLum(color, clum);
}

// ---------------------------

void main( void )
{
  // Read sources
  vec2 fg_texPos = (outputToInput[0] * vec3(gl_FragCoord.xy, 1.0)).xy;
  vec2 bg_texPos = (outputToInput[1] * vec3(gl_FragCoord.xy, 1.0)).xy;
  vec4 fg_frag = texture2D(inputImage[0], fg_texPos);
  vec4 bg_frag = texture2D(inputImage[1], bg_texPos);

  // De-premultiplication of textures
  vec3 fg_pix = vec3(0.0);
  if (fg_frag.a > 0.0) fg_pix = fg_frag.rgb / fg_frag.a;
  vec3 bg_pix = vec3(0.0);
  if (bg_frag.a > 0.0) bg_pix = bg_frag.rgb / bg_frag.a;

  // Figure out output alpha
  float fg_alpha = fg_frag.a * balpha;
  float bg_alpha = bg_frag.a;
  if (bmask) {
    gl_FragColor.a = bg_alpha;
  } else {
    gl_FragColor.a = bg_alpha + fg_alpha * (1.0 - bg_alpha);
  }
  if (gl_FragColor.a <= 0.0) discard;

  // Perform blending
  vec3 o_pix = SetLumSat(bhue ? fg_pix : bg_pix, bsat ? fg_pix : bg_pix, blum ? fg_pix : bg_pix);
  vec3 b_pix = bmask ? vec3(0.0) : fg_pix;
  gl_FragColor.rgb = bg_pix * bg_alpha * (1.0 - fg_alpha) + mix(b_pix, o_pix, bg_alpha) * fg_alpha;
}
