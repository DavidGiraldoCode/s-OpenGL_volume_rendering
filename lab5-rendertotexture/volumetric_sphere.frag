#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

uniform vec3	sphere_center						= vec3(0,0,0);// 25,0,-25 (20, 5,-40);
uniform float	sphere_radius						= 50.0;
uniform float	density								= 1.0;
uniform mat4	inverse_view_projection_matrix      = mat4(0);
uniform mat4	view_projection_matrix				= mat4(0);
uniform int		width								= 1;//1280;
uniform int		height								= 1;//720;
uniform vec2	normalize_factors					= vec2(1,1);
uniform vec3	camera_position						= vec3(0,0,0);

layout(binding = 0) uniform sampler2D sceneColorTexture;
layout(binding = 1) uniform sampler2D sceneDepthTexture;

layout(location = 0) out vec4 fragmentColor;

//https://www.geeks3d.com/20091216/geexlab-how-to-visualize-the-depth-buffer-in-glsl/
float LinearizeDepth(vec2 uv)
{
  float n = 15.0; // camera z near
  float f = 1000.0; // camera z far
  float z = texture2D(sceneDepthTexture, uv).x;
  return (2.0 * n) / (f + n - z * (f - n));	
}

bool hit(   vec3 origin, 
            vec3 direction, 
            vec3 sphere_center, 
            float radius, 
            out float t0, 
            out float t1)
{   
    vec3 sc_to_orign = origin.xyz - sphere_center.xyz;
    float a = dot(direction, direction);
    float h = dot(direction, sc_to_orign);
    float c = dot(sc_to_orign, sc_to_orign) - (radius * radius);

    float discriminant = (h * h) - (a * c);
    float sqr = sqrt(discriminant);

    t0 = ((h * -1.0) - sqr) / a;
    t1 = ((h * -1.0) + sqr) / a;
    
    if(discriminant < 0.0) return false;

    return true;
}

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

	float t0, t1;
	float d = LinearizeDepth(fagUV);
	vec4 color = vec4(1);

	if(hit(camera_position, ray_direction, sphere_center, sphere_radius, t0, t1))
	{
		vec4 sceneColor = texture(sceneColorTexture, fagUV);
		vec3 t0_position = camera_position + ray_direction * t0;
		vec3 t1_position = camera_position + ray_direction * t1;

		vec4 t0_clipSpace = view_projection_matrix * vec4(t0_position.xyz,1);

		float ndc_z = t0_clipSpace.z / t0_clipSpace.w;

		float n = 15.0; // camera z near
		float f = 1000.0; // camera z far
		float linearNDC_z = (2.0 * n) / (f + n - ndc_z * (f - n));

		if(linearNDC_z > d)
		{
			color = texture(sceneColorTexture, fagUV);
			
			//color = vec4(d,d,d,1);
		}
		else if(t0 < t1)
		{
			//color = vec4(linearNDC_z, linearNDC_z, linearNDC_z, 1 );
			float world_space_lenght = length(t1_position - t0_position);
			float optical_depth =  world_space_lenght / (sphere_radius * 2.0);
			float transmittance = exp(-optical_depth * density);
			transmittance = transmittance > 1.0 ? 1.0 : transmittance;

			vec4 volume_albedo = vec4(1,1,1,1);
			float opacity = 1.0 - (transmittance * transmittance);
			//color = vec4(transmittance, transmittance, transmittance, 1 );

			//color.rgb = (sceneColor.rgb * (1.0 - transmittance)) + (transmittance * volume_albedo.rgb);
			//color.a = (sceneColor.a * (1.0 - transmittance)) + (transmittance * volume_albedo.a);
			color = (sceneColor * (1.0 - opacity)) + (opacity * volume_albedo);
		}
		vec3 normal = normalize(t0_position - sphere_center);
		vec3 light  = vec3(0,1,0); 
		float lambert = max(0, dot(light, normal));

		//color = vec4(1, 1, 1, 0.5 ) * lambert;
	}
	else
	{
		
		//color = vec4(d,d,d,1);
		color = texture(sceneColorTexture, fagUV);
	}

	fragmentColor = color; 
	//return;
	//fragmentColor = vec4(d,d,d,1);
	//fragmentColor = vec4(fagUV.xy, 0.0, 1.0);
	//fragmentColor = vec4(normalized_device_coords.xy, 0.0, 1.0);
	//fragmentColor = vec4(fragVS.xy, 0.0, 1.0);
	//fragmentColor = vec4(ray_direction.xy, 0.0, 1.0);
	//fragmentColor = color;
	//texture(sceneColorTexture, fagUV);

}
