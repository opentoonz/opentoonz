#version 130

#ifdef GL_ES
precision mediump float;
#endif

uniform mat3 worldToOutput;

uniform sampler2D inputImage[1];
uniform mat3      outputToInput[1];

uniform float u_sigmaR; // Standard deviation
uniform float u_sigmaD; // Intensity difference

// -----------------------------------------------------------------------------

// Normal distribution function (Gaussian)
float normpdf(in float x, in float sigma)
{
	return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
}

// Bilateral filter (Gaussian blur)
vec4 bilateralFilter(sampler2D tex, vec2 uv, vec2 resolution)
{
  vec4 color = vec4(0.0);
  float Z = 0.0;

  /* Calculate the size of the kernel based on the standard deviation and
     clamp to 50 to avoid excessive compute. */
  int kernelSize = int(min(ceil(8 * u_sigmaD), 50));

  // Calculate the adjusted kernel size based on the current pixel's position
  int adjustedKernelSizeX = min(kernelSize, int(min(uv.x * resolution.x, (1.0 - uv.x) * resolution.x)));
  int adjustedKernelSizeY = min(kernelSize, int(min(uv.y * resolution.y, (1.0 - uv.y) * resolution.y)));

  for (int i = -adjustedKernelSizeX; i <= adjustedKernelSizeX; ++i) {
    for (int j = -adjustedKernelSizeY; j <= adjustedKernelSizeY; ++j) {
      vec2 offset     = vec2(float(i), float(j)) / resolution;
      vec4 texSample  = texture2D(tex, uv + offset);

      float kernelSigma_d   = u_sigmaD;
      float kernelSigma_r   = u_sigmaR;
      float intensityDiff   = length(texSample.rgb - texture2D(tex, uv).rgb);
      float spatialGaussian = normpdf(length(vec2(float(i), float(j))), kernelSigma_d);
      float rangeGaussian   = normpdf(intensityDiff, kernelSigma_r);
      float kernelValue     = spatialGaussian * rangeGaussian;

      color += kernelValue * texSample;
      Z += kernelValue;
    }
  }
  return color / Z;
}

// -----------------------------------------------------------------------------

void main( void )
{
  vec2 resolution = vec2(textureSize(inputImage[0], 0));
  vec2 texCoord   = (outputToInput[0] * vec3(gl_FragCoord.xy, 1.0)).xy;
  vec2 texelSize = vec2(1.0) / vec2(textureSize(inputImage[0], 0));

  // Colors
  vec4 color = texture2D(inputImage[0], texCoord);
  color = bilateralFilter(inputImage[0], texCoord, resolution);

  // DEBUG COLOR: Use this to test values, the color will be shown on the screen.
  vec4 debugColor = vec4(0.0);
  if (resolution.x < 1920.0) {
    debugColor = vec4(1.0, 0.0, 0.0, 1.0); // Red
  } else {
    debugColor = vec4(0.0, 1.0, 0.0, 1.0); // Green
  }

  // Output
  gl_FragColor = color;

  // Premultiply alpha (is this necessary?)
  gl_FragColor.rgb *= gl_FragColor.a;
}
