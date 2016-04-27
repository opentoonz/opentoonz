#ifdef GL_ES
precision mediump float;
#endif

uniform vec4 infiniteRect;
uniform vec4 inputBBox[1];

varying vec4 outputBBox;

uniform vec2  center;
uniform float radius;
uniform float blur;


void addPoint(inout vec4 rect, vec2 p) {
  rect.xy = min(rect.xy, p);
  rect.zw = max(rect.zw, p);
}

void addBlurredPointBox(inout vec4 rect, vec2 p)
{
  vec2 v        = p - center;
  float vLength = length(v);

  float dist    = max(length(v) - radius, 0.0);
  float b       = blur * dist;

  v *= (b / max(vLength, 0.01));
  
  addPoint(rect, p - v);
  addPoint(rect, p + v);
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
