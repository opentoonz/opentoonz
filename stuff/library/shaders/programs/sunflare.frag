#ifdef GL_ES
precision mediump float;
#endif

uniform mat3  outputToWorld;

uniform vec4  color;
uniform int   blades;
uniform float intensity;
uniform float angle;
uniform float bias;
uniform float sharpness;

float blades_ = float(blades);
float angle_  = radians(angle);
float bias_   = .01 * bias;


// never watch into the sun ;)

void main( void )
{
  vec2 p = .03 * (outputToWorld * vec3(gl_FragCoord.xy, 1.0)).xy;

  float a = atan(p.y, p.x) - angle_;
	float blade  = intensity * clamp(pow(sin(a * blades_) + bias_, sharpness), 0.0, 1.0);

  gl_FragColor = vec4(color.rgb * color.a, color.a);            // Premultiplication
  gl_FragColor = gl_FragColor * (1.0 + blade) / length(p);
}

