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
  
vec2  center_   = (worldToOutput * vec3(center, 1.0)).xy;
float rad_      = scale * max(radius, 0.0);


void addPoint(inout vec4 rect, vec2 p) {
  rect.xy = min(rect.xy, p);
  rect.zw = max(rect.zw, p);
}

void addBlurredPointBox(inout vec4 rect, vec2 p)
{
  vec2 v        = p - center_;
  float vLength = length(v);

  float dist    = max(length(v) - rad_, 0.0);
  float b       = blur * dist;

  v *= (b / max(vLength, 0.01));
  
  addPoint(rect, p - v);
  addPoint(rect, p + v);
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
