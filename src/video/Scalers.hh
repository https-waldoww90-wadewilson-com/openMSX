// $Id$

#ifndef __SCALERS_HH__
#define __SCALERS_HH__

#include <vector>
#include <SDL/SDL.h>
#include "Blender.hh"

using std::vector;

namespace openmsx {

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
class Scaler
{
public:
	/** Enumeration of Scalers known to openMSX.
	  */
	enum ScalerID {
		/** SimpleScaler. */
		SIMPLE,
		/** SaI2xScaler, naieve. */
		SAI2X,
	};

	/** Returns an array containing an instance of every Scaler subclass,
	  * indexed by ScaledID.
	  */
	template <class Pixel>
	static void createScalers(Blender<Pixel> blender,
	                          vector<Scaler*>& scalers);

	/** Disposes of the scalers created by the createScalers method.
	  */
	static void disposeScalers(vector<Scaler*>& scalers);

	/** Scales the given line.
	  * Pixels at even X coordinates are read and written in a 2x2 square
	  * with the original pixel at the top-left corner.
	  * The scaling algorithm should preserve the colour of the original pixel.
	  * @param surface Image to scale.
	  *   This image is both the source and the destination for the scale
	  *   operation.
	  * @param y Y-coordinate of the line to scale.
	  */
	virtual void scaleLine256(SDL_Surface* surface, int y) = 0;

	/** Scales the given line.
	  * Pixels on even lines are read and the odd lines are written.
	  * The scaling algorithm should preserve the content of the even lines.
	  * @param surface Image to scale.
	  *   This image is both the source and the destination for the scale
	  *   operation.
	  * @param y Y-coordinate of the line to scale.
	  */
	virtual void scaleLine512(SDL_Surface* surface, int y) = 0;

protected:
	Scaler();
};

/** Scaler which assigns the colour of the original pixel to all pixels in
  * the 2x2 square.
  */
class SimpleScaler: public Scaler
{
public:
	SimpleScaler();
	void scaleLine256(SDL_Surface* surface, int y);
	void scaleLine512(SDL_Surface* surface, int y);
private:
	/** Copies the line; implements both scaleLine256 and scaleLine512.
	  */
	inline void copyLine(SDL_Surface* surface, int y);
};

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template <class Pixel>
class SaI2xScaler: public Scaler
{
public:
	SaI2xScaler(Blender<Pixel> blender);
	void scaleLine256(SDL_Surface* surface, int y);
	void scaleLine512(SDL_Surface* surface, int y);
//private:
protected:
	Blender<Pixel> blender;
};


template <class Pixel>
void Scaler::createScalers(Blender<Pixel> blender, vector<Scaler*>& scalers)
{
	scalers.push_back(new SimpleScaler());
	scalers.push_back(new SaI2xScaler<Pixel>(blender));
}

} // namespace openmsx

#endif // __SCALERS_HH__
