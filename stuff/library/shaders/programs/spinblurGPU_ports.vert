#ifdef GL_ES
precision mediump float;
#endif

uniform mat3 worldToOutput;

uniform vec4 outputRect;

varying vec4 inputRect[1];
varying mat3 worldToInput[1];

uniform vec2  center;
uniform float radius;
uniform float blur;


float det(mat3 m) { return m[0][0] * m[1][1] - m[0][1] * m[1][0]; }


float scale     = sqrt(abs(det(worldToOutput)));
  
vec2 center_    = (worldToOutput * vec3(center, 1.0)).xy;
float rad_      = scale * max(radius, 0.0);

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
  float distance    = length(p - center_);
  float angle       = atan(p.y - center_.y, p.x - center_.x);
  
  // If rad_ > 0, we assume that the blurred length is proportional to (distance - rad_);
  float dist_       = max(distance - rad_, 0.0);
  float blurLen     = radians(max(blur, 0.0)) * dist_;
  
  // The actual blurring angle is then found as (blurLen_ / distance)
  float blur_       = blurLen / max(distance, 0.01);            // Jump the singularity
  
  vec2 angleRange   = vec2(angle - blur_, angle + blur_);       // Couldn't make it an array with
                                                                // explicit initialization... GLSL complained -.-
  // Include the points at angleRange's extremes
  addPoint(rect, center_ + distance * vec2(cos(angleRange.x), sin(angleRange.x)));
  addPoint(rect, center_ + distance * vec2(cos(angleRange.y), sin(angleRange.y)));
  
  // At pi/2 multiples we get a box extreme. Include them if present.
  float blur_twice = 2.0 * blur_;
  
  if(mod( - angleRange.x, pi_twice) < blur_twice)
    addPoint(rect, center_ + vec2(distance, 0.0));
  if(mod(pi_half - angleRange.x, pi_twice) < blur_twice)
    addPoint(rect, center_ + vec2(0.0, distance));
  if(mod(pi - angleRange.x, pi_twice) < blur_twice)
    addPoint(rect, center_ + vec2(-distance, 0.0));
  if(mod(-pi_half - angleRange.x, pi_twice) < blur_twice)
    addPoint(rect, center_ + vec2(0.0, -distance));
}

void main( void )
{
  worldToInput[0] = worldToOutput;                    // Let the input and output references be the same
  inputRect[0]    = outputRect;

  // Add the bounding box of each blurred corner
  addBlurredPointBox(inputRect[0], outputRect.xy);
  addBlurredPointBox(inputRect[0], outputRect.xw);
  addBlurredPointBox(inputRect[0], outputRect.zy);
  addBlurredPointBox(inputRect[0], outputRect.zw);
  
  gl_Position = vec4(0.0);                            // Does not link without
}
