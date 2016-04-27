#ifdef GL_ES
precision mediump float;
#endif

// Tweaked from  http://glsl.heroku.com/e#6051.0


// Posted by Trisomie21 : 2D noise experiment (pan/zoom)
//
// failed attempt at faking caustics
//


uniform mat3 outputToWorld;

uniform vec4  color;
uniform float time;


vec4 textureRND2D(vec2 uv){
	uv = floor(uv);
	float v = uv.x+uv.y*1e3;

  // Build space-specific corner values
  vec4 res = fract(1e5*sin(vec4(v*1e-2, (v+1.)*1e-2, (v+1e3)*1e-2, (v+1e3+1.)*1e-2)));

  // Add 'sawtooth-like'  wavefronts evolution
  return 2.0 * abs(fract(res + vec4(time * .03)) - 0.5);
}

float noise(vec2 p) {
  vec4 r = textureRND2D(p);                           // Noise values at cell corners
  
  vec2 f = fract(p);
	f = f*f*(3.0-2.0*f);                                // aka the smoothstep() builtin function
  
	return (mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y));
}

float buildColor(vec2 p) {
  p += noise(p);                                      // Noising p itself first. This helps
                                                      // preventing grid-like patterns.
  
	float v = 1.0 - abs(pow(abs(noise(p) - 0.5), 0.75)) * 1.7;  // Lots of magical constants  o_o?
	return v;
}


const float SPEED   = .15;

void main( void ) {
	vec2 p = (outputToWorld * vec3(gl_FragCoord.xy, 1.0)).xy;
  
	float c1 = buildColor(p*.03 + time * SPEED);
	float c2 = buildColor(p*.03 - time * SPEED);
	
	float c3 = buildColor(p*.02 - time * SPEED);
	float c4 = buildColor(p*.02 + time * SPEED);
	
	float cf = pow(c1*c2*c3*c4+0.5,6.);                 // Yep this is bad. Explicitly
                                                      // dependent on the 4 above. Better?
	vec3 c = vec3(cf);
	gl_FragColor = vec4(c, 0.0) + color;
  
  gl_FragColor.rgb *= gl_FragColor.a;                 // Premultiplication
}
