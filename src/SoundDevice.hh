
#ifndef __SOUNDDEVICE_HH__
#define __SOUNDDEVICE_HH__

#include "openmsx.hh"

class SoundDevice
{
	public:
		/**
		 *
		 */
		void init();

		/**
		 *
		 */
		void reset();

		/**
		 * Set the relative volume for this sound device, this
		 * can be used to make a MSX-MUSIC sound louder than a
		 * MSX-AUDIO
		 * 
		 *    0 <= newVolume <= 65535
		 */
		void setVolume (int newVolume);

		/**
		 *
		 */
		void setSampleRate (int newSampleRate);

		/**
		 * TODO update sound buffers
		 */
		void updateBuffer (word *buffer, int length);
};
#endif
