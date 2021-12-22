#ifdef GL_ES
precision mediump float;
#endif

uniform mat3 worldToOutput;
uniform vec4 outputRect;

varying vec4 inputRect[2];
varying mat3 worldToInput[2];


void main( void )
{
  // Let the input and output references be the same
  worldToInput[0] = worldToOutput;
  worldToInput[1] = worldToOutput;
  inputRect[0]    = outputRect;
  inputRect[1]    = outputRect;

  gl_Position = vec4(0.0);                            // Does not link without
}
