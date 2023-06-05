/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2016 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef USBTHREAD_HH
#define USBTHREAD_HH

#include <BasicJuceHeader.h>
#include <atomic>

#include "../oni-api/oni.h"

class AbstractRHXController;
class OniDevice;

#define ONI_BUFFER_SAMPLES 256

namespace RhythmNode
{
	/** 
	
		Reads data from a USB device in a separate thread
	
	*/
	class USBThread : Thread
	{
	public:

		/** Constructor */
		USBThread(AbstractRHXController*);

		/** Destructor */
		~USBThread() { }

		/** Calls readDataBlocksRaw() in a loop */
		void run() override;

		/** Called at start of acquisition */
		void startAcquisition(int nBytes);

		/** Called when acquisition ends */
		void stopAcquisition();

		/** Reads data into an unsigned char buffer */
		long usbRead(uint8_t* & buffer);

		/** Sets a pointer to ONI frames*/
		long getOniFrames(oni_frame_t** frames);
	
	private:
		
		AbstractRHXController* const m_board;
		OniDevice* m_oni_device;
		
		HeapBlock<unsigned char> m_buffers[20000];
		oni_frame_t* oni_buffers[ONI_BUFFER_SAMPLES * 2];
		
		long m_lastRead[2];
		
		unsigned short m_currentBuffer{ 0 };
		unsigned short m_readBuffer{ 0 };
		unsigned short m_currentFrame{ 0 };
		bool m_canRead{ false };
		
		CriticalSection m_lock;

		bool useOniFrame = false;
	};
}
#endif
