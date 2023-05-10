#ifdef GL_ES
precision mediump float;
#endif

uniform mat3 worldToOutput;
uniform vec4 outputRect;

varying vec4 inputRect[2];
varying mat3 worldToInput[2];

void main( void ) {
  // Let the input and output references be the same
  worldToInput[0] = worldToOutput;
  inputRect[0] = outputRect;

  // Does not link without this line
  gl_Position = vec4(0.0);
}
