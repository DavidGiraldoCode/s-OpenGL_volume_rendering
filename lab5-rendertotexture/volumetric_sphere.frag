#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

uniform int slice = 0;
int change = 0;

layout(binding = 0) uniform sampler3D frameBufferNoiseTexture;

layout(location = 0) out vec4 fragmentColor;

void main()
{
	change = 128 - slice;

	if(change == 0)
		change = 1;

	fragmentColor = vec4( (1.0/ float(change)), 0.0, 0.0, 1.0);
}
