#ifdef GL_ES
precision mediump float;
#endif

// Tweaked from  http://glsl.heroku.com/e#6015.0


// Posted by Trisomie21


uniform mat3 outputToWorld;

uniform vec4  color;
uniform float time;
uniform float brightness;

// Tweaked from  http://glsl.heroku.com/e#4982.0
float hash( float n ) { return fract(sin(n)*43758.5453); }
float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }

float noise( in vec2 x )
{
	vec2 p = floor(x);
	vec2 f = fract(x);
    	f = f*f*(3.0-2.0*f);
    	float n = p.x + p.y*57.0;
    	float res = mix(mix(hash(n+0.0), hash(n+1.0),f.x), mix(hash(n+57.0), hash(n+58.0),f.x),f.y);
    	return res;
}

vec3 cloud(vec2 p) {
	float f = 0.0;
    	f += 0.50000*noise(p*1.0*10.0); 
    	f += 0.25000*noise(p*2.0*10.0); 
    	f += 0.12500*noise(p*4.0*10.0); 
    	f += 0.06250*noise(p*8.0*10.0);	
	f *= f;
  
	return color.rgb * color.a * f * .6;
}

const float SPEED       = 0.01;
const float DENSITY	    = 1.5;

void main( void )
{	
	vec2 pos = .01 * (outputToWorld * vec3(gl_FragCoord.xy, 1.0)).xy;
  
	// Nebulous cloud - It's intended as background, ie it doesn't block stars visibility.
  // Stars ADD to this. 
	vec3 color = cloud(pos);
	
	// Stars Field - this is the idea: each star is drawn in a 'star cell' which results from
  // FLOORING a point function p(x,y) of the pixel coordinates. A cell's edges correspond to
  // coordinated lines of the form: p_x(x,y) = int, p_y(x, y) = int.
  
  // The problem lies in finding a function p which is suitable, ie p_x's and p_y's gradients should
  // be as orthogonal and with finite strictly positive norm as possible.
  
  // Changing to polar coordinates is simplest - when the radius (distance from origin)
  // is high, moving in radius and arc distance is almost orthogonal. Plus, star 'discs' are harder
  // to spot than star 'rows', since they are curved.
  
  // I think that a suitable deformation of the identity grid based on sin and cos exists,
  // but couldn't find it...  ^.^'
  
  float dist = length(pos);
	vec2 coord = vec2(dist, atan(pos.y, pos.x)/* / (3.1415926*2.0)*/);    // Pseudo-polar coordinates
  
	vec2  p = 40.0 * vec2(coord.x,                      // radius
    floor(coord.x + 1.0) * coord.y +                  // arc distance (floor helps stabilizing cell shapes, and 1.0 to avoid flooring to 0)
    hash(floor(40.0 * coord.x)));                     // shifts the star 'discs' along the arc, by a pseudo-random value (helps avoiding 'star rows', at least along the radial direction)
  
	vec2 uv = 2.0 * fract(p) - 1.0;                     // Pixel position in the cell, in [-1,1]^2 coordinates
  
  float cellValue      = abs(2.0 * fract(rand(floor(p)) + SPEED * time) - 1.0);
	float cellBrightness = clamp((cellValue - 0.9) * brightness * 10.0, 0.0, 1.0);

	color +=  clamp(
    (1.0 - 2.0 * length(uv)) *                                            // Comment this line to see the star cells
    cellBrightness, 0.0, 1.0);


  gl_FragColor = vec4(color, 1.0);
}
