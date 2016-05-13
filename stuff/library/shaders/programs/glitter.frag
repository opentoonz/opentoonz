#ifdef GL_ES
precision mediump float;
#endif


uniform mat3      worldToOutput;

uniform sampler2D inputImage[1];
uniform mat3      outputToInput[1];

mat3              worldToInput = outputToInput[0] * worldToOutput;


uniform float threshold;
uniform float brightness;
uniform float radius;
uniform float angle;
uniform float halo;


float det(mat3 m) { return m[0][0] * m[1][1] - m[0][1] * m[1][0]; }

float scale       = sqrt(abs(det(worldToOutput)));


float angle_      = radians(angle);
float sin_        = sin(angle_);
float cos_        = cos(angle_);
float threshold_  = 1.0 - 0.01 * threshold;

float rad_        = radius * scale;

const vec3 lVec   = vec3(0.298980712, 0.587036132, 0.113983154);

#define STEPS_PER_PIXEL  4.0

float stepsCount = ceil(STEPS_PER_PIXEL * rad_);

float halo_       = 0.01 * (halo + 1.0) * stepsCount;


float rayWeight(float s)
{
  s /= halo_;
  return clamp(1.0 - s * s, 0.0, 1.0);
}

vec4 lightValue(const vec2 texCoord)
{
  vec4 col = texture2D(inputImage[0], texCoord);
  float l = dot(lVec, col.rgb);

  return smoothstep(threshold_, 1.0, l) * col;
}

bool filterLine(inout vec4 col, const vec2 p, const vec2 dx, float s_y)
{
  float rw  = rayWeight(s_y);
  if(rw == 0.0)
    return false;
  
  float dw  = max(1.0 - s_y / stepsCount, 0.0);
  
  col      += dw * rw * lightValue(p);
  
  vec2 s    = vec2(0.0, s_y);
  
  for(s.x = 1.0; s.x < stepsCount; s.x += 1.0)
  {
    dw = max(1.0 - length(s) / stepsCount, 0.0);
    col += rw * dw * (
      lightValue(p + s.x * dx) +
      lightValue(p - s.x * dx));
  }
  
  return true;
}

void main( void )
{
  vec2 texCoord   = (outputToInput[0] * vec3(gl_FragCoord.xy, 1.0)).xy;

  float step      = radius / stepsCount;
  mat2  transf    =
    mat2((worldToInput * vec3(1.0, 0.0, 0.0)).xy,
         (worldToInput * vec3(0.0, 1.0, 0.0)).xy) *             // worldToInput without translational part
    mat2(cos_, sin_, -sin_, cos_) *                             // angle shift by uniform parameter
    step;                                                       // [-stepsCount,stepsCount]^2 to [-radius, radius]^2


  // Filter lines in the 2 orthogonal directions
  vec4 addCol = vec4(0.0);
  
  // Horizontal
  filterLine(addCol, texCoord, transf[0], 0.0);

  for(float s = 1.0; s < stepsCount; s += 1.0)
  {
    if(!filterLine(addCol, texCoord + s * transf[1], transf[0], s))
      break;

    filterLine(addCol, texCoord - s * transf[1], transf[0], s);
  }
  
  // Vertical
  filterLine(addCol, texCoord, transf[1], 0.0);

  for(float s = 1.0; s < stepsCount; s += 1.0)
  {
    if(!filterLine(addCol, texCoord + s * transf[0], transf[1], s))
      break;

    filterLine(addCol, texCoord - s * transf[0], transf[1], s);
  }

  
  float weight = stepsCount * STEPS_PER_PIXEL;
  
  vec4 col     = texture2D(inputImage[0], texCoord);
  gl_FragColor = col + addCol * (brightness / weight);
}
