#ifdef GL_ES
precision mediump float;
#endif

// Tweaked from  http://glsl.heroku.com/e#5941.2

//
// Description : Array and textureless GLSL 2D/3D/4D 
//               noise functions with wrapping
//      Author : People
//  Maintainer : Anyone
//     Lastmod : 20130111 (davidwparker)
//     License : No Copyright No rights reserved.
//               Freely distributed
//


uniform mat3  outputToWorld;

uniform vec4  color1;
uniform vec4  color2;
uniform float detail;
uniform float time;

const float pi_twice = 6.283185307;


float snoise(vec3 uv, float res)
{
	const vec3 s = vec3(1e0, 1e2, 1e4);
	
	uv *= res;
	
	vec3 uv0 = floor(mod(uv, res))*s;
	vec3 uv1 = floor(mod(uv+vec3(1.), res))*s;
	
	vec3 f = fract(uv);
	f = f*f*(3.0-2.0*f);

	vec4 v = vec4(uv0.x+uv0.y+uv0.z, uv1.x+uv0.y+uv0.z,
		      uv0.x+uv1.y+uv0.z, uv1.x+uv1.y+uv0.z);

	vec4 r = fract(sin(v*1e-3)*1e5);
	float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
	
	r = fract(sin((v + uv1.z - uv0.z)*1e-3)*1e5);
	float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
	
	return  2.0 * mix(r0, r1, f.z) - 1.0;               // Range in [-1, 1]
}

void main(void) 
{
  vec2 p = .002 * (outputToWorld * vec3(gl_FragCoord.xy, 1.0)).xy;
  
  float color   = 3.0 * (1.0 - 2.0 * length(p));
	vec3  coord   = vec3(atan(p.y, p.x) / pi_twice, length(p) * 0.4, 0.0);
	
	for(int i = 1; i <= 7; i++)
	{
		float power   = pow(2.0, float(i));
		vec3  timed   = vec3(0.0, - time*.02, time*.01);
    
		color        += 1.5 * snoise(coord + timed, power * detail) / power;
	}
  
  color = max(color, 0.);


  // ORIGINAL:
	//gl_FragColor = vec4( color, pow(max(color,0.),2.)*0.4, pow(max(color,0.),3.)*0.15 , 1.0);
  
  vec4 col1 = color1 * color1.a, col2 = color2 * color2.a;
  
  gl_FragColor    = mix(col1, col2, color / 3.0);
  gl_FragColor.a *= smoothstep(0.0, 1.0, color);

  gl_FragColor.rgb *= gl_FragColor.a;                 // Premultiplication
}

