#pragma once

#include "UtilityFunctions.h"
#include "Configuration.h"
#include "Geometry.h"
#include "Shader.h"
#include "Image.h"

using namespace UtilityFunctions;
using namespace std;

//////////////////////////////////////////////////////////////////////////////////////////////////
class GLApplication
{
public:
	GLApplication(const string& appName, size_t topLeftX = 100, size_t topLeftY = 50, size_t width = 1400, size_t height = 900);
	~GLApplication();

	void printVersionHistory();
	void printHelp();

	void buildScene(const string& imageFilename); // e.g.: g170204.dat

	void run();

	/// begin - getters / accessors
	const string	getAppName() { return _appName; };
	void			getWindowSize(size_t& width, size_t& height);
	/// end - getters / accessors

	/// begin - setters
	void	switchTileBoundariesRendering();
	/// end - setters

	/// begin - very simple navigation interface
	void	moveCameraForward();
	void	moveCameraBackward();
	void	moveCameraLeft();
	void	moveCameraRight();
	void	moveCameraUp();
	void	moveCameraDown();
	void	rotateViewLeft();
	void	rotateViewRight();
	void	gotoHomePositionAndView();
	void	switchProjection();
	/// end - very simple navigation interface

protected:
	string					_version;

	GLFWwindow*				_glWindow;
	string					_appName;
	size_t					_width, _height;
	
	Configuration			_configuration;
	TileGeometry*			_fullMapGeometry;
	vector<TileGeometry*>	_tileGeometryObjects;
	CameraGeometry*			_cameraGeometry;
	BasicShader*			_basicShader;

	glm::vec3				_bbLL, _bbUR; // bound box extents
	glm::vec3				_cameraPos, _cameraDir; // camera related
	bool					_projectionOrtho;
	glm::mat4				_projection;
	glm::mat4				_model;
	glm::mat4				_view;
	glm::mat4				_toEnu;
	float					_rotationAngle; // in degrees

	glm::vec3				_xAxis;
	glm::vec3				_yAxis;
	glm::vec3				_zAxis;

	bool					_readyToRun;

	void	initStates();
	void	release();
	void	computeMatrices();
	void	updateHeading();
	void	computeBoundingBox();

	void	startRendering();
	void	updatePass();
	void	cullPass();
	void	preRenderPass();
	void	renderPass();
	void	postRenderPass();
};