/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2022 Open Ephys

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

#include "RhythmDevice.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cmath>


RhythmDevice::RhythmDevice(ControllerType type_, AmplifierSampleRate sampleRate_) :
    AbstractRHXController(type_, sampleRate_),
    chipRegisters(30000.0f)
{

    INIT_STEP = type_ == ControllerOEOpalKellyUSB3 ? 256 : 60;

    std::cout << "RhythmDevice constructor" << std::endl;
    std::cout << "Max num streams: " << maxNumDataStreams() << std::endl;
}

RhythmDevice::~RhythmDevice()
{

}

int RhythmDevice::open(const std::string& boardSerialNumber, const char* libraryFilePath)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard = std::make_unique<Rhd2000EvalBoard>();

    int return_code = evalBoard->open(libraryFilePath);

    return return_code;
}


bool RhythmDevice::uploadFPGABitfile(const std::string& filename)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    return evalBoard->uploadFpgaBitfile(filename);
  
}
 
void RhythmDevice::resetBoard()
{
    std::lock_guard<std::mutex> lockOk(okMutex);


    evalBoard->resetBoard();
}


void RhythmDevice::run()
{

    evalBoard->run();
}

void RhythmDevice::updateRegisters()
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    // Set up an RHD2000 register object using this sample rate to
    // optimize MUX-related register settings.
    chipRegisters.defineSampleRate(SampleRate30000Hz);

    int commandSequenceLength;
    std::vector<unsigned int> commandList;

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig(commandList, true);
    // Upload version with ADC calibration to AuxCmd3 RAM Bank 0.
    evalBoard->uploadCommandList(commandList, Rhd2000EvalBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandLength(Rhd2000EvalBoard::AuxCmd3, 0,
        commandSequenceLength - 1);

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig(commandList, false);
    // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
    evalBoard->uploadCommandList(commandList, Rhd2000EvalBoard::AuxCmd3, 1);
    evalBoard->selectAuxCommandLength(Rhd2000EvalBoard::AuxCmd3, 0,
        commandSequenceLength - 1);
}

bool RhythmDevice::isRunning()
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    return evalBoard->isRunning();
}

void RhythmDevice::flush()
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->flush();
}


void RhythmDevice::resetFpga()
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->resetFpga();
}


bool RhythmDevice::readDataBlock(RHXDataBlock* dataBlock)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->readDataBlock(dataBlock);

    return true;
}


bool RhythmDevice::readDataBlocks(int numBlocks, std::deque<RHXDataBlock*>& dataQueue)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    return evalBoard->readDataBlocks(numBlocks, dataQueue);
}


long RhythmDevice::readDataBlocksRaw(int numBlocks, uint8_t* buffer)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    return evalBoard->readDataBlocksRaw(numBlocks, buffer);
}

bool RhythmDevice::readRawDataBlock(unsigned char** bufferPtr, int nSamples)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    return evalBoard->readRawDataBlock(bufferPtr, nSamples);
}

void RhythmDevice::setContinuousRunMode(bool continuousMode)
{
    std::lock_guard<std::mutex> lockOk(okMutex);


    evalBoard->setContinuousRunMode(continuousMode);
}

void RhythmDevice::setMaxTimeStep(unsigned int maxTimeStep)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setMaxTimeStep(maxTimeStep);
}

void RhythmDevice::setCableDelay(BoardPort port, int delay)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    int bitShift = 0;

    if ((delay < 0) || (delay > 15)) {
        std::cerr << "Warning in RhythmDevice::setCableDelay: delay out of range: " << delay << '\n';
        if (delay < 0) delay = 0;
        else if (delay > 15) delay = 15;
    }

    evalBoard->setCableDelay(Rhd2000EvalBoard::BoardPort(port), delay);

}


void RhythmDevice::setDspSettle(bool enabled)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setDspSettle(enabled);
   
}


void RhythmDevice::setTtlOut(const int* ttlOutArray)
{

    std::lock_guard<std::mutex> lockOk(okMutex);
 
    evalBoard->setTtlOut(ttlOutArray);
    
}


void RhythmDevice::setDacManual(int value)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setDacManual(value);
}


void RhythmDevice::enableLeds(bool ledsOn)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->enableBoardLeds(ledsOn);
}

// Set output BNC clock divide factor (Open Ephys boards only)
void RhythmDevice::setClockDivider(int divide_factor)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setClockDivider(divide_factor);

}


void RhythmDevice::setDacGain(int gain)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setDacGain(gain);
}

// 
void RhythmDevice::setAudioNoiseSuppress(int noiseSuppress)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setAudioNoiseSuppress(noiseSuppress);

}


void RhythmDevice::setExternalFastSettleChannel(int channel)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setExternalFastSettleChannel(channel);
}


void RhythmDevice::setExternalDigOutChannel(BoardPort port, int channel)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setExternalDigOutChannel(Rhd2000EvalBoard::BoardPort(port), channel);
}


void RhythmDevice::setDacHighpassFilter(double cutoff)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setDacHighpassFilter(cutoff);
}


void RhythmDevice::setDacThreshold(int dacChannel, int threshold, bool trigPolarity)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setDacThreshold(dacChannel, threshold, trigPolarity);

}


void RhythmDevice::setTtlMode(int mode)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setTtlMode(mode);

}
 
void RhythmDevice::setDacRerefSource(int stream, int channel)
{

    std::cerr << "RhythmDevice::setDacRerefSource: not implemented." << std::endl;

}

bool RhythmDevice::setSampleRate(AmplifierSampleRate newSampleRate)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setSampleRate(Rhd2000EvalBoard::SampleRate30000Hz);

    return true;
}


void RhythmDevice::enableDataStream(int stream, bool enabled)
{
    std::lock_guard<std::mutex> lockOk(okMutex);


    if (stream < 0 || stream > (maxNumDataStreams() - 1)) {
        std::cerr << "Error in RhythmDevice::enableDataStream: stream out of range.\n";
        return;
    }

    if (enabled) {
        if (dataStreamEnabled[stream] == 0) {
            evalBoard->enableDataStream(stream, enabled);
            dataStreamEnabled[stream] = 1;
            numDataStreams++;
        }
        else {
            std::cout << "Data stream " << stream << " already enabled. " << std::endl;
        }
    }
    else {
        if (dataStreamEnabled[stream] == 1) {
            evalBoard->enableDataStream(stream, enabled);
            dataStreamEnabled[stream] = 0;
            numDataStreams--;
        }
        else {
            std::cout << "Data stream " << stream << " already disabled. " << std::endl;
        }
    }

    //for (int i = 0; i < dataStreamEnabled.size(); i++)
    //    std::cout << dataStreamEnabled[i] << " ";

    //std::cout << std::endl;
   
}

void RhythmDevice::setDataSource(int stream, BoardDataSource source)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->setDataSource(stream, Rhd2000EvalBoard::BoardDataSource(source));
}


void RhythmDevice::enableDac(int dacChannel, bool enabled)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->enableDac(dacChannel, enabled);

}


void RhythmDevice::enableExternalFastSettle(bool enable)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->enableExternalFastSettle(enable);
}


void RhythmDevice::enableExternalDigOut(BoardPort port, bool enable)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->enableExternalDigOut(Rhd2000EvalBoard::BoardPort(port), enable);
   
}


void RhythmDevice::enableDacHighpassFilter(bool enable)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->enableDacHighpassFilter(enable);

}

void RhythmDevice::enableDacReref(bool enabled)
{

    std::cerr << "RhythmDevice::enableDacReref: not implemented." << std::endl;
}


void RhythmDevice::selectDacDataStream(int dacChannel, int stream)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->selectDacDataStream(dacChannel, stream);

}


void RhythmDevice::selectDacDataChannel(int dacChannel, int dataChannel)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->selectDacDataChannel(dacChannel, dataChannel);

}


void RhythmDevice::selectAuxCommandLength(AuxCmdSlot auxCommandSlot, int loopIndex, int endIndex)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->selectAuxCommandLength(Rhd2000EvalBoard::AuxCmdSlot(auxCommandSlot), loopIndex, endIndex);


}


void RhythmDevice::selectAuxCommandBank(BoardPort port, AuxCmdSlot auxCommandSlot, int bank)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->selectAuxCommandBank(Rhd2000EvalBoard::BoardPort(port), Rhd2000EvalBoard::AuxCmdSlot(auxCommandSlot), bank);

}

int RhythmDevice::getBoardMode()
{
    // not used?
    return 0; 
}


int RhythmDevice::getNumSPIPorts(bool& expanderBoardDetected)
{
    return 4;
}

 
void RhythmDevice::clearTtlOut()
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->clearTtlOut();

}


void RhythmDevice::uploadCommandList(const std::vector<unsigned int>& commandList, AuxCmdSlot auxCommandSlot, int bank)
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->uploadCommandList(commandList, Rhd2000EvalBoard::AuxCmdSlot(auxCommandSlot), bank);
  
}

unsigned int RhythmDevice::numWordsInFifo()
{

    std::lock_guard<std::mutex> lockOk(okMutex);

  
    return evalBoard->numWordsInFifo();
}


bool RhythmDevice::isDcmProgDone() const
{

    return evalBoard->isDcmProgDone();
}


bool RhythmDevice::isDataClockLocked() const
{

    return evalBoard->isDataClockLocked();
}


void RhythmDevice::forceAllDataStreamsOff()
{

    std::lock_guard<std::mutex> lockOk(okMutex);

    evalBoard->forceAllDataStreamsOff();
}

