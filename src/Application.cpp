#include "Application.h"

//////////////////////////////////////////////////////////////////////////////////
static GLApplication* __glApp = NULL; // TODO - redo to remove this instance
static void processKeyEvent(GLFWwindow* window, int key, int scan, int action, int mods);
//////////////////////////////////////////////////////////////////////////////////

GLApplication::GLApplication(const string& appName, size_t topLeftX, size_t topLeftY, size_t width, size_t height)
{
	_appName = appName.empty() ? "GLApplication" : appName;
	_width = width < 256 ? 256 : width > 2048 ? 2048 : width;
	_height = height < 256 ? 256 : height > 2048 ? 2048 : height;
	_readyToRun = false;

	printVersionHistory();

    cout << "System is initializing, Please wait..." << endl;

	_projectionOrtho = _configuration.getIsIntialProjectionOrtho();
	_rotationAngle = 90.0f;
	_basicShader = NULL;
	_fullMapGeometry = NULL;
	_cameraGeometry = NULL;

	_xAxis = glm::vec3(1, 0, 0);
	_yAxis = glm::vec3(0, 1, 0);
	_zAxis = glm::vec3(0, 0, 1);

	// east north up - coord sys transformer helper
	_toEnu = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), _xAxis); 
	//

	if (!glfwInit())
    {
        cout << __FUNCTION__ << "Error, Init GLFW failed" << endl;
        
		exit(EXIT_FAILURE);
    }

	glfwWindowHint(GLFW_DEPTH_BITS,  32);

    _glWindow = glfwCreateWindow((int)_width, (int)_height, _appName.c_str(), NULL, NULL);
    if (!_glWindow)
    {
        cout << "Error, GLFW window creation failed" << endl;
        glfwTerminate();
        
		exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(_glWindow); // make this context current
    glfwSwapInterval(1);

	/// init gl extensions using extension wrangler - it will to access 3.0 or later gl fuctionalities
	{
		glewExperimental = GL_TRUE;
		
		GLenum errorCode = glewInit();
		if (errorCode != GLEW_OK) 
		{
			cout << __FUNCTION__ << "Error, glewInit failed: " << glewGetErrorString(errorCode) << endl;
			glfwTerminate();
			
			exit(EXIT_FAILURE);
		}
	}
	//

	glfwSetWindowPos(_glWindow, (int)topLeftX, (int)topLeftY); // place the window starting here
	
	/// setup call backs - TODO: redo using std::function() or something...
	{
		glfwSetKeyCallback(_glWindow, processKeyEvent);

		// glfwSetFramebufferSizeCallback(_glWindow, reshape); // not needed for now
		// int w, h;
		// glfwGetFramebufferSize(_glWindow, &w, &h);
		// reshape(_glWindow, w, h);
	}
	///

	initStates();

	// print some gl info here...
	{
		const GLubyte *renderer = glGetString(GL_RENDERER);
		const GLubyte *vendor = glGetString(GL_VENDOR);
		const GLubyte *version = glGetString(GL_VERSION);
		const GLubyte *glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);

		GLint major, minor;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MINOR_VERSION, &minor);

		cout << "GL Renderer  : " << renderer << "\n";;
		cout << "GL Vendor    : " << vendor << "\n";
		cout << "GL Version   : " << version << "\n";
		cout << "GL Version   : " << major << "." <<  minor << "\n";
		cout << "GLSL Version : " << glslVersion << std::endl;
	}

	__glApp = this;

	logGLError(__FUNCTION__);
}

GLApplication::~GLApplication()
{
	release();

	glfwTerminate();

	cout << __FUNCTION__ << " application ended." << endl;
	cout << "Thank you" << endl;
}

void GLApplication::printVersionHistory()
{
	_version = "GLApplication ver: 1.0 by M Pai (c) M Pai 06/18/2017 mpai@hotmail.com";

	cout << "---------------------------------------------------------------------" << endl;
	cout << _version << endl;
	cout << "=====================================================================" << endl << endl;
	cout << "\t------------------------------------------------" << endl;
	cout << "\t A Simple OpenGL program to render a Air Photo" << endl;
	cout << "\t================================================" << endl << endl;
}

void GLApplication::getWindowSize(size_t& width, size_t& height)
{
	width = _width;
	height = _height;
}

void GLApplication::switchTileBoundariesRendering()
{
	for (TileGeometry* tile : _tileGeometryObjects)
		tile->switchTileBoundariesRendering();
}

 
/// init gl
///		- enable need features
///		- disable not need features
///
void GLApplication::initStates()
{
	/// begin - enable features
	glEnable(GL_DEPTH_TEST);
	/// end - enable features

	/// begin - gl disable features
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);
	glDisable(GL_CULL_FACE);
	/// end  - gl disable features

	/// clear both of double buffer with blueish color
	glClearColor(0.0f, 0.0f, 0.33f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
	glfwSwapBuffers(_glWindow);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
	glfwSwapBuffers(_glWindow);
	///

	logGLError(__FUNCTION__);
}

void GLApplication::release()
{
	for (TileGeometry* tile : _tileGeometryObjects)
		delete tile;

	_tileGeometryObjects.clear();

	if (_fullMapGeometry)
		delete _fullMapGeometry;

	_fullMapGeometry = NULL;

	if (_cameraGeometry != NULL)
		delete _cameraGeometry;

	if (_basicShader)
		delete _basicShader;

	_cameraGeometry = NULL;
	_basicShader = NULL;
}

void GLApplication::buildScene(const string& imageFilename)
{
	glfwHideWindow(_glWindow); // hide the window, until the scene is built

	release(); // to delete old geometry, if any

	ImageBuffer* fullMapBuffer = ImageFactory::getImage(imageFilename);

	if (fullMapBuffer == NULL || fullMapBuffer->getBuffer() == NULL)
	{
		cout << __FUNCTION__ << "Error, input image file " << imageFilename << " not read properly" << endl;
		return;
	}

	size_t	texWidth, texHeight;
	float	pixelSize = _configuration.getPixelSize();

	fullMapBuffer->getBufferDimension(texWidth, texHeight);

	float fullTileWidth  = (float)texWidth * pixelSize;
	float fullTileHeight = (float)texHeight * pixelSize;

	glm::vec3   fullTileDimension(fullTileWidth, fullTileHeight, 0.0f);
	
	glm::vec3	fullTileLL(0.0f, 0.0f, 0.0f);
	glm::vec3	fullTileUR = fullTileLL + fullTileDimension;  

	_fullMapGeometry = new TileGeometry(fullTileLL, fullTileUR, fullMapBuffer->generateTextureId());

	float tileTexSize = (float) _configuration.getTileTexSize();

	float tilesX = fullTileWidth  / (tileTexSize * pixelSize);
	float tilesY = fullTileHeight / (tileTexSize * pixelSize);

	float tileWidth  = fullTileWidth / tilesX;
	float tileHeight = fullTileWidth / tilesY;

	glm::vec3 tileDimension = glm::vec3(tileWidth, tileHeight, 0.0f);

	// construct row x col tiles bottom up
	//
	for (size_t row = 0; row < tilesY; row++)
	{	
		for (size_t col = 0; col < tilesX; col++)
		{
			///
			size_t rowPixelIndex = row * (size_t) tileTexSize;
			size_t colPixelIndex = col * (size_t) tileTexSize;
				
			GLuint subTexId = fullMapBuffer->getTile(rowPixelIndex, colPixelIndex, (size_t) tileTexSize, (size_t) tileTexSize);
			///

			///
			glm::vec3 ll = glm::vec3(row * tileWidth, col * tileHeight, 0.0f); // REDO: enu coord system
			glm::vec3 ur = ll + tileDimension;

			TileGeometry* tileGeometry = new TileGeometry(ll, ur, subTexId);
			///

			if (tileGeometry != NULL)
				_tileGeometryObjects.push_back(tileGeometry);

			if (_tileGeometryObjects.size() % 25 == 0)
				cout << __FUNCTION__ << " Constructed: " << _tileGeometryObjects.size() << " tiles..." << endl;
		}
	}

	cout << __FUNCTION__ << " Final tile count: " << _tileGeometryObjects.size() << endl;

	/// build camara icon geometry
	{
		glm::vec3 black(32, 32, 32);

		ImageBuffer* flatImageBlack = ImageFactory::getFlatColorImage(black, 32, 32);

		_cameraGeometry = new CameraGeometry(flatImageBlack->generateTextureId(), 24.0f);

		delete flatImageBlack;
	
		cout << __FUNCTION__ << " built the camara icon geometry " << endl;
	}
	///

	_basicShader = new BasicShader();

	cout << __FUNCTION__ << " built the shader " << endl;

	delete fullMapBuffer; // no longer needed so release it

	/// all set, set background color to be black
	_readyToRun = true;
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); 
	///

	glfwShowWindow(_glWindow); // unhide the window, now that the scene is built
}

void
GLApplication::updatePass()
{
	static glm::mat4	identity(1.0f);
	int					winWidth, winHeight;

	/// take care of viewport first
	glfwGetFramebufferSize(_glWindow, &winWidth, &winHeight);
	
	_width  = (size_t)winWidth;
	_height = (size_t)winHeight;

	glViewport(0, 0, (GLsizei)_width, (GLsizei)_height);
	///

	/// begin - compute model, view and projection matrices next
	///
	if (_projectionOrtho == true)
	{
		float mapSize = _configuration.getMapSize();
		float mapSizeB2 = mapSize / 2.0f;
	
		_projection = glm::ortho(0.0f, mapSize, 0.0f, mapSize, -mapSize, mapSize);

		glm::mat4 transMinus = glm::translate(identity, glm::vec3(-mapSizeB2, -mapSizeB2, 0));
		glm::mat4 rotate     = glm::rotate(identity, glm::radians(_rotationAngle), _zAxis);
		glm::mat4 transPlus  = glm::translate(identity, glm::vec3(mapSizeB2, mapSizeB2, 0));

		_view =  transPlus * rotate * transMinus;
		_model = identity;
	}
	else
	{
		GLfloat aspect = (GLfloat) winHeight / (GLfloat) winWidth;

		_projection = glm::perspective(_configuration.getFOV(), aspect, _configuration.getNear(), _configuration.getFar());

		/// begin - calculation are in enu from now on
		///
		glm::vec3 cameraDir = glm::normalize(glm::vec3(_cameraDir.x, 0.0, _cameraDir.z));

		glm::vec3 cameraPos = glm::vec3(_cameraPos.x, -_cameraPos.z, _cameraPos.y); // in enu
		
		_view = glm::lookAt(cameraPos, cameraPos - cameraDir, _yAxis);

		_model = _toEnu; // this will render all 3d geometry models in enu coord sys
		///
		/// end - calculation are in enu from now on

		_cameraGeometry->setLocationDirection(_cameraPos, _cameraDir); // 
	}
	/// end - compute model, view and projection matrices next
}

void
GLApplication::cullPass()
{
	// TODO: implement culling to buy performance
	
	// emtpy for now

	//	logGLError(__FUNCTION__);
}

void
GLApplication::preRenderPass()
{
	// TODO: implement pre render - as in multi target rendering etc
	
	// empty for now

	// logGLError(__FUNCTION__);
}

void
GLApplication::renderPass()
{
	/// must clear color and depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	///

	/// begin - render all small tiles
	glm::mat4 modelView = _view * _model;

	/// update the shader uniforms
	_basicShader->enable();
	_basicShader->setProjectionMatrix(_projection);
	_basicShader->setModelViewMatrix(modelView);
	///

	for (TileGeometry* tile : _tileGeometryObjects)
		tile->render();
	
	_basicShader->disable();
	/// end - render all small tiles

	logGLError(__FUNCTION__);
}

void
GLApplication::postRenderPass()
{
	// render camera icon geomentry
	// render large preview map tile geomentry near lower left

	static glm::mat4 identity = glm::mat4(1.0f);

	bool enableHUD = true; // still a wip

	if (_projectionOrtho)
	{
		// float aspect = (float)_width / (float) _height;

		glm::vec3 cameraPos = glm::vec3(_width / 2.0f, _height / 5.0f, _configuration.getCameraInitialPos().z);

		_cameraGeometry->setLocationDirection(cameraPos, _cameraDir);

		float mapSize = _configuration.getMapSize();

		_projection = glm::ortho(0.0f, mapSize, 0.0f, mapSize, -mapSize, mapSize);

		glm::mat4 modelRotate = glm::rotate(identity, glm::radians(_rotationAngle), _zAxis);

		glm::mat4 modelView = glm::translate(identity, cameraPos) * modelRotate;

		_basicShader->enable();
		_basicShader->setProjectionMatrix(_projection);
		_basicShader->setModelViewMatrix(modelView);
		_cameraGeometry->render();
		_basicShader->disable();

		logGLError(__FUNCTION__);
	}

	if (enableHUD)
	{
		// WIP, not ready
		// TODO: re-implement HUD to enable preview
		// for debugging also

		GLsizei previewWidth =  (GLsizei) _width  / 8;
		GLsizei previewHeight = (GLsizei) _height / 8;

		float mapSize = _configuration.getMapSize();
		float mapSizeB2 = mapSize / 2.0f;
	
		_projection = glm::ortho(0.0f, mapSize, 0.0f, mapSize, -mapSize, mapSize);

		glm::mat4 transMinus = glm::translate(identity, glm::vec3(-mapSizeB2, -mapSizeB2, 0));
		glm::mat4 rotate     = glm::rotate(identity, glm::radians(_rotationAngle), _zAxis);
		glm::mat4 transPlus  = glm::translate(identity, glm::vec3(mapSizeB2, mapSizeB2, 0));

		_model = identity;
		_view =  transPlus * rotate * transMinus;

		glm::mat4 modelView = _view * _model;

		glDisable(GL_DEPTH_TEST);

		glViewport((GLsizei)_width - previewWidth, (GLsizei)0, (GLsizei)previewWidth, (GLsizei)previewHeight);

		_basicShader->enable();
		_basicShader->setProjectionMatrix(_projection);
		_basicShader->setModelViewMatrix(modelView);
		_fullMapGeometry->render();
		_basicShader->disable();
			
		glEnable(GL_DEPTH_TEST);
	
		logGLError(__FUNCTION__);
	}
}

void
GLApplication::computeBoundingBox()
{
	/// begin - compute bounding box here
	float		bigNum = 10e6;
	glm::vec3	ll, ur;
	
	_bbLL = glm::vec3( bigNum,  bigNum,  bigNum);    // bounding box ll
	_bbUR = glm::vec3(-bigNum, -bigNum, -bigNum); // bounding box ur

	for (TileGeometry* tile : _tileGeometryObjects)
	{
		tile->getBBoxExtents(ll, ur);

		UtilityFunctions::getResizeExtents(ll, _bbLL, _bbUR);
		UtilityFunctions::getResizeExtents(ur, _bbLL, _bbUR);
	}

	cout << __FUNCTION__ << " bounding box  lower left: " << _bbLL.x << ", " << _bbLL.y << ", " << _bbLL.z << endl;
	cout << __FUNCTION__ << " bounding box upper right: " << _bbUR.x << ", " << _bbUR.y << ", " << _bbUR.z << endl;
	/// end - compute bounding box here
}

void GLApplication::startRendering()
{
	computeBoundingBox();

	gotoHomePositionAndView();

	printHelp();

	std::cout << __FUNCTION__ << " rendering started... " << endl;
	
	while (!glfwWindowShouldClose(_glWindow))
	{
		updatePass();

		cullPass();

		preRenderPass();

		renderPass();

		postRenderPass();

		glfwSwapBuffers(_glWindow); // swap the buffer to display it

		glfwPollEvents();           // handle next key press event
	}

	std::cout << __FUNCTION__ << " rendering ended... " << endl;
}

void
GLApplication::run()
{
	std::cout << __FUNCTION__ << " started..." << endl;

	if (_readyToRun)
		startRendering();
	else
	{
		glfwShowWindow(_glWindow);

		while (!glfwWindowShouldClose(_glWindow))
		{
			glfwSwapBuffers(_glWindow); // swap the buffer to display it
			glfwPollEvents();           // handle next key press event

			std::this_thread::sleep_for (std::chrono::seconds(1));	
			std::cout << __FUNCTION__ << " not rendering, press ESC to exit ..." << endl;
		}
	}

	std::cout << __FUNCTION__ << " done running, exiting..." << endl;
}

void GLApplication::moveCameraForward()
{
	// in enu coord sys
	_cameraPos.x += _configuration.getKeyPressIncrement();
}

void GLApplication::moveCameraBackward()
{
	// in enu coord sys
	_cameraPos.x -= _configuration.getKeyPressIncrement();
}

void GLApplication::moveCameraLeft()
{
	// in enu coord sys
	_cameraPos.y -= _configuration.getKeyPressIncrement();
}

void GLApplication::moveCameraRight()
{
	// in enu coord sys
	_cameraPos.y += _configuration.getKeyPressIncrement();
}

void GLApplication::moveCameraUp()
{
	_cameraPos.z += _configuration.getKeyPressIncrement();
}

void GLApplication::moveCameraDown()
{
	_cameraPos.z -= _configuration.getKeyPressIncrement();
}

void GLApplication::updateHeading()
{
	// xy plane
	_cameraDir.x = cos(glm::radians(_rotationAngle));
	_cameraDir.y = sin(glm::radians(_rotationAngle));
	_cameraDir.z = 0.0f;
	//

	_cameraDir = glm::normalize(_cameraDir);

	cout << __FUNCTION__ << " _rotationAngle: " << _rotationAngle << endl;
	cout << __FUNCTION__ << " camera Dir: " << _cameraDir.x << ", " << _cameraDir.y << ", "  
		 << _cameraDir.z << endl << endl;
}

void GLApplication::rotateViewLeft()
{
	_rotationAngle += _configuration.getRotationAngleIncrement();
	updateHeading();
}

void GLApplication::rotateViewRight()
{
	_rotationAngle -= _configuration.getRotationAngleIncrement();
	updateHeading();
}

void GLApplication::gotoHomePositionAndView()
{
	_cameraPos = _configuration.getCameraInitialPos();
	_cameraDir = _configuration.getCameraInitialDir();

	if (_cameraDir.x == 0.0f) // TODO: use fuzzy logic to 
	{
		_rotationAngle = 90.0;
	}
	else
	{
		// TODO: test this logic
		float tanValue = _cameraDir.x / _cameraDir.z;
		_rotationAngle = glm::degrees(atan(tanValue));
	}	

	updateHeading();
}

void GLApplication::switchProjection()
{
	_projectionOrtho = !_projectionOrtho;
}

void GLApplication::printHelp()
{
	cout << "------------------------ help ---------------------------------" << endl;
	cout << "Key press events:" << endl;
	cout << "'ESC': end the application" << endl << endl; 
	cout << "' ' : (space bar) toggles between orthographic and perspective projection" << endl;
	cout << "'L' : rotate clockwise by 10 degrees" << endl;
	cout << "'R' : rotate counter clockwise by 10 degrees" << endl << endl;
    cout << "Additional key press events to help in debugging..." << endl;
	cout << "'H' : reset the view to home position" << endl;
	cout << "'B' : toggle displaying triangle boundaries (the black lines)"  << endl;
	cout << "'W' : advances the camera NORTH"  << endl;
	cout << "'S' : advances the camera SOUTH"  << endl;
	cout << "'A' : advances the camera WEST"  << endl;
	cout << "'D' : advances the camera EAST"  << endl;
	cout << "'Q' : advances the camera UP"  << endl;
	cout << "'S' : advances the camera DOWN"  << endl << endl;
	cout << "'P' : print this help"  << endl;
	cout << "==============================================================" << endl << endl;
}
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
/// TODO: redo this function use: std::function() or something like that
/////////////////////////////////////////////////////////////////////////////////
void processKeyEvent(GLFWwindow* window, int key, int scan, int action, int mods)
{
	if (action == GLFW_RELEASE || __glApp == NULL)
		return;

	switch (key) 
	{
		case GLFW_KEY_W:
			__glApp->moveCameraForward();
		break;
		
		case GLFW_KEY_S:
			__glApp->moveCameraBackward();
		break;
		
		case GLFW_KEY_A:
			__glApp->moveCameraLeft();
		break;
		
		case GLFW_KEY_D:
			__glApp->moveCameraRight();
		break;

		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;

		case GLFW_KEY_Q:
		case GLFW_KEY_UP:
			__glApp->moveCameraUp();
		break;
	
		case GLFW_KEY_Z:
		case GLFW_KEY_DOWN:
			__glApp->moveCameraDown();
		break;
		
		case GLFW_KEY_L:
			__glApp->rotateViewLeft();
		break;

		case GLFW_KEY_RIGHT:
		case GLFW_KEY_R:
			__glApp->rotateViewRight();
		break;

		case GLFW_KEY_LEFT:
		case GLFW_KEY_SPACE:
			__glApp->switchProjection();
		break;

		case GLFW_KEY_H:
			__glApp->gotoHomePositionAndView();
		break;

		case GLFW_KEY_B:
			__glApp->switchTileBoundariesRendering();
		break;

		case GLFW_KEY_P:
			__glApp->printHelp();
		break;


		default:
			// ignore all other key press events
		break;
	}
}
/////////////////////////////////////////////////////////////////////////////////
