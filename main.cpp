#ifdef WIN32
#include <windows.h>
#endif


#include <GL/glew.h>
#include <GL/freeglut.h>

#include <IL/il.h>
#include <IL/ilut.h>

#include <stdlib.h>
#include <algorithm>

#include <OBJModel.h>
#include <glutil.h>
#include <float4x4.h>
#include <float3x3.h>

#include <vector>

#include <ctime>

#include "Octree.h"
#include "Triangle.h"
#include <chrono>

using namespace std;
using namespace chag;
using namespace chrono;

//*****************************************************************************
//	Global variables
//*****************************************************************************
bool paused = false;				// Tells us wether sun animation is paused
float currentTime = 0.0f;		// Tells us the current time
GLuint shaderProgram;
const float3 up = {0.0f, 1.0f, 0.0f};

//*****************************************************************************
//	OBJ Model declarations
//*****************************************************************************
OBJModel *world; 
OBJModel *worldCollision;
OBJModel *water; 
OBJModel *skybox; 
OBJModel *skyboxnight; 
OBJModel *car;
OBJModel *test;

Octree t(make_vector(0.0f, 0.0f, 0.0f), make_vector(100.0f, 50.0f, 100.0f), 0);

//*****************************************************************************
//	Camera state variables (updated in motion())
//*****************************************************************************
float camera_theta = M_PI / 1.0f;
float camera_phi = M_PI / 4.0f;
float camera_r = 30.0; 
float camera_target_altitude = 5.2; 

//*****************************************************************************
//	Light state variables (updated in idle())
//*****************************************************************************
float3 lightPosition = {30.1f, 450.0f, 0.1f};

//****************************************************************************
//	Mouse input state variables
//*****************************************************************************
bool leftDown = false;
bool middleDown = false;
bool rightDown = false;
int prev_x = 0;
int prev_y = 0;

struct Fbo{
	GLuint id;
	GLuint texture;
	GLuint shaderProgram;
	GLuint depthbuffer;

	int width, height;
};

int screenWidth = 1024, screenheight = 800;


bool keysDown[256];

Fbo sbo;
Fbo cMapAll;
Fbo postProcessFbo, horizontalBlurFbo, verticalBlurFbo, cutOffFbo;


struct Camera {
	float3 position;
	float3 lookAt;
	float3 up;
};

int shadowMapResolution = 2048;

struct Car{
	float3 frontDir = make_vector(0.0f, 0.0f, 1.0f);
	float3 upDir = make_vector(0.0f, 1.0f, 0.0f);
	float3 location = make_vector(0.0f, 0.0f, 0.0f);
	float3  
		wheel1 = make_vector( 1.2f, 0.0f,  1.5f),
		wheel2 = make_vector( 1.2f, 0.0f, -1.5f),
		wheel3 = make_vector(-0.8f, 0.0f,  1.5f),
		wheel4 = make_vector(-0.8f, 0.0f, -1.5f);

	float rotationSpeed = M_PI / 180 * 3;
	float moveSpeed = 1.3;
	float angley = 0, anglez;
	float lengthx = 2, lengthz = 3;
};


std::vector<Triangle> ts;

Car carLoc;
Camera cameras[6];
int camera = 6;

// Helper function to turn spherical coordinates into cartesian (x,y,z)
float3 sphericalToCartesian(float theta, float phi, float r)
{
	return make_vector( r * sinf(theta)*sinf(phi),
					 	r * cosf(phi), 
						r * cosf(theta)*sinf(phi) );
}

GLuint cubeMapTexture;

Camera createCamera(float3 position, float3 lookAt, float3 up);
void drawCubeMap(Fbo fbo);

void addMeshToCollision(OBJModel *model, float4x4 modelMatrix);

void drawFullScreenQuad();
Fbo createPostProcessFbo(int width, int height);
void blurImage();

void debugDrawOctree(const float4x4 &viewMatrix, const float4x4 &projectionMatrix, Octree tree);
void debugDrawQuad(const float4x4 &viewMatrix, const float4x4 &projectionMatrix, float3 origin, float3 halfVector);
void debugDrawLine(const float4x4 &viewMatrix, const float4x4 &projectionMatrix, float3 origin, float3 rayVector);

float rayOctreeIntersection(float3 rayOrigin, float3 rayVec, Octree oct);

GLuint postFxShader;
GLuint horizontalBlurShader;
GLuint verticalBlurShader;
GLuint cutoffShader;


void initGL()
{
	/* Initialize GLEW; this gives us access to OpenGL Extensions.
	 */
	glewInit();  

	/* Print information about OpenGL and ensure that we've got at a context 
	 * that supports least OpenGL 3.0. Then setup the OpenGL Debug message
	 * mechanism.
	 */
	startupGLDiagnostics();
	setupGLDebugMessages();

	/* Initialize DevIL, the image library that we use to load textures. Also
	 * tell IL that we intent to use it with OpenGL.
	 */
	ilInit();
	ilutRenderer(ILUT_OPENGL);

	/* Workaround for AMD. It might no longer be necessary, but I dunno if we
	 * are ever going to remove it. (Consider it a piece of living history.)
	 */
	if( !glBindFragDataLocation )
	{
		glBindFragDataLocation = glBindFragDataLocationEXT;
	}

	/* As a general rule, you shouldn't need to change anything before this 
	 * comment in initGL().
	 */

	//*************************************************************************
	//	Load shaders
	//*************************************************************************
	shaderProgram = loadShaderProgram("simple.vert", "simple.frag");
	glBindAttribLocation(shaderProgram, 0, "position"); 	
	glBindAttribLocation(shaderProgram, 2, "texCoordIn");
	glBindAttribLocation(shaderProgram, 1, "normalIn");
	glBindFragDataLocation(shaderProgram, 0, "fragmentColor");
	linkShaderProgram(shaderProgram);


	//*************************************************************************
	// Load the models from disk
	//*************************************************************************
	world = new OBJModel(); 
	world->load("../scenes/island2.obj");

	worldCollision = new OBJModel();
	worldCollision->load("../scenes/island2Collision.obj");

	skybox = new OBJModel();
	skybox->load("../scenes/skybox.obj");
	skyboxnight = new OBJModel();
	skyboxnight->load("../scenes/skyboxnight.obj");
	// Make the textures of the skyboxes use clamp to edge to avoid seams
	for(int i=0; i<6; i++){
		glBindTexture(GL_TEXTURE_2D, skybox->getDiffuseTexture(i)); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, skyboxnight->getDiffuseTexture(i)); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	water = new OBJModel(); 
	water->load("../scenes/water.obj");
	car = new OBJModel(); 
	car->load("../scenes/car.obj");

	test = new OBJModel();
	test->load("../scenes/test.obj");


	addMeshToCollision(worldCollision, make_identity<float4x4>());
	addMeshToCollision(water, make_translation(make_vector(0.0f, -6.0f, 0.0f)));
	addMeshToCollision(test, make_translation(make_vector(-15.0f, 0.0f, 0.0f)) * make_rotation_y<float4x4>(M_PI / 180 * 90) * make_scale<float4x4>(make_vector(2.0f, 2.0f, 2.0f)));

	high_resolution_clock::time_point start = high_resolution_clock::now();
	for (int i = 0; i < ts.size(); i++) {
		t.insert(&ts.at(i));
	}
	high_resolution_clock::time_point end = high_resolution_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(end - start);
	printf("Time to generate octree: %f\n", time_span.count());

	int cou = t.getCount();
	printf("%d", cou);

	//*************************************************************************
	// Generate shadow map frame buffer object
	//*************************************************************************
	
	sbo.shaderProgram = loadShaderProgram("shadowMap.vert", "shadowMap.frag");
		glBindAttribLocation(sbo.shaderProgram, 0, "position");
		glBindFragDataLocation(sbo.shaderProgram, 0, "fragmentColor");
	linkShaderProgram(sbo.shaderProgram);


	int w = screenWidth;
	int h = screenheight;

	sbo.width = shadowMapResolution;
	sbo.height = shadowMapResolution;
	
	glGenFramebuffers(1, &sbo.id);
	glBindFramebuffer(GL_FRAMEBUFFER, sbo.id);

	glGenTextures(1, &sbo.texture);
	glBindTexture(GL_TEXTURE_2D, sbo.texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, shadowMapResolution, shadowMapResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	float4 zeros = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &zeros.x);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sbo.texture, 0);

	//Disable reading color buffer
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	//Cleanup
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);


	//*************************************************************************
	// Create post process Fbo
	//*************************************************************************
	
	postFxShader = loadShaderProgram("postFx.vert", "postFx.frag");
	verticalBlurShader = loadShaderProgram("postFx.vert", "vertical_blur.frag");
	horizontalBlurShader = loadShaderProgram("postFx.vert", "horizontal_blur.frag");
	cutoffShader = loadShaderProgram("postFx.vert", "cutoff.frag");

	glBindAttribLocation(postFxShader, 0, "position");
	glBindFragDataLocation(postFxShader, 0, "fragmentColor");

	linkShaderProgram(postFxShader);
	linkShaderProgram(verticalBlurShader);
	linkShaderProgram(horizontalBlurShader);
	linkShaderProgram(cutoffShader);

	postProcessFbo = createPostProcessFbo(w, h);
	verticalBlurFbo = createPostProcessFbo(w, h);
	horizontalBlurFbo = createPostProcessFbo(w, h);
	cutOffFbo = createPostProcessFbo(w, h);

	//*************************************************************************
	// Load cube map texture
	//*************************************************************************
	cubeMapTexture = loadCubeMap("cube0.png", "cube1.png", "cube2.png", "cube3.png", "cube4.png", "cube5.png");

	//X
	cameras[0] = createCamera(make_vector(0.0f, 3.0f, 0.0f), make_vector(100.0f, 3.0f, 0.0f),    make_vector(0.0f, -1.0f, 0.0f));
	cameras[1] = createCamera(make_vector(0.0f, 3.0f, 0.0f), make_vector(-100.0f,3.0f, 0.0f),    make_vector(0.0f, 1.0f, 0.0f));

	//Y
	cameras[2] = createCamera(make_vector(0.0f, 3.0f, 0.0f), make_vector(0.1f, 100.0f, 0.1f), make_vector(0.0f, 1.0f, 0.0f));
	cameras[3] = createCamera(make_vector(0.0f, 3.0f, 0.0f), make_vector(0.1f, -100.0f, 0.1f), make_vector(0.0f, 1.0f, 0.0f));

	//Z
	cameras[4] = createCamera(make_vector(0.0f, 3.0f, 0.0f), make_vector(0.1f, 0.1f, 100.0f), make_vector(0.0f, 1.0f, 0.0f));
	cameras[5] = createCamera(make_vector(0.0f, 3.0f, 0.0f), make_vector(0.1f, -0.1f, -100.0f), make_vector(0.0f, 1.0f, 0.0f));

	cMapAll.width = w;
	cMapAll.height = h;

	glGenFramebuffers(1, &cMapAll.id);
	glBindFramebuffer(GL_FRAMEBUFFER, cMapAll.id);

	glGenTextures(1, &cMapAll.texture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cMapAll.texture);

	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	GLuint depthBuffer;
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,	GL_RENDERBUFFER, depthBuffer);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, cMapAll.texture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, cMapAll.texture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, cMapAll.texture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, cMapAll.texture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, cMapAll.texture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, cMapAll.texture, 0);

	cMapAll.shaderProgram = shaderProgram;
	
	//Cleanup
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glEnable(GL_DEPTH_TEST);
}

void addMeshToCollision(OBJModel *model, float4x4 modelMatrix) {
	for (int i = 0; i < model->m_chunks.size(); i++) {

		for (int j = 0; j < model->m_chunks[i].m_positions.size(); j += 3) {
			

			float4 p1 = modelMatrix * make_vector(model->m_chunks[i].m_positions[j + 0].x,
									model->m_chunks[i].m_positions[j + 0].y, 
									model->m_chunks[i].m_positions[j + 0].z, 1.0f) ;
			
			float4 p2 = modelMatrix * make_vector(model->m_chunks[i].m_positions[j + 1].x,
				model->m_chunks[i].m_positions[j + 1].y,
				model->m_chunks[i].m_positions[j + 1].z, 1.0f) ;

			float4 p3 = modelMatrix * make_vector(model->m_chunks[i].m_positions[j + 2].x,
				model->m_chunks[i].m_positions[j + 2].y,
				model->m_chunks[i].m_positions[j + 2].z, 1.0f) ;

			Triangle t(make_vector(p1.x, p1.y, p1.z), make_vector(p2.x, p2.y, p2.z), make_vector(p3.x, p3.y, p3.z));
		
			ts.push_back(t);
		}
	}
}

bool rayTriangle(float3 r_o, float3 r_d, float3 v1, float3 v2, float3 v3, float *ins)
{
	float3 e2 = v3 - v1;       // second edge
	float3 e1 = v2 - v1;       // first edge


	float3 r = cross(r_d, e2);  // (d X e2) is used two times in the formula
	// so we store it in an appropriate vector
	float3 s = r_o - v1;       // translated ray origin
	float a = dot(e1, r);    // a=(d X e2)*e1
	float f = 1.0f / a;           // slow division*
	float3 q = cross(s, e1);
	float u = dot(s, r);
	bool frontfacing = true;
	float eps = 0.0001f;
	if (a > eps)            // eps is the machine fpu epsilon (precision), 
		// or a very small number :)
	{ // Front facing triangle...
		if ((u<0) || (u>a)) return false;
		float v = dot(r_d, q);
		if ((v<0) || (u + v>a)) return false;
	}
	else if (a<-eps)
	{ // Back facing triangle...
		frontfacing = false;
		if ((u > 0) || (u < a)) return false;
		float v = dot(r_d, q);
		if ((v>0) || (u + v<a)) return false;
	}
	else return false; // Ray parallel to triangle plane
	float t = f*dot(e2, q);
	float v = dot(r_d, q);
	u = u*f; 
	v = v*f;

	*ins = t;

	return true;
}


Fbo createPostProcessFbo(int width, int height) {

	Fbo fbo;

	fbo.width = width;
	fbo.height = height;

	glGenFramebuffers(1, &fbo.id);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.id);

	glGenTextures(1, &fbo.texture);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fbo.texture);

	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, fbo.texture, 0);

	glGenRenderbuffers(1, &fbo.depthbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, fbo.depthbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo.depthbuffer);


	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	return fbo;
}


void drawModel(OBJModel *model, const float4x4 &modelMatrix, GLuint shaderProgram)
{
	setUniformSlow(shaderProgram, "modelMatrix", modelMatrix); 
	model->render();
}

/**
* In this function, add all scene elements that should cast shadow, that way
* there is only one draw call to each of these, as this function is called twice.
*/
void drawShadowCasters(GLuint shaderProgram)
{
	drawModel(world, make_identity<float4x4>(), shaderProgram);
	setUniformSlow(shaderProgram, "object_reflectiveness", 1.5f); 
	drawModel(car
		, make_translation(carLoc.location) * make_rotation_z<float4x4>(carLoc.anglez) * make_rotation_y<float4x4>(carLoc.angley),
		shaderProgram);
	setUniformSlow(shaderProgram, "object_reflectiveness", 0.0f); 

	drawModel(test, make_translation(make_vector(-15.0f, 0.0f, 0.0f)) * make_rotation_y<float4x4>(M_PI / 180 * 90) * make_scale<float4x4>(make_vector(2.0f, 2.0f, 2.0f)), shaderProgram);

}

void drawShadowMap(Fbo sbo, float4x4 viewProjectionMatrix) {
	glBindFramebuffer(GL_FRAMEBUFFER, sbo.id);
	glViewport(0, 0, shadowMapResolution, shadowMapResolution);

	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClearDepth(1.0); 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.5, 2.0);

	GLint currentProgram;
	glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
	glUseProgram(sbo.shaderProgram);

	setUniformSlow(sbo.shaderProgram, "viewProjectionMatrix", viewProjectionMatrix);

	drawShadowCasters(sbo.shaderProgram);

	//CLEANUP
	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(currentProgram);
}

void drawCubeMap(Fbo fbo) {
	int w = glutGet((GLenum)GLUT_WINDOW_WIDTH);
	int h = glutGet((GLenum)GLUT_WINDOW_HEIGHT);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo.id);
	glViewport(0, 0, w, h);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//GLint currentProgram;
	//glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
	//glUseProgram(fbo.shaderProgram);

	float4x4 projectionMatrix = perspectiveMatrix(90.0f, 1, 2.0f, 500.0f);

	for (int i = 0; i < 6; i++) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, fbo.texture, 0);

		float4x4 viewMatrix = lookAt(cameras[i].position, cameras[i].lookAt, cameras[i].up);

		setUniformSlow(shaderProgram, "projectionMatrix", projectionMatrix);
		setUniformSlow(shaderProgram, "viewMatrix", viewMatrix);
		setUniformSlow(shaderProgram, "inverseViewNormalMatrix", transpose(viewMatrix));

		
		drawModel(water, make_translation(make_vector(0.0f, -6.0f, 0.0f)), shaderProgram);
		drawModel(world, make_identity<float4x4>(), shaderProgram);

		
		//Draw skybox
		glDepthMask(GL_FALSE); //Used to write over the skyboxnight with the skybox alpha value
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		drawModel(skyboxnight, make_identity<float4x4>(), shaderProgram);

		setUniformSlow(shaderProgram, "object_alpha", max<float>(0.0f, cosf((currentTime / 20.0f) * 2.0f * M_PI)));
		drawModel(skybox, make_identity<float4x4>(), shaderProgram);
		setUniformSlow(shaderProgram, "object_alpha", 1.0f);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	// CLEAN UP
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glUseProgram(currentProgram);

}

void drawScene(void)
{
	
	// enable back face culling.
	glEnable(GL_CULL_FACE);	


	//*************************************************************************
	// Render shadow map
	//*************************************************************************
	float3 light_lookAt = make_vector(0.0f, 0.0f, 0.0f);
	float3 light_up = make_vector(0.0f, 1.0f, 0.0f);
	float4x4 lightViewMatrix = lookAt(lightPosition, light_lookAt, light_up);

	int w = glutGet((GLenum)GLUT_WINDOW_WIDTH);
	int h = glutGet((GLenum)GLUT_WINDOW_HEIGHT);

	float4x4 lightProjectionMatrix = perspectiveMatrix(25.0f, 1.0f, 370.0f, 530.0f);
	float4x4 lightViewProjectionMatrix = lightProjectionMatrix * lightViewMatrix;

	drawShadowMap(sbo, lightViewProjectionMatrix);
	glBindFramebuffer(GL_FRAMEBUFFER, postProcessFbo.id);

	//*************************************************************************
	// Render the scene from the cameras viewpoint, to the default framebuffer
	//*************************************************************************
	glClearColor(0.2,0.2,0.8,1.0);						
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
	glViewport(0, 0, w, h);								
	// Use shader and set up uniforms
	glUseProgram( shaderProgram );			

	//Calculate camera matrix
	float3 camera_position = carLoc.location + sphericalToCartesian(camera_theta, camera_phi, camera_r);
	float3 camera_lookAt = carLoc.location + make_vector(0.0f, camera_target_altitude, 0.0f);
	float3 camera_up = make_vector(0.0f, 1.0f, 0.0f);

	//CUBA MAP VIEW
	/*int i = 3;
	float4x4 viewMatrix = lookAt(cameras[i].position, cameras[i].lookAt, cameras[i].up);
	float4x4 projectionMatrix = perspectiveMatrix(90.0f, float(cMapAll.width) / float(cMapAll.height), 0.1f, 500.0f);*/

	// SUNS VIEW
	//float4x4 viewMatrix = lookAt(lightPosition, light_lookAt, light_up);
	//float4x4 projectionMatrix = perspectiveMatrix(25.0f, 1.0f, 370.0f, 530.0f);

	float4x4 viewMatrix;
	float4x4 projectionMatrix;
	if (camera == 6) {
		viewMatrix = lookAt(camera_position, camera_lookAt, camera_up);
		projectionMatrix = perspectiveMatrix(45.0f, float(w) / float(h), 0.1f, 1000.0f);
	}
	else if (camera == 7) {
		viewMatrix = lookAt(lightPosition, light_lookAt, light_up);
		projectionMatrix = perspectiveMatrix(25.0f, 1.0f, 370.0f, 530.0f);
	}
	else
	{
		viewMatrix = lookAt(cameras[camera].position, cameras[camera].lookAt, cameras[camera].up);
		projectionMatrix = perspectiveMatrix(90.0f, float(w) / float(h), 0.1f, 500.0f);
	}

	

	float4x4 lightMatrix = make_translation({ 0.5, 0.5, 0.5 }) * make_scale<float4x4>(make_vector(0.5f, 0.5f, 0.5f)) *
		lightViewProjectionMatrix * inverse(viewMatrix);

	

	//Sets matrices
	setUniformSlow(shaderProgram, "viewMatrix", viewMatrix);
	setUniformSlow(shaderProgram, "projectionMatrix", projectionMatrix);
	setUniformSlow(shaderProgram, "lightpos", lightPosition);
	setUniformSlow(shaderProgram, "lightMatrix", lightMatrix);
	setUniformSlow(shaderProgram, "inverseViewNormalMatrix", transpose(viewMatrix));


	//Set shadowmap
	setUniformSlow(shaderProgram, "shadowMap", 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, sbo.texture);

	//Set cube map
	setUniformSlow(shaderProgram, "cubeMap", 2);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cMapAll.texture);
	//glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);


	drawModel(water, make_translation(make_vector(0.0f, -6.0f, 0.0f)), shaderProgram);

	drawShadowCasters(shaderProgram);


	//DEBUGS ONLY
	//debugDrawOctree(viewMatrix, projectionMatrix, t);
	debugDrawLight(viewMatrix, projectionMatrix, lightPosition);
	//debugDrawQuad(viewMatrix, projectionMatrix, carLoc.location + make_vector(0.2f, 1.2f, 0.0f), make_vector(1.0f, 1.0f, 1.5f));
	float3x3 rot = make_rotation_y<float3x3>(carLoc.angley);
	debugDrawLine(viewMatrix, projectionMatrix, carLoc.location + rot * carLoc.wheel1, -carLoc.upDir);
	debugDrawLine(viewMatrix, projectionMatrix, carLoc.location + rot * carLoc.wheel2, -carLoc.upDir);
	debugDrawLine(viewMatrix, projectionMatrix, carLoc.location + rot * carLoc.wheel3, -carLoc.upDir);
	debugDrawLine(viewMatrix, projectionMatrix, carLoc.location + rot * carLoc.wheel4, -carLoc.upDir);

	//Draw skybox
	glDepthMask(GL_FALSE); //Used to write over the skyboxnight with the skybox alpha value
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawModel(skyboxnight, make_identity<float4x4>(), shaderProgram);

	setUniformSlow(shaderProgram, "object_alpha", max<float>(0.0f, cosf((currentTime / 20.0f) * 2.0f * M_PI))); 
	drawModel(skybox, make_identity<float4x4>(), shaderProgram);
	setUniformSlow(shaderProgram, "object_alpha", 1.0f); 
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	drawCubeMap(cMapAll);

	blurImage();

	/*glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, verticalBlurFbo.id);
	glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT,
		GL_NEAREST);*/

	

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, w, h);
	glClearColor(0.6, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glUseProgram(postFxShader);
	setUniformSlow(postFxShader, "frameBufferTexture", 0);
	setUniformSlow(postFxShader, "blurredFrameBufferTexture", 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postProcessFbo.texture);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, verticalBlurFbo.texture);

	setUniformSlow(postFxShader, "time", currentTime);

	drawFullScreenQuad();

	glUseProgram( 0 );	
}

void blurImage() {
	//CUTOFF
	glUseProgram(cutoffShader);
	glBindFramebuffer(GL_FRAMEBUFFER, cutOffFbo.id);
	glViewport(0, 0, screenWidth, screenheight);
	glClearColor(1.0, 1.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	setUniformSlow(cutoffShader, "time", currentTime);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postProcessFbo.texture);
	
	drawFullScreenQuad();

	//HORIZONTAL
	glBindFramebuffer(GL_FRAMEBUFFER, horizontalBlurFbo.id);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(horizontalBlurShader);

	setUniformSlow(horizontalBlurShader, "frameBufferTexture", 0);
	setUniformSlow(horizontalBlurShader, "time", currentTime);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, cutOffFbo.texture);
	drawFullScreenQuad();

	//VERTICAL
	glBindFramebuffer(GL_FRAMEBUFFER, verticalBlurFbo.id);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(verticalBlurShader);

	setUniformSlow(verticalBlurShader, "frameBufferTexture", 0);
	setUniformSlow(verticalBlurShader, "time", currentTime);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, horizontalBlurFbo.texture);
	drawFullScreenQuad();
}

void checkIntersection() {

	//debugDrawQuad(viewMatrix, projectionMatrix, carLoc.location + make_vector(0.2f, 1.2f, 0.0f), make_vector(1.0f, 1.0f, 1.5f));

	float3x3 rot = make_rotation_y<float3x3>(carLoc.angley);
	float a = rayOctreeIntersection(carLoc.location + rot * carLoc.wheel1, -carLoc.upDir, t);
	float b = rayOctreeIntersection(carLoc.location + rot * carLoc.wheel2, -carLoc.upDir, t);
	float c = rayOctreeIntersection(carLoc.location + rot * carLoc.wheel3, -carLoc.upDir, t);
	float d = rayOctreeIntersection(carLoc.location + rot * carLoc.wheel4, -carLoc.upDir, t);
	
	carLoc.anglez = -(asinf((carLoc.wheel1.y - a) - (carLoc.wheel3.y - c) / carLoc.lengthx));
	
	printf("angle: %f\n", carLoc.anglez);

	carLoc.wheel1 += make_vector(0.0f, a, 0.0f);
	carLoc.wheel2 += make_vector(0.0f, b, 0.0f);
	carLoc.wheel3 += make_vector(0.0f, c, 0.0f);
	carLoc.wheel4 += make_vector(0.0f, d, 0.0f);

	carLoc.location += make_vector(0.0f, carLoc.wheel1.y, 0.0f);
}

float rayOctreeIntersection(float3 rayOrigin, float3 rayVec, Octree oct ) {
	std::vector<Triangle*> geometry;

	oct.getGeometry(rayOrigin, rayVec, &geometry);

	float minIns = NULL;
	for (int i = 0; i < geometry.size(); i++) {
		Triangle t = *geometry[i];
		float ins;

		if (rayTriangle(rayOrigin, rayVec, t.p1, t.p2, t.p3, &ins)) {
			if ((minIns) > (ins)|| minIns == NULL) {
				minIns = ins;
			}
		}
	}
	
	if (minIns == 0 || minIns == NULL) {
		return 0;
	}

	return -minIns;
}

void display(void)
{
	checkIntersection();
	drawScene();
	
	glutSwapBuffers();  // swap front and back buffer. This frame will now be displayed.
	CHECK_GL_ERROR();
}

void handleKeys(unsigned char key, int /*x*/, int /*y*/)
{
	switch(key)
	{
	case 27:    /* ESC */
		exit(0); /* dirty exit */
		break;   /* unnecessary, I know */
	case 32:    /* space */
		paused = !paused;
		break;
	case 'w':
	case 'W':
		keysDown[(int)'w'] = true;
		break;
	case 'a':
	case 'A': 
		keysDown[(int)'a'] = true;
		break;
	case 's':   
	case 'S':
		keysDown[(int)'s'] = true;
		break;
	case 'd':   
	case 'D':
		keysDown[(int)'d'] = true;
		break;
	}
}

void handleKeysRelease(unsigned char key, int /*x*/, int /*y*/)
{
	switch (key)
	{
	case 27:    /* ESC */
		exit(0); /* dirty exit */
		break;   /* unnecessary, I know */
	case 32:    /* space */
		paused = !paused;
		break;
	case 'w':
	case 'W':
		keysDown[(int)'w'] = false;
		break;
	case 'a':
	case 'A':
		keysDown[(int)'a'] = false;
		break;
	case 's':
	case 'S':
		keysDown[(int)'s'] = false;
		break;
	case 'd':
	case 'D':
		keysDown[(int)'d'] = false;
		break;
	}
}

void handleSpecialKeys(int key, int /*x*/, int /*y*/)
{
	switch(key)
	{
	case GLUT_KEY_LEFT:
		printf("Left arrow\n");
		camera = (camera + 1) % 8;
		break;
	case GLUT_KEY_RIGHT:
		camera = (camera - 1);
		camera = camera == -1 ? 7 : camera;
		break;
	case GLUT_KEY_UP:
		break;
	case GLUT_KEY_DOWN:
		break;
	}
}

void mouse(int button, int state, int x, int y)
{
	// reset the previous position, such that we only get movement performed after the button
	// was pressed.
	prev_x = x;
	prev_y = y;

	bool buttonDown = state == GLUT_DOWN;

	switch(button)
	{
	case GLUT_LEFT_BUTTON:
		leftDown = buttonDown;
		break;
	case GLUT_MIDDLE_BUTTON:
		middleDown = buttonDown;
		break;
	case GLUT_RIGHT_BUTTON: 
		rightDown = buttonDown;
	default:
		break;
	}
}

void motion(int x, int y)
{
	int delta_x = x - prev_x;
	int delta_y = y - prev_y;

	if(middleDown)
	{
		camera_r -= float(delta_y) * 0.3f;
		// make sure cameraDistance does not become too small
		camera_r = max(0.1f, camera_r);
	}
	if(leftDown)
	{
		camera_phi	-= float(delta_y) * 0.3f * float(M_PI) / 180.0f;
		camera_phi = min(max(0.01f, camera_phi), float(M_PI) - 0.01f);
		camera_theta -= float(delta_x) * 0.3f * float(M_PI) / 180.0f;
	}

	if(rightDown)
	{
		camera_target_altitude += float(delta_y) * 0.1f; 
	}
	prev_x = x;
	prev_y = y;
}

Camera createCamera(float3 position, float3 lookAt, float3 up) {
	Camera cam;
	cam.position = position;
	cam.lookAt = lookAt;
	cam.up = up;

	return cam;
}

void tick() {
	if (keysDown[(int)'w']) {
		float3 term = carLoc.frontDir * carLoc.moveSpeed;

		carLoc.location += term;

	}if (keysDown[(int)'s']) {
		float3 term = carLoc.frontDir * carLoc.moveSpeed;

		carLoc.location -= term;

	}if (keysDown[(int)'a'] && (keysDown[(int)'w'] || keysDown[(int)'s'])) {
		carLoc.angley += carLoc.rotationSpeed;
		carLoc.frontDir = make_rotation_y<float3x3>(carLoc.rotationSpeed) * carLoc.frontDir;
		camera_theta += carLoc.rotationSpeed;

	}if (keysDown[(int)'d'] && (keysDown[(int)'w'] || keysDown[(int)'s'])) {
		carLoc.angley -= carLoc.rotationSpeed;
		carLoc.frontDir = make_rotation_y<float3x3>(-carLoc.rotationSpeed) * carLoc.frontDir;
		camera_theta -= carLoc.rotationSpeed;
	}
}

void idle( void )
{
	static float startTime = float(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f;
	// Here is a good place to put application logic.
	if (!paused)
	{
		currentTime = float(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f - startTime;
	}

	tick();

	// rotate light around X axis, sunlike fashion.
	// do one full revolution every 20 seconds.
	float4x4 rotateLight = make_rotation_x<float4x4>(2.0f * M_PI * currentTime / 20.0f);
	// rotate and update global light position.
	lightPosition = make_vector3(rotateLight * make_vector(30.1f, 450.0f, 0.1f, 1.0f));

	glutPostRedisplay();  
	// Uncommenting the line above tells glut that the window 
	// needs to be redisplayed again. This forces the display to be redrawn
	// over and over again. 
}

int main(int argc, char *argv[])
{
#	if defined(__linux__)
	linux_initialize_cwd();
#	endif // ! __linux__

	ts.reserve(32);

	glutInit(&argc, argv);

	/* Request a double buffered window, with a sRGB color buffer, and a depth
	 * buffer. Also, request the initial window size to be 800 x 600.
	 *
	 * Note: not all versions of GLUT define GLUT_SRGB; fall back to "normal"
	 * RGB for those versions.
	 */
#	if defined(GLUT_SRGB)
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_SRGB | GLUT_DEPTH);
#	else // !GLUT_SRGB
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	printf( "--\n" );
	printf( "-- WARNING: your GLUT doesn't support sRGB / GLUT_SRGB\n" );
#	endif // ~ GLUT_SRGB
	glutInitWindowSize(screenWidth,screenheight);

	/* Require at least OpenGL 3.0. Also request a Debug Context, which allows
	 * us to use the Debug Message API for a somewhat more humane debugging
	 * experience.
	 */
	glutInitContextVersion(3,0);
	glutInitContextFlags(GLUT_DEBUG);

	/* Request window
	 */
	glutCreateWindow("Project");

	/* Set callbacks that respond to various events. Most of these should be
	 * rather self-explanatory (i.e., the MouseFunc is called in response to
	 * a mouse button press/release). The most important callbacks are however
	 *
	 *   - glutDisplayFunc : called whenever the window is to be redrawn
	 *   - glutIdleFunc : called repeatedly
	 *
	 * The window is redrawn once at startup (at the beginning of
	 * glutMainLoop()), and whenever the window changes (overlap, resize, ...).
	 * To repeatedly redraw the window, we need to manually request that via
	 * glutPostRedisplay(). We call this from the glutIdleFunc.
	 */
	glutIdleFunc(idle);
	glutDisplayFunc(display);

	glutKeyboardFunc(handleKeys); // standard key is pressed/released
	glutKeyboardUpFunc(handleKeysRelease);
	glutSpecialFunc(handleSpecialKeys); // "special" key is pressed/released
	glutMouseFunc(mouse); // mouse button pressed/released
	glutMotionFunc(motion); // mouse moved *while* any button is pressed

	/* Now that we should have a valid GL context, perform our OpenGL 
	 * initialization, before we enter glutMainLoop().
	 */
	initGL();

	/* If sRGB is available, enable rendering in sRGB. Note: we should do
	 * this *after* initGL(), since initGL() initializes GLEW.
	 */
	glEnable(GL_FRAMEBUFFER_SRGB);

	/* Start the main loop. Note: depending on your GLUT version, glutMainLoop()
	 * may never return, but only exit via std::exit(0) or a similar method.
	 */
	glutMainLoop();


	return 0;          
}

void drawFullScreenQuad()
{
	static GLuint vertexArrayObject = 0;
	static int nofVertices = 4;

	// do this initialization first time the function is called... somewhat dodgy, but works for demonstration purposes
	if (vertexArrayObject == 0)
	{
		glGenVertexArrays(1, &vertexArrayObject);
		static const float2 positions[] = {
				{ -1.0f, -1.0f },
				{ 1.0f, -1.0f },
				{ 1.0f, 1.0f },
				{ -1.0f, 1.0f },
		};
		createAddAttribBuffer(vertexArrayObject, positions, sizeof(positions), 0, 2, GL_FLOAT);
	}

	glBindVertexArray(vertexArrayObject);
	glDrawArrays(GL_QUADS, 0, nofVertices);
}

void debugDrawLine(const float4x4 &viewMatrix, const float4x4 &projectionMatrix, float3 origin, float3 rayVector) {
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	GLint temp;
	glColor3f(1.0, 1.0, 0.0);
	glGetIntegerv(GL_CURRENT_PROGRAM, &temp);
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(&projectionMatrix.c1.x);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(&viewMatrix.c1.x);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	float3 p[8];
	p[0] = origin;
	p[1] = origin + (normalize(rayVector) * make_vector(1.0f, 5.0f, 1.0f));

	glBegin(GL_LINES);

	glVertex3f(p[0].x, p[0].y, p[0].z);
	glVertex3f(p[1].x, p[1].y, p[1].z);

	glEnd();
	glUseProgram(temp);
	glPopAttrib();
}

void debugDrawQuad(const float4x4 &viewMatrix, const float4x4 &projectionMatrix, float3 origin, float3 halfVector) {
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	GLint temp;
	glColor3f(1.0, 1.0, 0.0);
	glGetIntegerv(GL_CURRENT_PROGRAM, &temp);
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(&projectionMatrix.c1.x);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(&viewMatrix.c1.x);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	float3 p[8];
	p[0] = origin + (halfVector * make_vector(1.0f, 1.0f, 1.0f));
	p[1] = origin + (halfVector * make_vector(-1.0f, 1.0f, 1.0f));

	p[2] = origin + (halfVector * make_vector(1.0f, -1.0f, 1.0f));
	p[3] = origin + (halfVector * make_vector(-1.0f, -1.0f, 1.0f));

	p[4] = origin + (halfVector * make_vector(1.0f, 1.0f, -1.0f));
	p[5] = origin + (halfVector * make_vector(-1.0f, 1.0f, -1.0f));

	p[6] = origin + (halfVector * make_vector(1.0f, -1.0f, -1.0f));
	p[7] = origin + (halfVector * make_vector(-1.0f, -1.0f, -1.0f));


	glBegin(GL_LINES);

	glVertex3f(p[0].x, p[0].y, p[0].z);
	glVertex3f(p[1].x, p[1].y, p[1].z);

	glVertex3f(p[0].x, p[0].y, p[0].z);
	glVertex3f(p[2].x, p[2].y, p[2].z);

	glVertex3f(p[0].x, p[0].y, p[0].z);
	glVertex3f(p[4].x, p[4].y, p[4].z);

	///////
	glVertex3f(p[7].x, p[7].y, p[7].z);
	glVertex3f(p[6].x, p[6].y, p[6].z);

	glVertex3f(p[7].x, p[7].y, p[7].z);
	glVertex3f(p[5].x, p[5].y, p[5].z);

	glVertex3f(p[7].x, p[7].y, p[7].z);
	glVertex3f(p[3].x, p[3].y, p[3].z);

	///////
	glVertex3f(p[5].x, p[5].y, p[5].z);
	glVertex3f(p[1].x, p[1].y, p[1].z);

	glVertex3f(p[5].x, p[5].y, p[5].z);
	glVertex3f(p[4].x, p[4].y, p[4].z);

	///////
	glVertex3f(p[3].x, p[3].y, p[3].z);
	glVertex3f(p[1].x, p[1].y, p[1].z);

	glVertex3f(p[3].x, p[3].y, p[3].z);
	glVertex3f(p[2].x, p[2].y, p[2].z);

	///////
	glVertex3f(p[6].x, p[6].y, p[6].z);
	glVertex3f(p[4].x, p[4].y, p[4].z);

	glVertex3f(p[6].x, p[6].y, p[6].z);
	glVertex3f(p[2].x, p[2].y, p[2].z);

	glEnd();
	glUseProgram(temp);
	glPopAttrib();
}

void debugDrawOctree(const float4x4 &viewMatrix, const float4x4 &projectionMatrix, Octree tree)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	GLint temp;
	glColor3f(1.0, 1.0, 0.0);
	glGetIntegerv(GL_CURRENT_PROGRAM, &temp);
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(&projectionMatrix.c1.x);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(&viewMatrix.c1.x);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	float3 p[8];
	p[0] = tree.origin + (tree.halfVector * make_vector(1.0f, 1.0f, 1.0f));
	p[1] = tree.origin + (tree.halfVector * make_vector(-1.0f, 1.0f, 1.0f));

	p[2] = tree.origin + (tree.halfVector * make_vector(1.0f, -1.0f, 1.0f));
	p[3] = tree.origin + (tree.halfVector * make_vector(-1.0f, -1.0f, 1.0f));

	p[4] = tree.origin + (tree.halfVector * make_vector(1.0f, 1.0f, -1.0f));
	p[5] = tree.origin + (tree.halfVector * make_vector(-1.0f, 1.0f, -1.0f));

	p[6] = tree.origin + (tree.halfVector * make_vector(1.0f, -1.0f, -1.0f));
	p[7] = tree.origin + (tree.halfVector * make_vector(-1.0f, -1.0f, -1.0f));


	glBegin(GL_LINES);

	glVertex3f(p[0].x, p[0].y, p[0].z);
	glVertex3f(p[1].x, p[1].y, p[1].z);

	glVertex3f(p[0].x, p[0].y, p[0].z);
	glVertex3f(p[2].x, p[2].y, p[2].z);

	glVertex3f(p[0].x, p[0].y, p[0].z);
	glVertex3f(p[4].x, p[4].y, p[4].z);

	///////
	glVertex3f(p[7].x, p[7].y, p[7].z);
	glVertex3f(p[6].x, p[6].y, p[6].z);

	glVertex3f(p[7].x, p[7].y, p[7].z);
	glVertex3f(p[5].x, p[5].y, p[5].z);

	glVertex3f(p[7].x, p[7].y, p[7].z);
	glVertex3f(p[3].x, p[3].y, p[3].z);

	///////
	glVertex3f(p[5].x, p[5].y, p[5].z);
	glVertex3f(p[1].x, p[1].y, p[1].z);

	glVertex3f(p[5].x, p[5].y, p[5].z);
	glVertex3f(p[4].x, p[4].y, p[4].z);

	///////
	glVertex3f(p[3].x, p[3].y, p[3].z);
	glVertex3f(p[1].x, p[1].y, p[1].z);

	glVertex3f(p[3].x, p[3].y, p[3].z);
	glVertex3f(p[2].x, p[2].y, p[2].z);

	///////
	glVertex3f(p[6].x, p[6].y, p[6].z);
	glVertex3f(p[4].x, p[4].y, p[4].z);

	glVertex3f(p[6].x, p[6].y, p[6].z);
	glVertex3f(p[2].x, p[2].y, p[2].z);

	glEnd();
	glUseProgram(temp);
	glPopAttrib();
	

	std::vector<Octree> children;
	tree.getChildren(&children);

	if (children.size() != 0) {
		for (int i = 0; i < 8; i++) {
			debugDrawOctree(viewMatrix, projectionMatrix, children[i]);
		}
	}
}


/* TIME COUNT
high_resolution_clock::time_point start = high_resolution_clock::now();
high_resolution_clock::time_point end = high_resolution_clock::now();
duration<double> time_span = duration_cast<duration<double>>(end - start);
printf("TIME FOR COLL:%f\n", time_span.count());
*/