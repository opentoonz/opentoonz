#ifdef GL_ES
precision mediump float;
#endif

uniform vec4 infiniteRect;
uniform vec4 inputBBox[1];

varying vec4 outputBBox;

void main( void ) {
  if(inputBBox[0] == infiniteRect) // Avoid enlarging infinite rect
    outputBBox = infiniteRect;
  else
    outputBBox = inputBBox[0];
  
  gl_Position = vec4(0.0); // Does not link without
}
