#ifdef GL_ES
precision mediump float;
#endif


uniform mat3      worldToOutput;

uniform sampler2D inputImage[1];
uniform mat3      outputToInput[1];

uniform vec2  center;
uniform float radius;
uniform float blur;


float det(mat3 m) { return m[0][0] * m[1][1] - m[0][1] * m[1][0]; }


mat3 worldToInput = outputToInput[0] * worldToOutput;

vec2 center_s     = (worldToOutput * vec3(center, 1.0)).xy;
float scale_s     = sqrt(abs(det(worldToOutput)));
float rad_s       = scale_s * max(radius, 0.0);


#define STEPS_PER_PIXEL   4.0


void main( void )
{
  // Build lengths on output metrics
  vec2  v           = gl_FragCoord.xy - center_s;
  float vLength     = length(v);

  float dist_s      = max(vLength - rad_s, 0.0);
  float b_s         = blur * dist_s;
  
  // Putting a maximum samples count - to prevent freezes; besides, blurring too many
  // pixels is typically useless...
  int samplesCount  = int(clamp(ceil(b_s * STEPS_PER_PIXEL), 1.0, 2000.0));
  float step_s      = b_s / float(samplesCount);
  
  
  // Perform filtering
  vec2 texPos       = (outputToInput[0] * vec3(gl_FragCoord.xy, 1.0)).xy;
  vec4 pix          = texture2D(inputImage[0], texPos);
  
  vec2 vStep = v * (step_s / max(vLength, 0.01));
  vStep      = (outputToInput[0] * vec3(vStep, 0.0)).xy;
  
  vec2 tPos0 = texPos + vStep;
  vec2 tPos1 = texPos - vStep;
  
  for(int s = 1; s < samplesCount; ++s)
  {
    pix += texture2D(inputImage[0], tPos0);
    pix += texture2D(inputImage[0], tPos1);
  
    tPos0 += vStep, tPos1 -= vStep;
  }

  gl_FragColor = pix / float(2 * samplesCount - 1);
}
