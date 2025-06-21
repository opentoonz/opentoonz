#ifdef GL_ES
precision mediump float;
#endif

uniform mat3 worldToOutput;
uniform sampler2D inputImage[2];
uniform mat3 outputToInput[2];

uniform bool bhue;    // Blend hue component
uniform bool bsat;    // Blend saturation component  
uniform bool blum;    // Blend luminosity component
uniform float balpha; // Blend opacity
uniform bool bmask;   // Use base as mask

// ---------------------------
// Blending calculations from:
// https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_blend_equation_advanced.txt

float getMinChannel(vec3 color) {
    return min(min(color.r, color.g), color.b);
}

float getMaxChannel(vec3 color) {
    return max(max(color.r, color.g), color.b);
}

float getLuminance(vec3 color) {
    return dot(color, vec3(0.30, 0.59, 0.11));
}

float getSaturation(vec3 color) {
    return getMaxChannel(color) - getMinChannel(color);
}

vec3 clipColor(vec3 color) {
    float lum = getLuminance(color);
    float minChannel = getMinChannel(color);
    float maxChannel = getMaxChannel(color);
    
    if (minChannel < 0.0) {
        color = lum + ((color - lum) * lum) / (lum - minChannel);
    }
    if (maxChannel > 1.0) {
        color = lum + ((color - lum) * (1.0 - lum)) / (maxChannel - lum);
    }
    return color;
}

vec3 setLuminance(vec3 baseColor, vec3 lumSource) {
    float baseLum = getLuminance(baseColor);
    float targetLum = getLuminance(lumSource);
    vec3 result = baseColor + vec3(targetLum - baseLum);
    return clipColor(result);
}

vec3 setLuminanceAndSaturation(vec3 baseColor, vec3 satSource, vec3 lumSource) {
    float baseMin = getMinChannel(baseColor);
    float baseSat = getSaturation(baseColor);
    float targetSat = getSaturation(satSource);
    
    vec3 result;
    if (baseSat > 0.0) {
        result = (baseColor - baseMin) * targetSat / baseSat;
    } else {
        result = vec3(0.0);
    }
    return setLuminance(result, lumSource);
}

// Detects OpenToonz empty frame artifacts: black RGB with non-zero alpha
// Returns true when content is effectively empty (RGB channels below threshold)
bool isEffectivelyEmpty(vec4 color) {
    float maxChannel = max(max(color.r, color.g), color.b);
    return maxChannel < 0.001;
}

vec3 depremultiply(vec4 color) {
    return color.a > 0.0 ? color.rgb / color.a : vec3(0.0);
}

void main(void) {
    vec2 fgTexPos = (outputToInput[0] * vec3(gl_FragCoord.xy, 1.0)).xy;
    vec2 bgTexPos = (outputToInput[1] * vec3(gl_FragCoord.xy, 1.0)).xy;
    vec4 fgColor = texture2D(inputImage[0], fgTexPos);
    vec4 bgColor = texture2D(inputImage[1], bgTexPos);
    
    // Check for empty content using RGB analysis
    bool fgIsEmpty = isEffectivelyEmpty(fgColor);
    bool bgIsEmpty = isEffectivelyEmpty(bgColor);
    
    if (fgIsEmpty && bgIsEmpty) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    
    // Filter out empty content
    vec4 effectiveFg = fgIsEmpty ? vec4(0.0, 0.0, 0.0, 0.0) : fgColor;
    vec4 effectiveBg = bgIsEmpty ? vec4(0.0, 0.0, 0.0, 0.0) : bgColor;
    
    float fgAlpha = effectiveFg.a * balpha;
    float bgAlpha = effectiveBg.a;
    
    // Handle pure transparency cases
    if (fgAlpha <= 0.0 && bgAlpha <= 0.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    
    if (bgAlpha <= 0.0) {
        if (bmask) {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        } else {
            gl_FragColor = vec4(effectiveFg.rgb * balpha, fgAlpha);
        }
        return;
    }
    
    if (fgAlpha <= 0.0) {
        gl_FragColor = effectiveBg;
        return;
    }
    
    // Depremultiply colors for blending
    vec3 fgRGB = depremultiply(effectiveFg);
    vec3 bgRGB = depremultiply(effectiveBg);
    
    // Calculate output alpha
    float outputAlpha = bmask ? bgAlpha : bgAlpha + fgAlpha * (1.0 - bgAlpha);
    
    // Perform HSL blending
    vec3 blendedRGB = setLuminanceAndSaturation(
        bhue ? fgRGB : bgRGB,    // Hue source
        bsat ? fgRGB : bgRGB,    // Saturation source
        blum ? fgRGB : bgRGB     // Luminance source
    );
    
    vec3 maskRGB = bmask ? vec3(0.0) : fgRGB;
    vec3 finalRGB = bgRGB * bgAlpha * (1.0 - fgAlpha) + 
                    mix(maskRGB, blendedRGB, bgAlpha) * fgAlpha;
    gl_FragColor = vec4(finalRGB * outputAlpha, outputAlpha);
}
