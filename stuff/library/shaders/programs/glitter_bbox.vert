#ifdef GL_ES
precision mediump float;
#endif

uniform vec4 infiniteRect;
uniform vec4 inputBBox[1];

varying vec4 outputBBox;

uniform float radius;


void main( void )
{
  if(inputBBox[0] == infiniteRect)                    // Better avoid enlarging the infinite 
    outputBBox = infiniteRect;                        // rect...
  else
    outputBBox = vec4(
      inputBBox[0].x - radius,
      inputBBox[0].y - radius,
      inputBBox[0].z + radius,
      inputBBox[0].w + radius);
  
  gl_Position = vec4(0.0);                            // Does not link without
}
