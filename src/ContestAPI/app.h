//---------------------------------------------------------------------------------
// App.h
// Provides a number of basic helper functions that wrap around thing like rendering, sound, and input handling
//---------------------------------------------------------------------------------
#ifndef _APP_H
#define _APP_H

#if BUILD_PLATFORM_WINDOWS
//Have game project import main function
#pragma comment(linker, "/include:wWinMain")

//---------------------------------------------------------------------------------
#include <windows.h> 
#endif //BUILD_PLATFORM_WINDOWS

//---------------------------------------------------------------------------------
#include "freeglut_config.h"

//---------------------------------------------------------------------------------
#include "AppSettings.h"
#include "SimpleController.h"
#include "SimpleSprite.h"

#define APP_VIRTUAL_TO_NATIVE_COORDS(_x_,_y_)			_x_ = ((_x_ / APP_VIRTUAL_WIDTH )*2.0f) - 1.0f; _y_ = ((_y_ / APP_VIRTUAL_HEIGHT)*2.0f) - 1.0f;
#define APP_NATIVE_TO_VIRTUAL_COORDS(_x_,_y_)			_x_ = ((_x_ + 1.0f) * APP_VIRTUAL_WIDTH) / 2.0f; _y_ = ((_y_ + 1.0f) * APP_VIRTUAL_HEIGHT) / 2.0f;

//---------------------------------------------------------------------------------
// App namespace: These are the IO calls you can use for you game.
//---------------------------------------------------------------------------------
namespace App
{
	//*******************************************************************************************
	// App Keycodes
	//*******************************************************************************************
	//-------------------------------------------------------------------------------------------
	// Platform Independent way of managing ascii keys & special keys
	//-------------------------------------------------------------------------------------------
	enum Key
	{
		//Letter Keys
		KEY_A,
		KEY_B,
		KEY_C,
		KEY_D,
		KEY_E,
		KEY_F,
		KEY_G,
		KEY_H,
		KEY_I,
		KEY_J,
		KEY_K,
		KEY_L,
		KEY_M,
		KEY_N,
		KEY_O,
		KEY_P,
		KEY_Q,
		KEY_R,
		KEY_S,
		KEY_T,
		KEY_U,
		KEY_V,
		KEY_W,
		KEY_X,
		KEY_Y,
		KEY_Z,

		//Number Keys
		KEY_1,
		KEY_2,
		KEY_3,
		KEY_4,
		KEY_5,
		KEY_6,
		KEY_7,
		KEY_8,
		KEY_9,
		KEY_0,

		//Special Keys
		KEY_SPACE,
		KEY_ESC,
		KEY_ENTER,
		KEY_TAB,
		KEY_LEFT,
		KEY_RIGHT,
		KEY_UP,
		KEY_DOWN,
		KEY_HOME,
		KEY_END,
		KEY_INSERT,
	};

	//*******************************************************************************************
	// Display Calls.
	//*******************************************************************************************
	//-------------------------------------------------------------------------------------------
	// Draw a 2D Line from sx,sy to ex,ey using color r=red, g=green, b=blue.
	//-------------------------------------------------------------------------------------------
	void DrawLine(const float sx, const float sy, const float ex, const float ey, const float r = 1.0f, const float g = 1.0f, const float b = 1.0f);

	//-------------------------------------------------------------------------------------------
	// Draws a triangle from the pixel coordinates (or NDC if APP_USE_VIRTUAL_RES is undefined) and the z and w coordinates in OpenGL's NDC (-1 to 1) (default depth test is GL_LESS).
	// Each vertex can have a different color.
	// There is also the option to draw the triangle as a wireframe or filled.
	//-------------------------------------------------------------------------------------------
	void DrawTriangle(const float p1x, const float p1y, const float p1z, const float p1w,
					  const float p2x, const float p2y, const float p2z, const float p2w,
					  const float p3x, const float p3y, const float p3z, const float p3w,
					  const float r1, const float g1, const float b1,
					  const float r2, const float g2, const float b2,
					  const float r3, const float g3, const float b3,
					  const bool isWireframe = false);

	//-------------------------------------------------------------------------------------------
	// Print text to screen at x,y coordinates using color r=red, g=green, b=blue using the passed GLUT font. 
	// Color values are in the range 0.0f to 1.0f.
	// Available fonts...
	// GLUT_BITMAP_9_BY_15, GLUT_BITMAP_8_BY_13, GLUT_BITMAP_TIMES_ROMAN_10, GLUT_BITMAP_TIMES_ROMAN_24
	// GLUT_BITMAP_HELVETICA_10, GLUT_BITMAP_HELVETICA_12, GLUT_BITMAP_HELVETICA_18	
	//-------------------------------------------------------------------------------------------
	void Print(const float x, const float y, const char* text, const float r = 1.0f, const float g = 1.0f, const float b = 1.0f, void* font = GLUT_BITMAP_HELVETICA_18);

	//-------------------------------------------------------------------------------------------
	// Creates a sprite object from the given texture file. File must be BMP, JPG, or PNG format.
	// The columns and rows parameters define the number of columns and rows of sprite animation
	// frames in the given image. These row/column numbers start at 1 (not 0).
	// You can then use the CSimpleSprite methods to animate/move etc.
	//-------------------------------------------------------------------------------------------
	CSimpleSprite* CreateSprite(const char* fileName, const int columns, const int rows);

	//*******************************************************************************************
	// Sound handling.	
	//*******************************************************************************************
	//-------------------------------------------------------------------------------------------
	// Plays the passed WAV, MP3, or FLAC file. Set looping to true if you want the sound to loop.
	// If the sound is already playing it will restart.
	//-------------------------------------------------------------------------------------------
	void PlayAudio(const char* fileName, const bool isLooping = false);

	//-------------------------------------------------------------------------------------------	
	// Stops the given audio file from playing.
	//-------------------------------------------------------------------------------------------	
	void StopAudio(const char* fileName);

	//-------------------------------------------------------------------------------------------	
	// Returns true if the given audio file is currently playing.
	//-------------------------------------------------------------------------------------------	
	bool IsSoundPlaying(const char* filename);

	//*******************************************************************************************
	// Input handling.	
	//*******************************************************************************************
	//-------------------------------------------------------------------------------------------
	// Returns true if the given key is currently being pressed. Uses custom App keys
	// Doesn't support extended functions
	// e.g.
	// IsKeyPressed(App:KEY_A); // Is A pressed
	// IsKeyPressed(App::KEY_LEFT; // Is left arrow pressed
	//-------------------------------------------------------------------------------------------
	bool IsKeyPressed(const Key key);

	//-------------------------------------------------------------------------------------------
	// Sets the value of the passed in float references to the current position of the mouse pointer.
	//-------------------------------------------------------------------------------------------
	void GetMousePos(float& x, float& y);

	//-------------------------------------------------------------------------------------------
	// Checks if mouse button is pressed. Uses GLUT constants
	// e.g.
	// IsMousePressed(GLUT_LEFT_BUTTON)
	// Available buttons: GLUT_LEFT_BUTTON, GLUT_MIDDLE_BUTTON, and GLUT_RIGHT_BUTTON
	//-------------------------------------------------------------------------------------------
	bool IsMousePressed(int button);

	//-------------------------------------------------------------------------------------------
	// Returns a reference to a game controller. 
	// You can use this object to query gamepad buttons and analog stick positions etc.
	// e.g.
	// GetController(0).CheckButton(XINPUT_GAMEPAD_A, false); Returns true if the A button of pad 0 was pressed and repeat if held.
	// GetController(0).CheckButton(XINPUT_GAMEPAD_A, true); Returns true if the A button of pad 0 was pressed but DO NOT repeat if held.	
	// GetController(0).GetLeftThumbStickX(); Returns a float between -1.0f to 1.0f representing the position of the left thumb sticks X Axis.
	// See SimpleController.h for more info.
	//-------------------------------------------------------------------------------------------
	const CController& GetController(const int pad = 0);
};
#endif //_APP_H