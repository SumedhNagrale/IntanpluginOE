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


#include "../Threads/USBThread.h"
#include "../rhx-api/Abstract/abstractrhxcontroller.h"
#include "../oni-api/OniDevice.h"

using namespace RhythmNode;

USBThread::USBThread(AbstractRHXController* b)
	: m_board(b), Thread("USBThread")
{
	if (b->getType() == ControllerOEECP5)
	{
		m_oni_device = (OniDevice*) m_board;
		useOniFrame = true;
	}
	else {
		useOniFrame = false;
	}
}

void USBThread::startAcquisition(int nBytes)
{
    const ScopedLock lock(m_lock);

	for (int i = 0; i < 2; i++)
	{
		m_lastRead[i] = 0;
		m_buffers[i].malloc(nBytes);
	}
	
	m_currentBuffer = 0;
	m_readBuffer = 0;
	m_currentFrame = 0;
	m_canRead = true;

	startThread();
}

void USBThread::stopAcquisition()
{
	std::cout << "Stopping usb thread" << std::endl;
	
	if (isThreadRunning())
	{
		if (!stopThread(1000))
		{
			std::cerr << "RhythmNode USB Thread could not stop cleanly. Forcing quit." << std::endl;
		}
	}
}


long USBThread::usbRead(uint8_t*& buffer)
{
	const ScopedLock lock(m_lock);

	if (m_readBuffer == m_currentBuffer)
		return 0;

	buffer = m_buffers[m_readBuffer].getData();

	long read = m_lastRead[m_readBuffer];
	m_readBuffer = ++m_readBuffer % 2;
	m_canRead = true;

	notify();

	return read;
}

long USBThread::getOniFrames(oni_frame_t** frames)
{
	const ScopedLock lock(m_lock);

	if (m_readBuffer == m_currentBuffer)
		return 0;

	frames = &oni_buffers[m_readBuffer * ONI_BUFFER_SAMPLES];

	m_currentFrame -= ONI_BUFFER_SAMPLES;

	for (int i = 0; i < ONI_BUFFER_SAMPLES; i++)
		oni_destroy_frame(oni_buffers[m_currentFrame++]);

	m_currentFrame %= (ONI_BUFFER_SAMPLES * 2);

	m_readBuffer = ++m_readBuffer % 2;
	m_canRead = true;

}

void USBThread::run()
{
	while (!threadShouldExit())
	{
		m_lock.enter();
		
		if (m_canRead)
		{
			m_lock.exit();
			
			long read;
			
			do
			{

				if (threadShouldExit())
					break;

				if (!useOniFrame)
					read = m_board->readDataBlocksRaw(8, m_buffers[m_currentBuffer].getData());
				else
				{
					for (int i = 0; i < ONI_BUFFER_SAMPLES; i++)
						read = m_oni_device->readFrame(&oni_buffers[m_currentFrame++]);
				}
					

			} while (read <= 0);
			{
				const ScopedLock lock(m_lock);
				m_lastRead[m_currentBuffer] = read;
				m_currentBuffer = ++m_currentBuffer % 2;
				m_canRead = false;
			}
		}
		else
			m_lock.exit();

		if (!threadShouldExit())
			wait(100);
	}
}
