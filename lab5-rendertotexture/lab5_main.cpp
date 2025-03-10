////////////////////////////////////////////////////////////////////////////
// This codesource has RenderDoc markers for debugging OpenGL calls. Read
// the session notes to know more.
// Has am implementation of ray-marching volume rendering.
////////////////////////////////////////////////////////////////////////////


#include <GL/glew.h>

// STB_IMAGE for loading images of many filetypes
#include <stb_image.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include <Model.h>
#include "hdr.h"

using std::min;
using std::max;

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
static float currentTime = 0.0f;
static float deltaTime = 0.0f;
bool showUI = false;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint backgroundProgram, shaderProgram, postFxShader;
GLuint perlinWorleyNoiseProgram;
GLuint volumetricSphereProgram;

///////////////////////////////////////////////////////////////////////////////
// Noise textures
///////////////////////////////////////////////////////////////////////////////
GLuint perlinWorleyNoise;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float	environment_multiplier = 1.0f;
GLuint	environmentMap, irradianceMap, reflectionMap;
const	std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
float point_light_intensity_multiplier = 1000.0f;
vec3 point_light_color = vec3(1.f, 1.f, 1.f);
const vec3 lightPosition = vec3(20.0f, 40.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 securityCamPos = vec3(70.0f, 50.0f, -70.0f);
vec3 securityCamDirection = normalize(-securityCamPos);
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f, 15.f, 0.f) - cameraPosition);
float cameraSpeed = 10.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model* landingpadModel = nullptr;
labhelper::Model* fighterModel = nullptr;
labhelper::Model* sphereModel = nullptr;
labhelper::Model* cameraModel = nullptr;

float fighterRotateSpeed = 0;

///////////////////////////////////////////////////////////////////////////////
// Volumetrics
///////////////////////////////////////////////////////////////////////////////
vec3	volume_sphere_center	= vec3(0.f,0.f,0.f);
float	volume_sphere_radius	= 50.f;
float	volume_density			= 1.f;

///////////////////////////////////////////////////////////////////////////////
// Post processing effects
///////////////////////////////////////////////////////////////////////////////
enum PostProcessingEffect
{
	None = 0,
	Sepia = 1,
	Mushroom = 2,
	Blur = 3,
	Grayscale = 4,
	Composition = 5,
	Mosaic = 6,
	Separable_blur = 7,
	Bloom = 8,
};

int currentEffect = PostProcessingEffect::None;
int filterSize = 1;
int filterSizes[12] = { 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25 };


///////////////////////////////////////////////////////////////////////////////
// Framebuffers
///////////////////////////////////////////////////////////////////////////////

struct FboInfo;
std::vector<FboInfo> fboList;
// The noise textures are also store in this frame buffers
// The volumetric sphere is store as a color texture target 

///////////////////////////////////////////////////////////////////////////////
/// Holds and manages a framebuffer object
///////////////////////////////////////////////////////////////////////////////
struct FboInfo
{
	GLuint		framebufferId;
	GLuint		colorTextureTarget;
	GLuint		noiseTextureTarget;
	GLuint		depthBuffer;
	int			width;
	int			height;
	int			depth = 0;
	bool		isComplete;

	// For off-screen render-to-texture that are just noises
	bool		isNoise = false;

	FboInfo(int w, int h)
	{
		isComplete = false;
		width = w;
		height = h;
		// Generate two textures and set filter parameters (no storage allocated yet)
		glGenTextures(1, &colorTextureTarget);
		glBindTexture(GL_TEXTURE_2D, colorTextureTarget);

		///////// Render Doc Labels
		glObjectLabel(GL_TEXTURE, colorTextureTarget, -1, "color_texture_target");
		/////////

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

		glGenTextures(1, &depthBuffer);
		glBindTexture(GL_TEXTURE_2D, depthBuffer);

		///////// Render Doc Labels
		glObjectLabel(GL_TEXTURE, depthBuffer, -1, "depth_buffer_target");
		/////////

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// allocate storage for textures
		resize(width, height);

		///////////////////////////////////////////////////////////////////////
		// Generate and bind framebuffer
		///////////////////////////////////////////////////////////////////////
		// Task 1
		//Generate an ID to handle the memory allocation of the buffer and bind to set the state machine
		glGenFramebuffers(1, &framebufferId);
		glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);

		///////// Render Doc Labels
		glObjectLabel(GL_FRAMEBUFFER, framebufferId, -1, "off_screen_framebuffer");
		/////////

		// Attach the color textute target to which OpenGl will write color data
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTextureTarget, 0);
		// Difine to which attactment to draw
		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		// Attach the depth texture
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthBuffer, 0);

		// check if framebuffer is complete
		isComplete = checkFramebufferComplete();

		// bind default framebuffer, just in case.
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Constructor for a noise buffer
	FboInfo(int w, int h, int d, bool noise)
	{
		isNoise		= noise;
		isComplete	= false;
		width		= w;
		height		= h;
		depth		= d;

		glGenTextures(1, &noiseTextureTarget);
		glBindTexture(GL_TEXTURE_3D, noiseTextureTarget);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, width, height, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		///////////////////////////////////////////////////////////////////////
		// Generate and bind framebuffer
		///////////////////////////////////////////////////////////////////////
		glGenFramebuffers(1, &framebufferId);
		glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);

		// Attach the color textute target to which OpenGl will write color data
		glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, noiseTextureTarget, 0, 0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0); // Difine to which attactment to draw

		// Attach the depth texture
		//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthBuffer, 0);

		// check if framebuffer is complete
		isComplete = checkFramebufferComplete(); // Potential Error

		// bind default framebuffer, just in case.
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		///////// Render Doc Labels
		glObjectLabel(GL_TEXTURE, noiseTextureTarget, -1, "perlin_worley_noise_texture_target");
		glObjectLabel(GL_FRAMEBUFFER, framebufferId, -1, "off_screen_framebuffer_noise");
		/////////
	}

	// if no resolution provided
	FboInfo()
	    : isComplete(false)
	    , framebufferId(UINT32_MAX)
	    , colorTextureTarget(UINT32_MAX)
	    , depthBuffer(UINT32_MAX)
	    , width(0)
	    , height(0){};

	~FboInfo() {};

	void resize(int w, int h)
	{
		if (isNoise) return; // avoids resizing fixed-size noise texture

		width = w;
		height = h;
		// Allocate a texture
		glBindTexture(GL_TEXTURE_2D, colorTextureTarget);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		// generate a depth texture
		glBindTexture(GL_TEXTURE_2D, depthBuffer);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
		             nullptr);
	}

	bool checkFramebufferComplete(void)
	{
		// Check that our FBO is correctly set up, this can fail if we have
		// incompatible formats in a buffer, or for example if we specify an
		// invalid drawbuffer, among things.
		glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE)
		{
			labhelper::fatal_error("Framebuffer not complete");
		}

		return (status == GL_FRAMEBUFFER_COMPLETE);
	}
};


FboInfo* noiseFramebuffer;
FboInfo volumetricSphereFramebuffer;

///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();

	// enable Z-buffering
	glEnable(GL_DEPTH_TEST);

	// enable backface culling
	glEnable(GL_CULL_FACE);

	// Load some models.
	landingpadModel = labhelper::loadModelFromOBJ("../scenes/landingpad.obj");
	cameraModel = labhelper::loadModelFromOBJ("../scenes/wheatley.obj");
	fighterModel = labhelper::loadModelFromOBJ("../scenes/space-ship.obj");

	// load and set up default shader
	backgroundProgram = labhelper::loadShaderProgram("../lab5-rendertotexture/background.vert",
	                                                 "../lab5-rendertotexture/background.frag");
	shaderProgram = labhelper::loadShaderProgram("../lab5-rendertotexture/shading.vert",
	                                             "../lab5-rendertotexture/shading.frag");
	postFxShader = labhelper::loadShaderProgram("../lab5-rendertotexture/postFx.vert",
	                                            "../lab5-rendertotexture/postFx.frag");

	perlinWorleyNoiseProgram = labhelper::loadShaderProgram("../lab5-rendertotexture/perlin_worley_noise.vert",
																"../lab5-rendertotexture/perlin_worley_noise.frag");
	
	volumetricSphereProgram = labhelper::loadShaderProgram("../lab5-rendertotexture/volumetric_sphere.vert",
															"../lab5-rendertotexture/volumetric_sphere.frag");

	// Labeling Shader programs for Render Doc
	glObjectLabel(GL_PROGRAM, backgroundProgram, -1, "BackgroundProgram");
	glObjectLabel(GL_PROGRAM, shaderProgram, -1, "MainShaderProgramProgram");
	glObjectLabel(GL_PROGRAM, postFxShader, -1, "PostFxShader");
	glObjectLabel(GL_PROGRAM, perlinWorleyNoiseProgram, -1, "PerlinWorleyNoiseProgram");
	glObjectLabel(GL_PROGRAM, volumetricSphereProgram, -1, "VolumetricSphereProgram");

	///////////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////////
	const int roughnesses = 8;
	std::vector<std::string> filenames;
	for(int i = 0; i < roughnesses; i++)
		filenames.push_back("../scenes/envmaps/" + envmap_base_name + "_dl_" + std::to_string(i) + ".hdr");

	reflectionMap = labhelper::loadHdrMipmapTexture(filenames);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

	irradianceMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + "_irradiance.hdr");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

	///////////////////////////////////////////////////////////////////////////
	// Setup Framebuffers
	///////////////////////////////////////////////////////////////////////////
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	const int numFbos = 5;
	for (size_t i = 0; i < numFbos; i++)
	{
		fboList.push_back(FboInfo(w, h));
	}

	volumetricSphereFramebuffer = FboInfo(w, h);
	///////////////////////////////////////////////////////////////////////////
	// Setup Framebuffers for Noise Textures
	///////////////////////////////////////////////////////////////////////////
	noiseFramebuffer = new FboInfo(128, 128, 128, true);


}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to draw the main objects on the scene
///////////////////////////////////////////////////////////////////////////////
void drawScene(const mat4& view, const mat4& projection)
{
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "FULL_SCREEN_QUAD");
		glUseProgram(backgroundProgram);
		labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
		labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projection * view));
		labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
		labhelper::drawFullScreenQuad();
		glPopDebugGroup();
	}
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "LANDING_PAD");
		glUseProgram(shaderProgram);
		// Light source
		vec4 viewSpaceLightPosition = view * vec4(lightPosition, 1.0f);
		labhelper::setUniformSlow(shaderProgram, "point_light_color", point_light_color);
		labhelper::setUniformSlow(shaderProgram, "point_light_intensity_multiplier",
								  point_light_intensity_multiplier);
		labhelper::setUniformSlow(shaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));

		// Environment
		labhelper::setUniformSlow(shaderProgram, "environment_multiplier", environment_multiplier);

		// camera
		labhelper::setUniformSlow(shaderProgram, "viewInverse", inverse(view));

		// landing pad
		mat4 modelMatrix(1.0f);
		labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix", projection * view * modelMatrix);
		labhelper::setUniformSlow(shaderProgram, "modelViewMatrix", view * modelMatrix);
		labhelper::setUniformSlow(shaderProgram, "normalMatrix", inverse(transpose(view * modelMatrix)));

		labhelper::render(landingpadModel);
		glPopDebugGroup();
	}
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "FIGHTER");
		// Fighter
		mat4 fighterModelMatrix = translate(10.0f * worldUp) * rotate(currentTime * fighterRotateSpeed, worldUp);
		labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
								  projection * view * fighterModelMatrix);
		labhelper::setUniformSlow(shaderProgram, "modelViewMatrix", view * fighterModelMatrix);
		labhelper::setUniformSlow(shaderProgram, "normalMatrix", inverse(transpose(view * fighterModelMatrix)));

		labhelper::render(fighterModel);
		glPopDebugGroup();
	}
}


///////////////////////////////////////////////////////////////////////////////
/// This function draws only the "security" camera model
///////////////////////////////////////////////////////////////////////////////
void drawCamera(const mat4& camView, const mat4& view, const mat4& projection)
{
	glUseProgram(shaderProgram);
	mat4 invCamView = inverse(camView);
	mat4 camMatrix = invCamView * scale(vec3(10.0f)) * rotate(float(M_PI), vec3(0.0f, 1.0, 0.0));
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix", projection * view * camMatrix);
	labhelper::setUniformSlow(shaderProgram, "modelViewMatrix", view * camMatrix);
	labhelper::setUniformSlow(shaderProgram, "normalMatrix", inverse(transpose(view * camMatrix)));

	labhelper::render(cameraModel);
}

///////////////////////////////////////////////////////////////////////////////
/// Send a full-screen triangle through the pipeline. 
///////////////////////////////////////////////////////////////////////////////
void drawFullScreenTriangle()
{
	GLboolean previous_depth_state;
	glGetBooleanv(GL_DEPTH_TEST, &previous_depth_state);
	glDisable(GL_DEPTH_TEST);
	static GLuint vertexArrayObject = 0;
	static int nofVertices = 3;
	// do this initialization first time the function is called...
	if (vertexArrayObject == 0)
	{
		glGenVertexArrays(1, &vertexArrayObject);
		static const glm::vec2 positions[] = { 
												{	-1.0f,	1.0f	},
												{	-1.0f, -3.0f	}, 
												{	 3.0f,	1.0f	}
		};
		labhelper::createAddAttribBuffer(vertexArrayObject, positions, labhelper::array_length(positions) * sizeof(glm::vec2), 0, 2, GL_FLOAT);
	}
	glBindVertexArray(vertexArrayObject);
	glDrawArrays(GL_TRIANGLES, 0, nofVertices);
	if (previous_depth_state)
		glEnable(GL_DEPTH_TEST);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display()
{
	///////////////////////////////////////////////////////////////////////////
	// Noise
	///////////////////////////////////////////////////////////////////////////
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "NOISE_GENERATION");
		
		glDisable(GL_DEPTH_TEST);
		//glEnable(GL_TEXTURE_3D); // This is causing problems
		glBindFramebuffer(GL_FRAMEBUFFER, noiseFramebuffer->framebufferId);
		glViewport(0, 0, noiseFramebuffer->width, noiseFramebuffer->height); // The size of the window to render
		glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// Call draw
		glUseProgram(perlinWorleyNoiseProgram); // The new pipeline definition
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D, noiseFramebuffer->noiseTextureTarget);

		for (size_t i = 0; i < (size_t)noiseFramebuffer->depth; i++)
		{
			labhelper::setUniformSlow(perlinWorleyNoiseProgram, "slice",(int)i);
			glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, noiseFramebuffer->noiseTextureTarget, 0, i);
			drawFullScreenTriangle();
			
		}

		//glDisable(GL_TEXTURE_3D);
		glEnable(GL_DEPTH_TEST);
		glPopDebugGroup();
	}

	///////////////////////////////////////////////////////////////////////////
	// Check if any framebuffer needs to be resized
	///////////////////////////////////////////////////////////////////////////
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);

	for(int i = 0; i < fboList.size(); i++)
	{
		// Each Framebuffer info keeps a record of the size of the window it was first initialized, 
		// and at everty frame checks is this has changed to update a reallocate the render targets.
		if(fboList[i].width != w || fboList[i].height != h)
			fboList[i].resize(w, h);
	}

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 securityCamViewMatrix = lookAt(securityCamPos, securityCamPos + securityCamDirection, worldUp);
	// Notice that the near and far plane values, and well as the Field of view can be different, as it is another POV.
	mat4 securityCamProjectionMatrix = perspective(radians(30.0f), float(w) / float(h), 15.0f, 1000.0f);

	mat4 projectionMatrix = perspective(radians(45.0f), float(w) / float(h), 10.0f, 1000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);
	
	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, irradianceMap);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, reflectionMap);

	///////////////////////////////////////////////////////////////////////////
	// draw scene from security camera
	///////////////////////////////////////////////////////////////////////////
	// Bind the framebuffer to update the state machine
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "OFF_SCREEN_SECURITY_CAMERA_POV");
		glBindFramebuffer(GL_FRAMEBUFFER, fboList[0].framebufferId);
		glViewport(0,0, fboList[0].width, fboList[0].height); // The size of the window to render
		glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		drawScene(securityCamViewMatrix, securityCamProjectionMatrix);
		glPopDebugGroup();
	}

	// Use color texture target as a image texture to sample from

	labhelper::Material& screen = landingpadModel->m_materials[8];
	screen.m_emission_texture.gl_id = fboList[0].colorTextureTarget;

	///////////////////////////////////////////////////////////////////////////
	// draw scene from camera
	///////////////////////////////////////////////////////////////////////////
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "OFF_SCREEN_CAMERA_POV");
		glBindFramebuffer(GL_FRAMEBUFFER, fboList[1].framebufferId);
		//glBindFramebuffer(GL_FRAMEBUFFER, 0); // to be replaced with another framebuffer when doing post processing

		glViewport(0, 0, fboList[1].width, fboList[1].height);
		glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		drawScene(viewMatrix, projectionMatrix); // using both shaderProgram and backgroundProgram
		glPopDebugGroup();
	}
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "CAMERA_MESH");
		// camera (obj-model)
		drawCamera(securityCamViewMatrix, viewMatrix, projectionMatrix);
		glPopDebugGroup();
	}
	///////////////////////////////////////////////////////////////////////////
	// Volumetric Render Pass
	///////////////////////////////////////////////////////////////////////////
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 2, -1, "VOLUMETRIC_PASS");

		glBindFramebuffer(GL_FRAMEBUFFER, volumetricSphereFramebuffer.framebufferId);
		glViewport(0, 0, volumetricSphereFramebuffer.width, volumetricSphereFramebuffer.height);
		glClearColor(0.f, 1.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(volumetricSphereProgram);

		mat4 volumeSphereModelMatrix = translate(vec3(25,0,-25));
		
		GLint uniformLocation;
		
		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "sphere_center");
		glUniform3fv(uniformLocation, 1, &volume_sphere_center.x); // Pass the value of the first argument411

		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "sphere_radius");
		glUniform1fv(uniformLocation, 1, &volume_sphere_radius);

		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "density");
		glUniform1fv(uniformLocation, 1, &volume_density);

		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "inverse_view_projection_matrix");
		glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, &inverse(projectionMatrix * viewMatrix)[0].x); // Pass the value of the first argument
		

		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "view_projection_matrix");
		glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, &(projectionMatrix * viewMatrix /* volumeSphereModelMatrix*/)[0].x); // Pass the value of the first argument
		
		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "width");
		glUniform1i(uniformLocation, volumetricSphereFramebuffer.width);
		
		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "height");
		glUniform1i(uniformLocation, volumetricSphereFramebuffer.height);
		
		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "normalize_factors");
		vec2 normalize_extend = vec2(1.0) / vec2((float)volumetricSphereFramebuffer.width, (float)volumetricSphereFramebuffer.height);
		glUniform2fv(uniformLocation, 1, &normalize_extend.x);
		
		uniformLocation = glGetUniformLocation(volumetricSphereProgram, "camera_position");
		glUniform3fv(uniformLocation, 1,  &cameraPosition.x);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, fboList[1].colorTextureTarget);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, fboList[1].depthBuffer);
	
		drawFullScreenTriangle();

		glPopDebugGroup();
	}
	{
		/*glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 2, -1, "VOLUMETRIC_COMPOSITE");

		glBindFramebuffer(GL_FRAMEBUFFER, fboList[2].framebufferId);
		glViewport(0, 0, fboList[2].width, fboList[2].height);
		glClearColor(1, 0, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



		glPopDebugGroup();*/
	}

	// Until this point, the screen will show a balck window, since out default frame buffer has no render data.
	// The reneder data has been written in the framebuffer [1]. This is an off-screen render target that we can sample latter
	// To render again to the screen we:
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 2, -1, "BACKBUFFER_COMPOSITE");
		// Bind the default frame buffer again, set the viewport and clear it
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, w, h);
		glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
		// We draw to a full screen quad now
		// This process requiers a new pipeline, since we do not need to send any meshes to the GPU, just shade the fragments
		// of the full-screen quad using the already generated color texture

		glUseProgram(postFxShader); // The new pipeline definition
		// Set the uniforms for this shader instance
		labhelper::setUniformSlow(postFxShader, "time", currentTime);
		labhelper::setUniformSlow(postFxShader, "currentEffect", currentEffect);
		labhelper::setUniformSlow(postFxShader, "filterSize", filterSizes[filterSize - 1]);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, volumetricSphereFramebuffer.colorTextureTarget);
		//glBindTexture(GL_TEXTURE_2D, fboList[1].colorTextureTarget);
		labhelper::drawFullScreenQuad();
		glPopDebugGroup();
	}
	///////////////////////////////////////////////////////////////////////////
	// Post processing pass(es)
	///////////////////////////////////////////////////////////////////////////
	// Task 3:
	// 1. Bind and clear default framebuffer
	// 2. Set postFxShader as active
	// 3. Bind the framebuffer to texture unit 0
	// 4. Draw a quad over the entire viewport

	// Task 4: Set the required uniforms

	glUseProgram(0);

	CHECK_GL_ERROR();
}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	ImGuiIO& io = ImGui::GetIO();
	while(SDL_PollEvent(&event))
	{
		ImGui_ImplSdlGL3_ProcessEvent(&event);

		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		else if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			showUI = !showUI;
		}
		else if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_PRINTSCREEN)
		{
			labhelper::saveScreenshot();
		}
		else if(event.type == SDL_MOUSEBUTTONDOWN
		        && (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)
		        && (!showUI || !ImGui::GetIO().WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		uint32_t mouseState = SDL_GetMouseState(NULL, NULL);
		if(!(mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) && !(mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.1f;
			if(mouseState & SDL_BUTTON(SDL_BUTTON_LEFT))
			{
				mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
				mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
				                    normalize(cross(cameraDirection, worldUp)));
				cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			}
			else if(mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT))
			{
				mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
				mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
				                    normalize(cross(securityCamDirection, worldUp)));
				securityCamDirection = vec3(pitch * yaw * vec4(securityCamDirection, 0.0f));
			}
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
		}
	}

	if(!io.WantCaptureKeyboard)
	{
		// check keyboard state (which keys are still pressed)
		const uint8_t* state = SDL_GetKeyboardState(nullptr);
		vec3 cameraRight = cross(cameraDirection, worldUp);
		if(state[SDL_SCANCODE_W])
		{
			cameraPosition += deltaTime * cameraSpeed * cameraDirection;
		}
		if(state[SDL_SCANCODE_S])
		{
			cameraPosition -= deltaTime * cameraSpeed * cameraDirection;
		}
		if(state[SDL_SCANCODE_A])
		{
			cameraPosition -= deltaTime * cameraSpeed * cameraRight;
		}
		if(state[SDL_SCANCODE_D])
		{
			cameraPosition += deltaTime * cameraSpeed * cameraRight;
		}
		if(state[SDL_SCANCODE_Q])
		{
			cameraPosition -= deltaTime * cameraSpeed * worldUp;
		}
		if(state[SDL_SCANCODE_E])
		{
			cameraPosition += deltaTime * cameraSpeed * worldUp;
		}
	}

	return quitEvent;
}


///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
float volume_center[3] = { 62.500f , -45.833f,  -61.983f };
void gui()
{
	// ----------------- Set variables --------------------------
	ImGui::Text("Volumetrics");
	ImGui::SliderFloat3("Sphere center", volume_center, -500, 500);
	ImGui::SliderFloat("Sphere radius", &volume_sphere_radius, 10.f, 1000.f);
	ImGui::SliderFloat("Volume density", &volume_density, 0.f, 1.f);
	ImGui::SameLine();
	ImGui::Text("Post-processing effect");
	ImGui::RadioButton("None", &currentEffect, PostProcessingEffect::None);
	ImGui::RadioButton("Sepia", &currentEffect, PostProcessingEffect::Sepia);
	ImGui::RadioButton("Mushroom", &currentEffect, PostProcessingEffect::Mushroom);
	ImGui::RadioButton("Blur", &currentEffect, PostProcessingEffect::Blur);
	ImGui::SameLine();
	ImGui::SliderInt("Filter size", &filterSize, 1, 12);
	ImGui::RadioButton("Grayscale", &currentEffect, PostProcessingEffect::Grayscale);
	ImGui::RadioButton("All of the above", &currentEffect, PostProcessingEffect::Composition);
	ImGui::RadioButton("Mosaic", &currentEffect, PostProcessingEffect::Mosaic);
	ImGui::RadioButton("Separable Blur", &currentEffect, PostProcessingEffect::Separable_blur);
	ImGui::RadioButton("Bloom", &currentEffect, PostProcessingEffect::Bloom);
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// ----------------------------------------------------------
	volume_sphere_center = vec3(volume_center[0], volume_center[1], volume_center[2]);
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Lab 5");

	initialize();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		deltaTime = timeSinceStart.count() - currentTime;
		currentTime = timeSinceStart.count();

		// Inform imgui of new frame
		ImGui_ImplSdlGL3_NewFrame(g_window);

		// check events (keyboard among other)
		stopRendering = handleEvents();

		// render to window
		display();

		// Render overlay GUI.
		if(showUI)
		{
			gui();
		}

		// Render the GUI.
		ImGui::Render();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}
	// Delete Frames
	delete noiseFramebuffer;

	// Free Models
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(cameraModel);
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(sphereModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
