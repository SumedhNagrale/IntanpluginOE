//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.6
//
//  Copyright (c) 2020-2022 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include <iostream>
#include "syntheticrhxcontroller.h"

SyntheticRHXController::SyntheticRHXController(ControllerType type_, AmplifierSampleRate sampleRate_) :
    AbstractRHXController(type_, sampleRate_)
{
    dataGenerator = new SynthDataBlockGenerator(type, getSampleRate(sampleRate));
}

SyntheticRHXController::~SyntheticRHXController()
{
    delete dataGenerator;
}

// For a physical board, read data block from the USB interface. Fill given dataBlock from USB buffer.
bool SyntheticRHXController::readDataBlock(RHXDataBlock *dataBlock)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    unsigned int numBytesToRead = BytesPerWord * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

    if (numBytesToRead > usbBufferSize) {
        std::cerr << "Error in SyntheticRHXController::readDataBlock: USB buffer size exceeded.  " <<
                "Increase value of MAX_NUM_BLOCKS.\n";
        return false;
    }

    dataBlock->fillFromUsbBuffer(usbBuffer, 0);

    return true;
}

// For a physical board, read a certain number of USB data blocks, and append them to queue.
// Return true if data blocks were available.
bool SyntheticRHXController::readDataBlocks(int numBlocks, std::deque<RHXDataBlock*> &dataQueue)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    unsigned int numWordsToRead = numBlocks * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

    if (numWordsInFifo() < numWordsToRead)
        return false;

    unsigned int numBytesToRead = BytesPerWord * numWordsToRead;

    if (numBytesToRead > usbBufferSize) {
        std::cerr << "Error in SyntheticRHXController::readDataBlocks: USB buffer size exceeded.  " <<
                "Increase value of MAX_NUM_BLOCKS.\n";
        return false;
    }

    for (int i = 0; i < numBlocks; ++i) {
        RHXDataBlock* dataBlock = new RHXDataBlock(type, numDataStreams);
        dataBlock->fillFromUsbBuffer(usbBuffer, i);
        dataQueue.push_back(dataBlock);
    }

    return true;
}

// For a physical board, read a certain number of USB data blocks, and write the raw bytes to a buffer.
// Return total number of bytes read.
long SyntheticRHXController::readDataBlocksRaw(int numBlocks, uint8_t *buffer)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    return dataGenerator->readSynthDataBlocksRaw(numBlocks, buffer, numDataStreams);
}

// Set the delay for sampling the MISO line on a particular SPI port (PortA - PortH), in integer clock steps, where each
// clock step is 1/2800 of a per-channel sampling period.  Note: Cable delay must be updated after sampleRate is changed,
// since cable delay calculations are based on the clock frequency!
void SyntheticRHXController::setCableDelay(BoardPort port, int delay)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((delay < 0) || (delay > 15)) {
        std::cerr << "Warning in SyntheticRHXController::setCableDelay: delay out of range: " << delay << '\n';
        if (delay < 0) delay = 0;
        else if (delay > 15) delay = 15;
    }

    switch (port) {
    case PortA:
        cableDelay[0] = delay;
        break;
    case PortB:
        cableDelay[1] = delay;
        break;
    case PortC:
        cableDelay[2] = delay;
        break;
    case PortD:
        cableDelay[3] = delay;
        break;
    case PortE:
        cableDelay[4] = delay;
        break;
    case PortF:
        cableDelay[5] = delay;
        break;
    case PortG:
        cableDelay[6] = delay;
        break;
    case PortH:
        cableDelay[7] = delay;
        break;
    default:
        std::cerr << "Error in SyntheticRHXController::setCableDelay: unknown port.\n";
    }
}

// Assign a particular data source (e.g., PortA1, PortA2, PortB1,...) to one of the eight available USB data streams (0-7).
// Used only with ControllerRecordUSB2.
void SyntheticRHXController::setDataSource(int stream, BoardDataSource dataSource)
{
    if (type != ControllerRecordUSB2) return;

    if ((stream < 0) || (stream > 7)) {
        std::cerr << "Error in SyntheticRHXController::setDataSource: stream out of range.\n";
        return;
    }
    boardDataSources[stream] = dataSource;
}

// Set the per-channel sampling rate of the RHD/RHS chips connected to the FPGA.
bool SyntheticRHXController::setSampleRate(AmplifierSampleRate newSampleRate)
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    sampleRate = newSampleRate;
    return true;
}

// Enable or disable one of the 32 available USB data streams (0-31).
void SyntheticRHXController::enableDataStream(int stream, bool enabled)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (stream < 0 || stream > (maxNumDataStreams() - 1)) {
        std::cerr << "Error in SyntheticRHXController::enableDataStream: stream out of range.\n";
        return;
    }

    if (enabled) {
        if (dataStreamEnabled[stream] == 0) {
            dataStreamEnabled[stream] = 1;
            numDataStreams++;
        }
    } else {
        if (dataStreamEnabled[stream] == 1) {
            dataStreamEnabled[stream] = 0;
            numDataStreams--;
        }
    }
}

// Return 4-bit "board mode" input.
int SyntheticRHXController::getBoardMode()
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    return boardMode(type);
}

// Return number of SPI ports and if I/O expander board is present.
int SyntheticRHXController::getNumSPIPorts(bool &expanderBoardDetected)
{
    expanderBoardDetected = true;
    return (type == ControllerRecordUSB3 ? 8 : 4);
}

// Return the number of 16-bit words in the USB FIFO.  The user should never attempt to read more data than the
// FIFO currently contains, as it is not protected against underflow.
unsigned int SyntheticRHXController::numWordsInFifo()
{
    numWordsHasBeenUpdated = true;
    return lastNumWordsInFifo;
}
