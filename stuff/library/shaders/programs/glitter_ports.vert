#ifdef GL_ES
precision mediump float;
#endif

uniform mat3 worldToOutput;

uniform vec4 outputRect;
varying vec4 inputRect[1];
varying mat3 worldToInput[1];

uniform float radius;


float det(mat3 m) { return m[0][0] * m[1][1] - m[0][1] * m[1][0]; }

void main( void )
{
  float rad  = radius * sqrt(abs(det(worldToOutput)));

  worldToInput[0] = worldToOutput;                    // Let the input and output references
                                                      // be the same
  inputRect[0] = vec4(
    outputRect.x - rad,
    outputRect.y - rad,
    outputRect.z + rad,
    outputRect.w + rad);
  
  gl_Position = vec4(0.0);                            // Does not link without
}
