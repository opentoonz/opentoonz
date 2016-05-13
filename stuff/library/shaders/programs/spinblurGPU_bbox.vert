#ifdef GL_ES
precision mediump float;
#endif

uniform vec4 infiniteRect;
uniform vec4 inputBBox[1];

varying vec4 outputBBox;

uniform vec2  center;
uniform float radius;
uniform float blur;


const float pi          = 3.141592653;
const float pi_twice    = 2.0 * pi;
const float pi_half     = pi / 2.0;


void addPoint(inout vec4 rect, vec2 p) {
  rect.xy = min(rect.xy, p.xy);
  rect.zw = max(rect.zw, p.xy);
}

void addBlurredPointBox(inout vec4 rect, vec2 p)
{
  // Remember the *definition* of angle:    angle = arc length / radius

  // Build p's blurred angular range
  float distance    = length(p - center);
  float angle       = atan(p.y - center.y, p.x - center.x);
  
  // If radius > 0, we assume that the blurred length is proportional to (distance - radius);
  float dist_       = max(distance - radius, 0.0);
  float blurLen     = radians(max(blur, 0.0)) * dist_;
  
  // The actual blurring angle is then found as (blurLen_ / distance)
  float blur_       = blurLen / max(distance, 0.01);            // Jump the singularity
  
  vec2 angleRange   = vec2(angle - blur_, angle + blur_);       // Couldn't make it an array with
                                                                // explicit initialization... GLSL complained -.-
  // Include the points at angleRange's extremes
  addPoint(rect, center + distance * vec2(cos(angleRange.x), sin(angleRange.x)));
  addPoint(rect, center + distance * vec2(cos(angleRange.y), sin(angleRange.y)));
  
  // At pi/2 multiples we get a box extreme. Include them if present.
  float blur_twice = 2.0 * blur_;
  
  if(mod( - angleRange.x, pi_twice) < blur_twice)
    addPoint(rect, center + vec2(distance, 0.0));
  if(mod(pi_half - angleRange.x, pi_twice) < blur_twice)
    addPoint(rect, center + vec2(0.0, distance));
  if(mod(pi - angleRange.x, pi_twice) < blur_twice)
    addPoint(rect, center + vec2(-distance, 0.0));
  if(mod(-pi_half - angleRange.x, pi_twice) < blur_twice)
    addPoint(rect, center + vec2(0.0, -distance));
}

void main( void )
{
  outputBBox = inputBBox[0];

  if(outputBBox != infiniteRect)
  {
    // Add the bounding box of each blurred corner
    addBlurredPointBox(outputBBox, inputBBox[0].xy);
    addBlurredPointBox(outputBBox, inputBBox[0].xw);
    addBlurredPointBox(outputBBox, inputBBox[0].zy);
    addBlurredPointBox(outputBBox, inputBBox[0].zw);
  }
  
  gl_Position = vec4(0.0);                            // Does not link without
}
