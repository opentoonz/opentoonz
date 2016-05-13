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
  float distance_s  = length(v);
  float angle       = atan(v.y, v.x);

  float dist_s      = max(distance_s - rad_s, 0.0);
  float blurLen_s   = radians(max(blur, 0.0)) * dist_s;
  
  float blur_       = blurLen_s / max(distance_s, 0.01);        // Jump the singularity
  
  // Putting a maximum samples count - to prevent freezes; besides, blurring too many
  // pixels is typically useless...
  int samplesCount = int(clamp(ceil(blurLen_s * STEPS_PER_PIXEL), 1.0, 2000.0));

  
  float angle_step  = blur_ / float(samplesCount);
  
  float cos_step    = cos(angle_step);
  float sin_step    = sin(angle_step);
  
  mat2 rot_step0    = mat2(cos_step, sin_step, -sin_step, cos_step);
  mat2 rot_step1    = mat2(cos_step, -sin_step, sin_step, cos_step);
  
  
  // Perform filtering
  vec4 pix          = texture2D(inputImage[0], (outputToInput[0] * vec3(gl_FragCoord.xy, 1.0)).xy);
  
  vec2 v0 = rot_step0 * v, v1 = rot_step1 * v;
  
  for(int s = 1; s < samplesCount; ++s)
  {
    pix += texture2D(inputImage[0], (outputToInput[0] * vec3(center_s + v0, 1.0)).xy);
    pix += texture2D(inputImage[0], (outputToInput[0] * vec3(center_s + v1, 1.0)).xy);
  
    v0 = rot_step0 * v0, v1 = rot_step1 * v1;
  }

  gl_FragColor = pix / float(2 * samplesCount - 1);
}
