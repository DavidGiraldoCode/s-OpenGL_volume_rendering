#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

uniform vec3	sphere_center						= vec3(0,0,0);
uniform float	sphere_radius						= 10.0;
uniform mat4	inverse_view_projection_matrix      = mat4(0);
uniform int		width								= 1;//1280;
uniform int		height								= 1;//720;
uniform vec2	normalize_factors					= vec2(1,1);
uniform vec3	camera_position						= vec3(0,0,0);

layout(location = 0) out vec4 fragmentColor;

void main()
{
	vec2 extend = vec2(width, height);
	vec2 fagUV;//normalized_device_coords;
	
	fagUV.x = gl_FragCoord.x * normalize_factors.x;
	fagUV.y = gl_FragCoord.y * normalize_factors.y;

	mat4 ndcMatrix;          
	ndcMatrix[0].xyzw = vec4( 2,  0, 0, 0); // X column
	ndcMatrix[1].xyzw = vec4( 0,  2, 0, 0); // Y column
	ndcMatrix[2].xyzw = vec4( 0,  0, 1, 0); // Y column 
	ndcMatrix[3].xyzw = vec4(-1, -1, 0, 1); // T column
	vec4 uvHomCoord = vec4(fagUV.xy, 0.0, 1.0);

	vec4 normalized_device_coords = ndcMatrix * uvHomCoord;
	vec4 positionClipSpace = vec4(normalized_device_coords.xy, -1.0, 1.0);

	vec4 posistionWorldSpace = inverse_view_projection_matrix * positionClipSpace;
	posistionWorldSpace /= posistionWorldSpace.w; // Perspective division.

	//vec3 fragVS = (inverse_view_projection_matrix * vec4(normalized_device_coords.xy, 0.0, 1.0)).xyz;
	vec4 fragVS = inverse_view_projection_matrix * normalized_device_coords;

	//vec3 ray_direction =normalize(fragVS.xyz - camera_position.xyz).xyz;
	vec3 ray_direction =normalize(posistionWorldSpace.xyz - camera_position.xyz).xyz;

	//fragmentColor = vec4(fagUV.xy, 0.0, 1.0);
	//fragmentColor = vec4(normalized_device_coords.xy, 0.0, 1.0);
	//fragmentColor = vec4(fragVS.xy, 0.0, 1.0);
	fragmentColor = vec4(ray_direction.xy, 0.0, 1.0);
}
