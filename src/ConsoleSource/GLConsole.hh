// $Id$

#ifndef __GLCONSOLE_HH__
#define __GLCONSOLE_HH__

#include "GLUtil.hh"
#ifdef __OPENGL_AVAILABLE__

#ifdef HAVE_SDL_IMAGE_H
#include "SDL_image.h"
#else
#include "SDL/SDL_image.h"
#endif

#include "OSDConsoleRenderer.hh"


class GLConsole : public OSDConsoleRenderer
{
	public:
		GLConsole();
		virtual ~GLConsole();

		virtual bool loadFont(const std::string &filename);
		virtual bool loadBackground(const std::string &filename);
		virtual void drawConsole();
		virtual void updateConsole();

	private:
		int powerOfTwo(int a);
		bool loadTexture(const std::string &filename, GLuint &texture,
		                 int &width, int &height, GLfloat *texCoord);

		GLuint backgroundTexture;
		BackgroundSetting* backgroundSetting;
		FontSetting* fontSetting;
		GLfloat backTexCoord[4];
		int consoleWidth;
		int consoleHeight;
		int dispX;
		int dispY;
		class Console* console;
		void updateConsoleRect(SDL_Surface *screen);
};

#endif // __OPENGL_AVAILABLE__
#endif // __GLCONSOLE_HH__
