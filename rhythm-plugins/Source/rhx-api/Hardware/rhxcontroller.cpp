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
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cmath>

#include "rhxcontroller.h"
#include "rhxregisters.h"

//#define SetWireInValue(a, b) SetWireInValue(a, b); std::cout << "SetWireInValue address: " << (a) << " value: " << (b) << std::endl;
//#define ActivateTriggerIn(a, b) ActivateTriggerIn(a, b); std::cout << "ActivateTriggerIn address: " << (a) << " value: " << (b) << std::endl;
//#define GetWireOutValue(a) GetWireOutValue(a); std::cout << "GetWireOutValue address: " << (a) << std::endl;


// This class provides access to and control of one of the following:
//   (1) an Opal Kelly XEM6010 USB2/FPGA interface board running the Intan Rhythm interface Verilog code
//       (e.g., a 256-channel Intan RHD2000 USB Interface Board with 256-channel capacity)
//   (2) an Opal Kelly XEM6310 USB3/FPGA interface board running the Intan Rhythm USB3 interface Verilog code
//       (e.g., an Intan Recording Controller with 512-channel or 1024-channel capacity)
//   (3) an Opal Kelly XEM6010 USB2/FPGA interface board running the Intan RhythmStim interface Verilog code
//       (e.g., an Intan Stim/Recording Controller with 128-channel capacity)

RHXController::RHXController(ControllerType type_, AmplifierSampleRate sampleRate_) :
    AbstractRHXController(type_, sampleRate_),
    dev(nullptr)
{

    std::cout << "RHXController::RHXController" << std::endl;
}

RHXController::~RHXController()
{
    if (dev) delete dev;
}

// Find an Opal Kelly board attached to a USB port with the given serial number and open it returns 1 if successful,
// -1 if FrontPanel cannot be loaded, and -2 if board can't be found.
int RHXController::open(const std::string& boardSerialNumber, const char* libraryFilePath)
{
    dev = new okCFrontPanel;
    std::cout << "Attempting to connect to device '" << boardSerialNumber.c_str() << "'\n";

    okCFrontPanel::ErrorCode result = dev->OpenBySerial(boardSerialNumber);
    // Attempt to open device.
    if (result != okCFrontPanel::NoError) {
        delete dev;
         std::cerr << "Device could not be opened.  Is one connected?\n";
         std::cerr << "Error = " << result << "\n";
        return -2;
    }

    // Configure the on-board PLL appropriately.
    dev->LoadDefaultPLLConfiguration();
    std::cout << "okCFrontPanel connected the device which you were trying to connect see details below" << std::endl; 
    // Get some general information about the XEM.
    std::cout << "FPGA system clock: " << getSystemClockFreq() << " MHz" << std::endl; // Should indicate 100 MHz
    std::cout << "Opal Kelly device firmware version: " << dev->GetDeviceMajorVersion() << "." <<
            dev->GetDeviceMinorVersion() << '\n';
    std::cout << "Opal Kelly device serial number: " << dev->GetSerialNumber() << '\n';
    std::cout << "Opal Kelly device ID string: " << dev->GetDeviceID() << "\n";

    return 1;
}

// Upload the configuration file (bitfile) to the FPGA.  Return true if successful.
bool RHXController::uploadFPGABitfile(const std::string& filename)
{
    okCFrontPanel::ErrorCode errorCode = dev->ConfigureFPGA(filename);

    switch (errorCode) {
    case okCFrontPanel::NoError:
        std::cerr << "FPGA configuration successful: Device now open.\n";
        break;
    case okCFrontPanel::DeviceNotOpen:
        std::cerr << "FPGA configuration failed: Device not open.\n";
        return false;
    case okCFrontPanel::FileError:
        std::cerr << "FPGA configuration failed: Cannot find configuration file.\n";
        return false;
    case okCFrontPanel::InvalidBitstream:
        std::cerr << "FPGA configuration failed: Bitstream is not properly formatted.\n";
        return false;
    case okCFrontPanel::DoneNotHigh:
        std::cerr << "FPGA configuration failed: FPGA DONE signal did not assert after configuration.\n";
        std::cerr << "Note: Switch may be in PROM position instead of USB position.\n";
        return false;
    case okCFrontPanel::TransferError:
        std::cerr << "FPGA configuration failed: USB error occurred during download.\n";
        return false;
    case okCFrontPanel::CommunicationError:
        std::cerr << "FPGA configuration failed: Communication error with firmware.\n";
        return false;
    case okCFrontPanel::UnsupportedFeature:
        std::cerr << "FPGA configuration failed: Unsupported feature.\n";
        return false;
    default:
        std::cerr << "FPGA configuration failed: Unknown error.\n";
        return false;
    }

    // Check for Opal Kelly FrontPanel support in the FPGA configuration.
    if (dev->IsFrontPanelEnabled() == false) {
         std::cerr << "Opal Kelly FrontPanel support is not enabled in this FPGA configuration.\n";
        delete dev;
        return false;
    }

    int boardId, boardVersion;
    dev->UpdateWireOuts();
    boardId = dev->GetWireOutValue(WireOutBoardId);
    boardVersion = dev->GetWireOutValue(WireOutBoardVersion);

    std::cout << "Rhythm configuration file successfully loaded. Rhythm version number: " <<
            boardVersion << "\n\n";

    return true;
}

// Reads system clock frequency from Opal Kelly board (in MHz).  Should be 100 MHz for normal
// Rhythm operation.
double RHXController::getSystemClockFreq() const
{
    // Read back the CY22393 PLL configuation
    okCPLL22393 pll;
    dev->GetEepromPLL22393Configuration(pll);

    return pll.GetOutputFrequency(0);
}

// Reset FPGA.  This clears all auxiliary command RAM banks, clears the USB FIFO, and resets the per-channel sampling
// rate to 30.0 kS/s/ch.
void RHXController::resetBoard()
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    std::cout << " RHXController::resetBoard()" << std::endl;

    resetBoard(dev);

    if (type == ControllerRecordUSB3) {
        // Set up USB3 block transfer parameters.
        dev->SetWireInValue(WireInMultiUse, USB3BlockSize / 4);  // Divide by 4 to convert from bytes to 32-bit words (used in FPGA FIFO)
        dev->UpdateWireIns();
        dev->ActivateTriggerIn(TrigInConfig_USB3, 9);
        dev->SetWireInValue(WireInMultiUse, RAMBurstSize);
        dev->UpdateWireIns();
        dev->ActivateTriggerIn(TrigInConfig_USB3, 10);
    }
    else if (type == ControllerOEOpalKellyUSB3 || type == ControllerOEECP5)
    {
        dev->SetWireInValue(WireInMultiUse, 1024 / 4);
        dev->UpdateWireIns();
        dev->ActivateTriggerIn(TrigInOpenEphys, 16);
        std::cout << "USB block size set to " << 1024 << std::endl;
        dev->SetWireInValue(WireInMultiUse, 32);
        dev->UpdateWireIns();
        dev->ActivateTriggerIn(TrigInOpenEphys, 17);
        std::cout << "DDR burst set to " << 32 << std::endl;
    }
  
}

// Initiate SPI data acquisition.
void RHXController::run()
{

    //std::cout << " RHXController::run()" << std::endl;

    std::lock_guard<std::mutex> lockOk(okMutex);

    //dev->UpdateWireOuts(); //sumedh
    dev->ActivateTriggerIn(TrigInSpiStart, 0);
}

// Is the FPGA currently running?
bool RHXController::isRunning()
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->UpdateWireOuts();
    int value = dev->GetWireOutValue(WireOutSpiRunning);

    // update number of words in FIFO while we're at it
    if (type == ControllerRecordUSB3) {
        lastNumWordsInFifo = dev->GetWireOutValue(WireOutNumWords_USB3);
    } else {
        lastNumWordsInFifo = (dev->GetWireOutValue(WireOutNumWordsMsb_USB2) << 16) +
                dev->GetWireOutValue(WireOutNumWordsLsb_USB2);
    }
    numWordsHasBeenUpdated = true;

    return (value & 0x01) != 0;
}

// Flush all remaining data out of the FIFO.  (This function should only be called when SPI data acquisition has been stopped.)
void RHXController::flush()
{

    std::cout << " RHXController::flush()" << std::endl;

    std::lock_guard<std::mutex> lockOk(okMutex);

    if (type == ControllerRecordUSB3) {
        dev->SetWireInValue(WireInResetRun, 1 << 16, 1 << 16); // override pipeout block throttle
        dev->UpdateWireIns();

        while (numWordsInFifo() >= usbBufferSize / BytesPerWord) {
            dev->ReadFromBlockPipeOut(PipeOutData, USB3BlockSize, usbBufferSize, usbBuffer);
        }
        while (numWordsInFifo() > 0) {
            dev->ReadFromBlockPipeOut(PipeOutData, USB3BlockSize,
                                      USB3BlockSize * max(BytesPerWord * numWordsInFifo() / USB3BlockSize, (unsigned int)1),
                                      usbBuffer);
        }

        dev->SetWireInValue(WireInResetRun, 0 << 16, 1 << 16);
        dev->UpdateWireIns();
    } else {
        while (numWordsInFifo() >= usbBufferSize / BytesPerWord) {
            dev->ReadFromPipeOut(PipeOutData, usbBufferSize, usbBuffer);
        }
        while (numWordsInFifo() > 0) {
            dev->ReadFromPipeOut(PipeOutData, BytesPerWord * numWordsInFifo(), usbBuffer);
        }
    }
}

// Low-level FPGA reset.  Call when closing application to make sure everything has stopped.
void RHXController::resetFpga()
{

    std::cout << " RHXController::resetFpga()" << std::endl;

    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->ResetFPGA();
}

// Read data block from the USB interface, if one is available.  Return true if data block was available.
bool RHXController::readDataBlock(RHXDataBlock *dataBlock)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    //std::cout << " RHXController::readDataBlock()" << std::endl;

    unsigned int numBytesToRead = BytesPerWord * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

    //std::cout << "Reading " << numBytesToRead << " from pipeout " << PipeOutData << std::endl;
    //std::cout << "Num data streams: " << numDataStreams << std::endl;

    if (numBytesToRead > usbBufferSize) {
        std::cerr << "Error in RHXController::readDataBlock: USB buffer size exceeded.  " <<
                "Increase value of MAX_NUM_BLOCKS.\n";
        return false;
    }

    if (type == ControllerRecordUSB3 || type == ControllerOEOpalKellyUSB3 ) {
        long result = dev->ReadFromBlockPipeOut(PipeOutData, USB3BlockSize,
                                                USB3BlockSize * max(numBytesToRead / USB3BlockSize, (unsigned int)1),
                                                usbBuffer);
        if (result == ok_Failed) {
            std::cerr << "CRITICAL (readDataBlock): Failure on pipe read.  Check block and buffer sizes.\n";
        } else if (result == ok_Timeout) {
            std::cerr << "CRITICAL (readDataBlock): Timeout on pipe read.  Check block and buffer sizes.\n";
        }
    } else {
        dev->ReadFromPipeOut(PipeOutData, numBytesToRead, usbBuffer);
    }

    dataBlock->fillFromUsbBuffer(usbBuffer, 0);

    //dataBlock->print(0);

    return true;
}

// Read a certain number of USB data blocks, if the specified number is available, and append them to queue.
// Return true if data blocks were available.
bool RHXController::readDataBlocks(int numBlocks, std::deque<RHXDataBlock*> &dataQueue)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    std::cout << " RHXController::readDataBlocks()" << std::endl;

    unsigned int numWordsToRead = numBlocks * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

    if (numWordsInFifo() < numWordsToRead)
        return false;

    unsigned int numBytesToRead = BytesPerWord * numWordsToRead;

    if (numBytesToRead > usbBufferSize) {
        std::cerr << "Error in RHXController::readDataBlocks: USB buffer size exceeded.  " <<
                "Increase value of MaxNumBlocksToRead.\n";
        return false;
    }

    if (type == ControllerRecordUSB3 || type == ControllerOEOpalKellyUSB3) {
        long result = dev->ReadFromBlockPipeOut(PipeOutData, USB3BlockSize, numBytesToRead, usbBuffer);

        if (result == ok_Failed) {
            std::cerr << "CRITICAL (readDataBlocks): Failure on pipe read.  Check block and buffer sizes.\n";
        } else if (result == ok_Timeout) {
            std::cerr << "CRITICAL (readDataBlocks): Timeout on pipe read.  Check block and buffer sizes.\n";
        }
    } else {
        dev->ReadFromPipeOut(PipeOutData, numBytesToRead, usbBuffer);
    }

    for (int i = 0; i < numBlocks; ++i) {
        RHXDataBlock* dataBlock = new RHXDataBlock(type, numDataStreams);
        dataBlock->fillFromUsbBuffer(usbBuffer, i);
        dataQueue.push_back(dataBlock);
    }

    return true;
}

// Read a certain number of USB data blocks, if the specified number is available, and write the raw bytes to a buffer.
// Return total number of bytes read.
long RHXController::readDataBlocksRaw(int numBlocks, uint8_t* buffer)
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    //numBlocks = 8;
    unsigned int numWordsToRead = numBlocks * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);
    std::cout <<  std::endl;
    if (numWordsInFifo() < numWordsToRead) return 0;

    long result;
    if (type == ControllerRecordUSB3 || type == ControllerOEOpalKellyUSB3) {
        result = dev->ReadFromBlockPipeOut(PipeOutData, USB3BlockSize, BytesPerWord * numWordsToRead, buffer);
    } else {
        result = dev->ReadFromPipeOut(PipeOutData, BytesPerWord * numWordsToRead, buffer);
    }
    std::cout <<"\n" << "numWordsToRead :: " << numBlocks << " result :: " << result << " Pipeoutdata :: " << PipeOutData << " BytesPerWord :: " << BytesPerWord << " numWordsToRead ::" << numWordsToRead << std::endl;
    std::cout << std::endl;
    if (result == ok_Failed) {
        std::cerr << "RHXController::readDataBlocksRaw: Failure on BT pipe read.  Check block and buffer sizes.\n";
    } else if (result == ok_Timeout) {
        std::cerr << "RHXController::readDataBlocksRaw: Timeout on BT pipe read.  Check block and buffer sizes.\n";
    }

    return result;
}

// Set the FPGA to run continuously once started (if continuousMode == true) or to run until maxTimeStep is reached
// (if continuousMode == false).
void RHXController::setContinuousRunMode(bool continuousMode)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (continuousMode) {
        dev->SetWireInValue(WireInResetRun, 0x02, 0x02);
    } else {
        dev->SetWireInValue(WireInResetRun, 0x00, 0x02);
    }
    dev->UpdateWireIns();
}

// Set maxTimeStep for cases where continuousMode == false.
void RHXController::setMaxTimeStep(unsigned int maxTimeStep)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (type == ControllerRecordUSB3) {
        dev->SetWireInValue(WireInMaxTimeStep_USB3, maxTimeStep);
    } else {
        unsigned int maxTimeStepLsb = maxTimeStep & 0x0000ffff;
        unsigned int maxTimeStepMsb = maxTimeStep & 0xffff0000;
        dev->SetWireInValue(WireInMaxTimeStepLsb_USB2, maxTimeStepLsb);
        dev->SetWireInValue(WireInMaxTimeStepMsb_USB2, maxTimeStepMsb >> 16);
    }
    dev->UpdateWireIns();
}

// Set the delay for sampling the MISO line on a particular SPI port (PortA - PortH), in integer clock steps, where
// each clock step is 1/2800 of a per-channel sampling period.  Note: Cable delay must be updated after sampleRate is
// changed, since cable delay calculations are based on the clock frequency!
void RHXController::setCableDelay(BoardPort port, int delay)
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    int bitShift = 0;

    if ((delay < 0) || (delay > 15)) {
        std::cerr << "Warning in RHXController::setCableDelay: delay out of range: " << delay << '\n';
        if (delay < 0) delay = 0;
        else if (delay > 15) delay = 15;
    }

    switch (port) {
    case PortA:
        bitShift = 0;
        cableDelay[0] = delay;
        break;
    case PortB:
        bitShift = 4;
        cableDelay[1] = delay;
        break;
    case PortC:
        bitShift = 8;
        cableDelay[2] = delay;
        break;
    case PortD:
        bitShift = 12;
        cableDelay[3] = delay;
        break;
    case PortE:
        bitShift = 16;
        cableDelay[4] = delay;
        break;
    case PortF:
        bitShift = 20;
        cableDelay[5] = delay;
        break;
    case PortG:
        bitShift = 24;
        cableDelay[6] = delay;
        break;
    case PortH:
        bitShift = 28;
        cableDelay[7] = delay;
        break;
    default:
        std::cerr << "Error in RHXController::setCableDelay: unknown port.\n";
    }

    dev->SetWireInValue(WireInMisoDelay, delay << bitShift, 0x0000000f << bitShift);
    dev->UpdateWireIns();
}

// Turn on or off DSP settle function in the FPGA.  (Only executes when CONVERT commands are sent.)
void RHXController::setDspSettle(bool enabled)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInResetRun, (enabled ? 0x04 : 0x00), 0x04);
    dev->UpdateWireIns();
}

// Assign a particular data source (e.g., PortA1, PortA2, PortB1,...) to one of the eight available USB data streams (0-7).
// Used only with ControllerRecordUSB2.
void RHXController::setDataSource(int stream, BoardDataSource dataSource)
{
    if (type != ControllerRecordUSB2) return;//sumedh

    int bitShift = 0;
    EndPointRecordUSB2 endPoint;

    std::cout << "RHXController::setDataSource(" << stream << ", " << dataSource << ")" << std::endl;


    if ((stream < 0) || (stream > 7)) {
        std::cerr << "Error in RHXController::setDataSource: stream out of range.\n";
        return;
    }
    boardDataSources[stream] = dataSource;

    switch (stream) {
    case 0:
        endPoint = WireInDataStreamSel1234_R_USB2;
        bitShift = 0;
        break;
    case 1:
        endPoint = WireInDataStreamSel1234_R_USB2;
        bitShift = 4;
        break;
    case 2:
        endPoint = WireInDataStreamSel1234_R_USB2;
        bitShift = 8;
        break;
    case 3:
        endPoint = WireInDataStreamSel1234_R_USB2;
        bitShift = 12;
        break;
    case 4:
        endPoint = WireInDataStreamSel5678_R_USB2;
        bitShift = 0;
        break;
    case 5:
        endPoint = WireInDataStreamSel5678_R_USB2;
        bitShift = 4;
        break;
    case 6:
        endPoint = WireInDataStreamSel5678_R_USB2;
        bitShift = 8;
        break;
    case 7:
        endPoint = WireInDataStreamSel5678_R_USB2;
        bitShift = 12;
        break;
    }

    dev->SetWireInValue(endPoint, dataSource << bitShift, 0x000f << bitShift);
    dev->UpdateWireIns();
}

// Set the 16 bits of the digital TTL output lines on the FPGA high or low according to integer array. Not used with
// ControllerStimRecordUSB2.
void RHXController::setTtlOut(const int* ttlOutArray)
{
    if (type == ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    int ttlOut = 0;
    for (int i = 0; i < 16; ++i) {
        if (ttlOutArray[i] > 0)
            ttlOut += 1 << i;
    }
    dev->SetWireInValue(WireInTtlOut_R, ttlOut);
    dev->UpdateWireIns();
}

// Set manual value for DACs.
void RHXController::setDacManual(int value)
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    if ((value < 0) || (value > 65535)) {
        std::cerr << "Error in RHXController::setDacManual: value out of range.\n";
        return;
    }

    dev->SetWireInValue(WireInDacManual, value);
    dev->UpdateWireIns();
}

// Turn LEDs on or off (Open Ephys boards only)
void RHXController::enableLeds(bool ledsOn)
{

    std::cout << " RHXController::enableLeds()" << std::endl;

    if (type == ControllerOEECP5 || type == ControllerOEOpalKellyUSB2 || type == ControllerOEOpalKellyUSB3)
    {
        std::lock_guard<std::mutex> lockOk(okMutex);

        dev->SetWireInValue(WireInMultiUse, ledsOn ? 1 : 0);
        dev->UpdateWireIns();
        dev->ActivateTriggerIn(TrigInOpenEphys, 0);
    }
}

// Set output BNC clock divide factor (Open Ephys boards only)
void RHXController::setClockDivider(int divide_factor)
{

    std::cout << " RHXController::setClockDivider()" << std::endl;

    if (type == ControllerOEECP5 || type == ControllerOEOpalKellyUSB2 || type == ControllerOEOpalKellyUSB3)
    {
        std::lock_guard<std::mutex> lockOk(okMutex);
        dev->SetWireInValue(WireInMultiUse, divide_factor);
        dev->UpdateWireIns();
        dev->ActivateTriggerIn(TrigInOpenEphys, 1);
    }
}

// Set the eight red LEDs on the Opal Kelly XEM6x10 board according to integer array.
void RHXController::setLedDisplay(const int* ledArray)
{

    std::cout << " RHXController::setLedDisplay()" << std::endl;

    std::lock_guard<std::mutex> lockOk(okMutex);

    int ledOut = 0;
    for (int i = 0; i < 8; ++i) {
        if (ledArray[i] > 0)
            ledOut += 1 << i;
    }
    switch (type) {
    case (ControllerRecordUSB2):
        dev->SetWireInValue(WireInLedDisplay_R_USB2, ledOut);
        break;
    case (ControllerRecordUSB3):
        dev->SetWireInValue(WireInLedDisplay_R_USB3, ledOut);
        break;
    case (ControllerStimRecordUSB2):
        dev->SetWireInValue(WireInLedDisplay_S_USB2, ledOut);
        break;
    case (ControllerOEOpalKellyUSB2):
        dev->SetWireInValue(WireInLedDisplay, ledOut);
        break;
    case (ControllerOEOpalKellyUSB3):
        dev->SetWireInValue(WireInLedDisplay, ledOut);
        break;
    }
    dev->UpdateWireIns();
}

// Set the eight red LEDs on the front panel SPI ports according to integer array. Not used with ControllerRecordUSB2 or Open Ephys boards.
void RHXController::setSpiLedDisplay(const int* ledArray)
{
    if (type == ControllerRecordUSB2 ||
        type == ControllerOEOpalKellyUSB2 ||
        type == ControllerOEOpalKellyUSB3 ||
        type == ControllerOEECP5) return;

    std::cout << " RHXController::setSpiLedDisplay()" << std::endl;

    std::lock_guard<std::mutex> lockOk(okMutex);

    int ledOut = 0;
    for (int i = 0; i < 8; ++i) {
        if (ledArray[i] > 0)
            ledOut += 1 << i;
    }

    if (type == ControllerStimRecordUSB2) {
        dev->SetWireInValue(WireInLedDisplay_S_USB2, (ledOut << 8), 0xff00);
    } else if (type == ControllerRecordUSB3) {
        dev->SetWireInValue(WireInMultiUse, ledOut);
    }

    dev->UpdateWireIns();
    if (type == ControllerRecordUSB3) dev->ActivateTriggerIn(TrigInConfig_USB3, 8);
}

// Set the gain level of all eight DAC channels to 2^gain (gain = 0-7).
void RHXController::setDacGain(int gain)
{

    //std::cout << " RHXController::setDacGain()" << std::endl;

    std::lock_guard<std::mutex> lockOk(okMutex);
    if ((gain < 0) || (gain > 7)) {
        std::cerr << "Error in RHXController::setDacGain: gain setting out of range.\n";
        return;
    }

    dev->SetWireInValue(WireInResetRun, gain << 13, 0xe000);
    dev->UpdateWireIns();
}

// Suppress the noise on DAC channels 0 and 1 (the audio channels) between +16*noiseSuppress and -16*noiseSuppress LSBs.
// (noiseSuppress = 0-127).
void RHXController::setAudioNoiseSuppress(int noiseSuppress)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    std::cout << " RHXController::setAudioNoiseSuppress()" << std::endl;

    if ((noiseSuppress < 0) || (noiseSuppress > 127)) {
        std::cerr << "Error in RHXController::setAudioNoiseSuppress: noiseSuppress out of range.\n";
        return;
    }

    dev->SetWireInValue(WireInResetRun, noiseSuppress << 6, 0x1fc0);
    dev->UpdateWireIns();
}

// Select which of the TTL inputs 0-15 is used to perform a hardware 'fast settle' (blanking) of the amplifiers if external
// triggering of fast settling is enabled.
void RHXController::setExternalFastSettleChannel(int channel)
{

    std::cout << " RHXController::setExternalFastSettleChannel()" << std::endl;

    if (type == ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((channel < 0) || (channel > 15)) {
        std::cerr << "Error in RHXController::setExternalFastSettleChannel: channel out of range.\n";
        return;
    }

    dev->SetWireInValue(WireInMultiUse, channel);
    dev->UpdateWireIns();
    if (type == ControllerRecordUSB3) {
        dev->ActivateTriggerIn(TrigInConfig_USB3, 7);
    } else if (type == ControllerRecordUSB2) {
        dev->ActivateTriggerIn(TrigInExtFastSettle_R_USB2, 1);
    }
}

// Select which of the TTL inputs 0-15 is used to control the auxiliary digital output pin of the chips connected to
// a particular SPI port, if external control of auxout is enabled.
void RHXController::setExternalDigOutChannel(BoardPort port, int channel)
{

    std::cout << " RHXController::setExternalDigOutChannel()" << std::endl;

    if (type == ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((channel < 0) || (channel > 15)) {
        std::cerr << "Error in RHXController::setExternalDigOutChannel: channel out of range.\n";
        return;
    }

    dev->SetWireInValue(WireInMultiUse, channel);
    dev->UpdateWireIns();

    if (type == ControllerRecordUSB2) {
        switch (port) {
        case PortA:
            dev->ActivateTriggerIn(TrigInExtDigOut_R_USB2, 4);
            break;
        case PortB:
            dev->ActivateTriggerIn(TrigInExtDigOut_R_USB2, 5);
            break;
        case PortC:
            dev->ActivateTriggerIn(TrigInExtDigOut_R_USB2, 6);
            break;
        case PortD:
            dev->ActivateTriggerIn(TrigInExtDigOut_R_USB2, 7);
            break;
        default:
            std::cerr << "Error in RHXController::setExternalDigOutChannel: port out of range.\n";
        }
    } else if (type == ControllerRecordUSB3) {
        switch (port) {
        case PortA:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 24);
            break;
        case PortB:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 25);
            break;
        case PortC:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 26);
            break;
        case PortD:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 27);
            break;
        case PortE:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 28);
            break;
        case PortF:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 29);
            break;
        case PortG:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 30);
            break;
        case PortH:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 31);
            break;
        default:
            std::cerr << "Error in RHXController::setExternalDigOutChannel: port out of range.\n";
        }
    }
}

// Set cutoff frequency (in Hz) for optional FPGA-implemented digital high-pass filters associated with DAC outputs on
// USB interface board.  These one-pole filters can be used to record wideband neural data while viewing only spikes
// without LFPs on the DAC outputs, for example.  This is useful when using the low-latency FPGA thresholds to detect
// spikes and produce digital pulses on the TTL outputs, for example.
void RHXController::setDacHighpassFilter(double cutoff)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    std::cout << " RHXController::setDacHighpassFilter()" << std::endl;

    // Note that the filter coefficient is a function of the amplifier sample rate, so this
    // function should be called after the sample rate is changed.
    double b = 1.0 - exp(-1.0 * TwoPi * cutoff / getSampleRate());

    // In hardware, the filter coefficient is represented as a 16-bit number.
    int filterCoefficient = (int)floor(65536.0 * b + 0.5);

    if (filterCoefficient < 1) {
        filterCoefficient = 1;
    } else if (filterCoefficient > 65535) {
        filterCoefficient = 65535;
    }

    dev->SetWireInValue(WireInMultiUse, filterCoefficient);
    dev->UpdateWireIns();
    if (type == ControllerRecordUSB3) {
        dev->ActivateTriggerIn(TrigInConfig_USB3, 5);
    } else {
        dev->ActivateTriggerIn(TrigInDacHpf_USB2, 1);
    }
}

// Set thresholds for DAC channels; threshold output signals appear on TTL outputs 0-7.
// The parameter 'threshold' corresponds to the RHD/RHS chip ADC output value, and must fall in the range of 0 to 65535,
// where the 'zero' level is 32768.  If trigPolarity is true, voltages equaling or rising above the threshold produce a
// high TTL output. If trigPolarity is false, voltages equaling or falling below the threshold produce a high TTL output.
void RHXController::setDacThreshold(int dacChannel, int threshold, bool trigPolarity)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    //std::cout << " RHXController::setDacThreshold()" << std::endl;

    if ((dacChannel < 0) || (dacChannel > 7)) {
        std::cerr << "Error in RHXController::setDacThreshold: dacChannel out of range.\n";
        return;
    }

    if ((threshold < 0) || (threshold > 65535)) {
        std::cerr << "Error in RHXController::setDacThreshold: threshold out of range.\n";
        return;
    }

    // Set threshold level.
    dev->SetWireInValue(WireInMultiUse, threshold);
    dev->UpdateWireIns();
    if (type == ControllerRecordUSB3) {
        dev->ActivateTriggerIn(TrigInDacConfig_USB3, dacChannel);
    } else {
        dev->ActivateTriggerIn(TrigInDacThresh_USB2, dacChannel);
    }

    // Set threshold polarity.
    dev->SetWireInValue(WireInMultiUse, (trigPolarity ? 1 : 0));
    dev->UpdateWireIns();
    if (type == ControllerRecordUSB3) {
        dev->ActivateTriggerIn(TrigInDacConfig_USB3, dacChannel + 8);
    } else {
        dev->ActivateTriggerIn(TrigInDacThresh_USB2, dacChannel + 8);
    }
}

// Set the TTL output mode of the board. mode = 0: All 16 TTL outputs are under manual control mode = 1:
// Top 8 TTL outputs are under manual control; Bottom 8 TTL outputs are outputs of DAC comparators
void RHXController::setTtlMode(int mode)
{

    std::cout << " RHXController::setTtlMode()" << std::endl;

    if (type == ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((mode < 0) || (mode > 1)) {
        std::cerr << "Error in RHXController::setTtlMode: mode out of range.\n";
        return;
    }

    dev->SetWireInValue(WireInResetRun, mode << 3, 0x0008);
    dev->UpdateWireIns();
}

// Select an amplifier channel from a particular data stream to be subtracted from all DAC signals.
void RHXController::setDacRerefSource(int stream, int channel)
{

    std::cout << " RHXController::setDacRerefSource()" << std::endl;

    if (type == ControllerRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (stream < 0 || stream > (maxNumDataStreams() - 1)) {
        std::cerr << "Error in RHXController::setDacRerefSource: stream out of range.\n";
        return;
    }

    if (channel < 0 || channel > RHXDataBlock::channelsPerStream(type) - 1) {
        std::cerr << "Error in RHXController::setDacRerefSource: channel out of range.\n";
        return;
    }

    if (type == ControllerRecordUSB3) {
        dev->SetWireInValue(WireInDacReref_R_USB3, (stream << 5) + channel, 0x0000003ff);
    } else if (type == ControllerStimRecordUSB2) {
        dev->SetWireInValue(WireInDacReref_S_USB2, (stream << 5) + channel, 0x0000000ff);
    }
    dev->UpdateWireIns();
}

// Set the given extra states
void RHXController::setExtraStates(unsigned int extraStates)
{

    std::cout << " RHXController::setExtraStates()" << std::endl;

    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInExtraStates_S_USB2, extraStates);
    dev->UpdateWireIns();
}

// Turn on or off automatic stimulation command mode in the FPGA.
void RHXController::setStimCmdMode(bool enabled)
{

    std::cout << " RHXController::setStimCmdMode()" << std::endl;

    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInStimCmdMode_S_USB2, (enabled ? 0x01 : 0x00), 0x01);
    dev->UpdateWireIns();
}

// Set the voltage threshold to be used for digital triggers on Analog In ports.
void RHXController::setAnalogInTriggerThreshold(double voltageThreshold)
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    int value = (int) (32768 * (voltageThreshold / 10.24) + 32768);
    if (value < 0) {
        value = 0;
    } else if (value > 65535) {
        value = 65535;
    }

    dev->SetWireInValue(WireInAdcThreshold_S_USB2, value);
    dev->UpdateWireIns();
}

// Set state of manual stimulation trigger 0-7 (e.g., from keypresses).
void RHXController::setManualStimTrigger(int trigger, bool triggerOn)
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((trigger < 0) || (trigger > 7)) {
        std::cerr << "Error in RHXController::setManualStimTrigger: trigger out of range.\n";
        return;
    }

    dev->SetWireInValue(WireInManualTriggers_S_USB2, (triggerOn ? 1 : 0) << trigger, 1 << trigger);
    dev->UpdateWireIns();
}

// The first four boolean parameters determine if global settling should be applied to particular SPI ports A-D.
// If global settling is enabled, the amp settle function will be applied to ALL channels on a headstage when any one
// channel asserts amp settle. If the last boolean parameter is set true, global settling will be applied across all
// headstages: if any one channel asserts amp settle, then amp settle will be asserted on all channels, across all connected
// headstages.
void RHXController::setGlobalSettlePolicy(bool settleWholeHeadstageA, bool settleWholeHeadstageB, bool settleWholeHeadstageC,
                                             bool settleWholeHeadstageD, bool settleAllHeadstages)
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    int value;

    value = (settleAllHeadstages ? 16 : 0) + (settleWholeHeadstageA ? 1 : 0) + (settleWholeHeadstageB ? 2 : 0) +
            (settleWholeHeadstageC ? 4 : 0) + (settleWholeHeadstageD ? 8 : 0);

    dev->SetWireInValue(WireInGlobalSettleSelect_S_USB2, value, 0x001f);
    dev->UpdateWireIns();
}

// Set the function of Digital Out ports 1-8.
// true = Digital Out port controlled by DAC threshold-based spike detector ... false = Digital Out port controlled by digital
// sequencer.  Note: Digital Out ports 9-16 are always controlled by a digital sequencer.
void RHXController::setTtlOutMode(bool mode1, bool mode2, bool mode3, bool mode4, bool mode5, bool mode6, bool mode7, bool mode8)
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    int value = 0;
    value += mode1 ? 1 : 0;
    value += mode2 ? 2 : 0;
    value += mode3 ? 4 : 0;
    value += mode4 ? 8 : 0;
    value += mode5 ? 16 : 0;
    value += mode6 ? 32 : 0;
    value += mode7 ? 64 : 0;
    value += mode8 ? 128 : 0;

    dev->SetWireInValue(WireInTtlOutMode_S_USB2, value, 0x000000ff);
    dev->UpdateWireIns();
}

// Select amp settle mode for all connected chips: useFastSettle false = amplifier low frequency cutoff select
// (recommended mode) ... useFastSettle true = amplifier fast settle (legacy mode from RHD2000 series chips)
void RHXController::setAmpSettleMode(bool useFastSettle)
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInResetRun, (useFastSettle ? 0x08 : 0x00), 0x08); // set amp_settle_mode (0 = amplifier low frequency cutoff select; 1 = amplifier fast settle)
    dev->UpdateWireIns();
}

// Select charge recovery mode for all connected chips: useSwitch false = current-limited charge recovery drivers ...
// useSwitch true = charge recovery switch
void RHXController::setChargeRecoveryMode(bool useSwitch)
{
    if (type != ControllerStimRecordUSB2) return;
    dev->SetWireInValue(WireInResetRun, (useSwitch ? 0x10 : 0x00), 0x10); // set charge_recov_mode (0 = current-limited charge recovery drivers; 1 = charge recovery switch)
    dev->UpdateWireIns();
}

// Set the per-channel sampling rate of the RHD/RHS chips connected to the FPGA.
bool RHXController::setSampleRate(AmplifierSampleRate newSampleRate)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    std::cout << " RHXController::setSampleRate()" << std::endl;

    // Assuming a 100 MHz reference clock is provided to the FPGA, the programmable FPGA clock frequency
    // is given by:
    //
    //       FPGA internal clock frequency = 100 MHz * (M/D) / 2
    //
    // M and D are "multiply" and "divide" integers used in the FPGA's digital clock manager (DCM) phase-
    // locked loop (PLL) frequency synthesizer, and are subject to the following restrictions:
    //
    //                M must have a value in the range of 2 - 256
    //                D must have a value in the range of 1 - 256
    //                M/D must fall in the range of 0.05 - 3.33
    //
    // (See pages 85-86 of Xilinx document UG382 "Spartan-6 FPGA Clocking Resources" for more details.)
    //
    // This variable-frequency clock drives the state machine that controls all SPI communication
    // with the RHD2000 chips.  A complete SPI cycle (consisting of one CS pulse and 16 SCLK pulses)
    // takes 80 clock cycles.  The SCLK period is 4 clock cycles; the CS pulse is high for 14 clock
    // cycles between commands.
    //
    // Rhythm samples all 32 channels and then executes 3 "auxiliary" commands that can be used to read
    // and write from other registers on the chip, or to sample from the temperature sensor or auxiliary ADC
    // inputs, for example.  Therefore, a complete cycle that samples from each amplifier channel takes
    // 80 * (32 + 3) = 80 * 35 = 2800 clock cycles.
    //
    // So the per-channel sampling rate of each amplifier is 2800 times slower than the clock frequency.
    //
    // Based on these design choices, we can use the following values of M and D to generate the following
    // useful amplifier sampling rates for electrophsyiological applications:
    //
    //   M    D     clkout frequency    per-channel sample rate     per-channel sample period
    //  ---  ---    ----------------    -----------------------     -------------------------
    //    7  125          2.80 MHz               1.00 kS/s                 1000.0 usec = 1.0 msec
    //    7  100          3.50 MHz               1.25 kS/s                  800.0 usec
    //   21  250          4.20 MHz               1.50 kS/s                  666.7 usec
    //   14  125          5.60 MHz               2.00 kS/s                  500.0 usec
    //   35  250          7.00 MHz               2.50 kS/s                  400.0 usec
    //   21  125          8.40 MHz               3.00 kS/s                  333.3 usec
    //   14   75          9.33 MHz               3.33 kS/s                  300.0 usec
    //   28  125         11.20 MHz               4.00 kS/s                  250.0 usec
    //    7   25         14.00 MHz               5.00 kS/s                  200.0 usec
    //    7   20         17.50 MHz               6.25 kS/s                  160.0 usec
    //  112  250         22.40 MHz               8.00 kS/s                  125.0 usec
    //   14   25         28.00 MHz              10.00 kS/s                  100.0 usec
    //    7   10         35.00 MHz              12.50 kS/s                   80.0 usec
    //   21   25         42.00 MHz              15.00 kS/s                   66.7 usec
    //   28   25         56.00 MHz              20.00 kS/s                   50.0 usec
    //   35   25         70.00 MHz              25.00 kS/s                   40.0 usec
    //   42   25         84.00 MHz              30.00 kS/s                   33.3 usec
    //
    // To set a new clock frequency, assert new values for M and D (e.g., using okWireIn modules) and
    // pulse DCM_prog_trigger high (e.g., using an okTriggerIn module).  If this module is reset, it
    // reverts to a per-channel sampling rate of 30.0 kS/s.

    unsigned long M, D;

    switch (newSampleRate) {
    case SampleRate1000Hz:
        M = 7;
        D = 125;
        break;
    case SampleRate1250Hz:
        M = 7;
        D = 100;
        break;
    case SampleRate1500Hz:
        M = 21;
        D = 250;
        break;
    case SampleRate2000Hz:
        M = 14;
        D = 125;
        break;
    case SampleRate2500Hz:
        M = 35;
        D = 250;
        break;
    case SampleRate3000Hz:
        M = 21;
        D = 125;
        break;
    case SampleRate3333Hz:
        M = 14;
        D = 75;
        break;
    case SampleRate4000Hz:
        M = 28;
        D = 125;
        break;
    case SampleRate5000Hz:
        M = 7;
        D = 25;
        break;
    case SampleRate6250Hz:
        M = 7;
        D = 20;
        break;
    case SampleRate8000Hz:
        M = 112;
        D = 250;
        break;
    case SampleRate10000Hz:
        M = 14;
        D = 25;
        break;
    case SampleRate12500Hz:
        M = 7;
        D = 10;
        break;
    case SampleRate15000Hz:
        M = 21;
        D = 25;
        break;
    case SampleRate20000Hz:
        M = 28;
        D = 25;
        break;
    case SampleRate25000Hz:
        M = 35;
        D = 25;
        break;
    case SampleRate30000Hz:
        M = 42;
        D = 25;
        break;
    default:
        return false;
    }

    sampleRate = newSampleRate;

    // Wait for DcmProgDone = 1 before reprogramming clock synthesizer.
    while (isDcmProgDone() == false) {}
    // Reprogram clock synthesizer.
    std::cout << "Reprogram clock synthesizer" << " " << WireInDataFreqPll << " " << (256 * M + D) << std::endl;
    dev->SetWireInValue(WireInDataFreqPll, (256 * M + D));
    dev->UpdateWireIns();

    if (type == ControllerRecordUSB3 || type == ControllerOEOpalKellyUSB3) {
        dev->ActivateTriggerIn(TrigInConfig_USB3, 0);
    } else {
        dev->ActivateTriggerIn(TrigInDcmProg_USB2, 0);
    }

    // Wait for DataClkLocked = 1 before allowing data acquisition to continue.
    while (isDataClockLocked() == false) {}

    return true;
}

void RHXController::updateRegisters()
{
    std::cout << "RHXController::updateRegisters()" << std::endl;

    // Set up an RHD2000 register object using this sample rate to
    // optimize MUX-related register settings.

    RHXRegisters chipRegisters(getType(), getSampleRate(), StimStepSize500nA);

    int commandSequenceLength;
    std::vector<unsigned int> commandList;

    commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, true, 60);
    // Upload version with ADC calibration to AuxCmd3 RAM Bank 0.
    uploadCommandList(commandList, AuxCmd3, 0);
    selectAuxCommandLength(AuxCmd3, 0,
        commandSequenceLength - 1);

    commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false, 60);
    // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
    uploadCommandList(commandList, AuxCmd3, 1);
    selectAuxCommandLength(AuxCmd3, 0,
        commandSequenceLength - 1);

    if (false)
    {
        // Create a command list for the AuxCmd1 slot.  This command sequence will continuously
        // update Register 3, which controls the auxiliary digital output pin on each RHD2000 chip.
        // In concert with the v1.4 Rhythm FPGA code, this permits real-time control of the digital
        // output pin on chips on each SPI port.
        chipRegisters.setDigOutLow();   // Take auxiliary output out of HiZ mode.
        commandSequenceLength = chipRegisters.createCommandListRHDUpdateDigOut(commandList, 10);

        uploadCommandList(commandList, AuxCmd1, 0);

        selectAuxCommandLength(AuxCmd1, 0, commandSequenceLength - 1);
        selectAuxCommandBank(PortA, AuxCmd1, 0);
        selectAuxCommandBank(PortB, AuxCmd1, 0);
        selectAuxCommandBank(PortC, AuxCmd1, 0);
        selectAuxCommandBank(PortD, AuxCmd1, 0);

        if (getType() == ControllerRecordUSB3)
        {
            selectAuxCommandBank(PortE, AuxCmd1, 0);
            selectAuxCommandBank(PortF, AuxCmd1, 0);
            selectAuxCommandBank(PortG, AuxCmd1, 0);
            selectAuxCommandBank(PortH, AuxCmd1, 0);

        }

        // Next, we'll create a command list for the AuxCmd2 slot.  This command sequence
        // will sample the temperature sensor and other auxiliary ADC inputs.
        /*commandSequenceLength = chipRegisters.createCommandListTempSensor(commandList);
        uploadCommandList(commandList, AuxCmd2, 0);
        selectAuxCommandLength(AuxCmd2, 0, commandSequenceLength - 1);
        selectAuxCommandBank(PortA, AuxCmd2, 0);
        selectAuxCommandBank(PortB, AuxCmd2, 0);
        selectAuxCommandBank(PortC, AuxCmd2, 0);
        selectAuxCommandBank(PortD, AuxCmd2, 0);

        if (getType() == ControllerRecordUSB3)
        {
            selectAuxCommandBank(PortE, AuxCmd2, 0);
            selectAuxCommandBank(PortF, AuxCmd2, 0);
            selectAuxCommandBank(PortG, AuxCmd2, 0);
            selectAuxCommandBank(PortH, AuxCmd2, 0);
        }*/

        // Before generating register configuration command sequences, set amplifier
        // bandwidth paramters.
        chipRegisters.setDspCutoffFreq(1.0);
        chipRegisters.setLowerBandwidth(1.0);
        chipRegisters.setUpperBandwidth(7500.0);
        chipRegisters.enableDsp(true);

        // enable/disable aux inputs:
        chipRegisters.enableAux1(true);
        chipRegisters.enableAux2(true);
        chipRegisters.enableAux3(true);

        chipRegisters.createCommandListRHDRegisterConfig(commandList, true, 60);
        // Upload version with ADC calibration to AuxCmd3 RAM Bank 0.
        uploadCommandList(commandList, AuxCmd3, 0);
        selectAuxCommandLength(AuxCmd3, 0,
            commandSequenceLength - 1);

        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false, 60);
        // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
        uploadCommandList(commandList, AuxCmd3, 1);
        selectAuxCommandLength(AuxCmd3, 0,
            commandSequenceLength - 1);


        chipRegisters.setFastSettle(true);

        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false, 60);
        // Upload version with fast settle enabled to AuxCmd3 RAM Bank 2.
        uploadCommandList(commandList, AuxCmd3, 2);
        selectAuxCommandLength(AuxCmd3, 0,
            commandSequenceLength - 1);

        chipRegisters.setFastSettle(false);
        selectAuxCommandBank(PortA, AuxCmd3,
            false ? 2 : 1);
        selectAuxCommandBank(PortB, AuxCmd3,
            false ? 2 : 1);
        selectAuxCommandBank(PortC, AuxCmd3,
            false ? 2 : 1);
        selectAuxCommandBank(PortD, AuxCmd3,
            false ? 2 : 1);

        if (getType() == ControllerRecordUSB3)
        {
            selectAuxCommandBank(PortE, AuxCmd3,
                false ? 2 : 1);
            selectAuxCommandBank(PortF, AuxCmd3,
                false ? 2 : 1);
            selectAuxCommandBank(PortG, AuxCmd3,
                false ? 2 : 1);
            selectAuxCommandBank(PortH, AuxCmd3,
                false ? 2 : 1);
        }
    }
}

// Enable or disable one of the 32 available USB data streams (0-31).
void RHXController::enableDataStream(int stream, bool enabled)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    //std::cout << "RHXController::enableDataStream(" << stream << ", " << enabled << ")" << std::endl;

    if (stream < 0 || stream > (maxNumDataStreams() - 1)) {
        std::cerr << "Error in RHXController::enableDataStream: stream out of range.\n";
        return;
    }

    if (enabled) {
        if (dataStreamEnabled[stream] == 0) {
            dev->SetWireInValue(WireInDataStreamEn, 0x00000001 << stream, 0x00000001 << stream);
            dev->UpdateWireIns();
            dataStreamEnabled[stream] = 1;
            numDataStreams++;
        }
    } else {
        if (dataStreamEnabled[stream] == 1) {
            dev->SetWireInValue(WireInDataStreamEn, 0x00000000 << stream, 0x00000001 << stream);
            dev->UpdateWireIns();
            dataStreamEnabled[stream] = 0;
            numDataStreams--;
        }
    }
}

// Enable or disable DAC channel (0-7).
void RHXController::enableDac(int dacChannel, bool enabled)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    //std::cout << "RHXController::enableDac(" << dacChannel << ", " << enabled << ")" << std::endl;

    if ((dacChannel < 0) || (dacChannel > 7)) {
        std::cerr << "Error in RHXController::enableDac: dacChannel out of range.\n";
        return;
    }

    unsigned int enableVal = 0x0200;
    if (type == ControllerRecordUSB3) enableVal = 0x0800;

    switch (dacChannel) {
    case 0:
        dev->SetWireInValue(WireInDacSource1, (enabled ? enableVal : 0), enableVal);
        break;
    case 1:
        dev->SetWireInValue(WireInDacSource2, (enabled ? enableVal : 0), enableVal);
        break;
    case 2:
        dev->SetWireInValue(WireInDacSource3, (enabled ? enableVal : 0), enableVal);
        break;
    case 3:
        dev->SetWireInValue(WireInDacSource4, (enabled ? enableVal : 0), enableVal);
        break;
    case 4:
        dev->SetWireInValue(WireInDacSource5, (enabled ? enableVal : 0), enableVal);
        break;
    case 5:
        dev->SetWireInValue(WireInDacSource6, (enabled ? enableVal : 0), enableVal);
        break;
    case 6:
        dev->SetWireInValue(WireInDacSource7, (enabled ? enableVal : 0), enableVal);
        break;
    case 7:
        dev->SetWireInValue(WireInDacSource8, (enabled ? enableVal : 0), enableVal);
        break;
    }
    dev->UpdateWireIns();
}

// Enable external triggering of RHD amplifier hardware 'fast settle' function (blanking).
// If external triggering is enabled, the fast settling of amplifiers on all connected chips will be controlled in real time
// via one of the 16 TTL inputs.
void RHXController::enableExternalFastSettle(bool enable)
{
    if (type == ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    std::cout << "RHXController::enableExternalFastSettle(" << enable << ")" << std::endl;


    dev->SetWireInValue(WireInMultiUse, enable ? 1 : 0);
    dev->UpdateWireIns();
    if (type == ControllerRecordUSB3) {
        dev->ActivateTriggerIn(TrigInConfig_USB3, 6);
    } else if (type == ControllerRecordUSB2) {
        dev->ActivateTriggerIn(TrigInExtFastSettle_R_USB2, 0);
    }
}

// Enable external control of RHD2000 auxiliary digital output pin (auxout).
// If external control is enabled, the digital output of all chips connected to a selected SPI port will be controlled in
// real time via one of the 16 TTL inputs.
void RHXController::enableExternalDigOut(BoardPort port, bool enable)
{
    if (type == ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    std::cout << "RHXController::enableExternalDigOut(" << port << ", " << enable << ")" << std::endl;

    dev->SetWireInValue(WireInMultiUse, enable ? 1 : 0);
    dev->UpdateWireIns();

    if (type == ControllerRecordUSB2) {
        switch (port) {
        case PortA:
            dev->ActivateTriggerIn(TrigInExtDigOut_R_USB2, 0);
            break;
        case PortB:
            dev->ActivateTriggerIn(TrigInExtDigOut_R_USB2, 1);
            break;
        case PortC:
            dev->ActivateTriggerIn(TrigInExtDigOut_R_USB2, 2);
            break;
        case PortD:
            dev->ActivateTriggerIn(TrigInExtDigOut_R_USB2, 3);
            break;
        default:
            std::cerr << "Error in RHXController::enableExternalDigOut: port out of range.\n";
        }
    } else if (type == ControllerRecordUSB3) {
    switch (port) {
        case PortA:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 16);
            break;
        case PortB:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 17);
            break;
        case PortC:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 18);
            break;
        case PortD:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 19);
            break;
        case PortE:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 20);
            break;
        case PortF:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 21);
            break;
        case PortG:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 22);
            break;
        case PortH:
            dev->ActivateTriggerIn(TrigInDacConfig_USB3, 23);
            break;
        default:
            std::cerr << "Error in RHXController::enableExternalDigOut: port out of range.\n";
        }
    }
}

// Enable optional FPGA-implemented digital high-pass filters associated with DAC outputs on USB interface board.
// These one-pole filters can be used to record wideband neural data while viewing only spikes without LFPs on the
// DAC outputs, for example.  This is useful when using the low-latency FPGA thresholds to detect spikes and produce
// digital pulses on the TTL outputs, for example.
void RHXController::enableDacHighpassFilter(bool enable)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInMultiUse, enable ? 1 : 0);
    dev->UpdateWireIns();
    if (type == ControllerRecordUSB3) {
        dev->ActivateTriggerIn(TrigInConfig_USB3, 4);
    } else {
        dev->ActivateTriggerIn(TrigInDacHpf_USB2, 0);
    }
}

// Enable DAC rereferencing, where a selected amplifier channel is subtracted from all DACs in real time.
void RHXController::enableDacReref(bool enabled)
{
    if (type == ControllerRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (type == ControllerRecordUSB3) {
        dev->SetWireInValue(WireInDacReref_R_USB3, (enabled ? 0x00000400 : 0x00000000), 0x00000400);
    } else if (type == ControllerStimRecordUSB2) {
        dev->SetWireInValue(WireInDacReref_S_USB2, (enabled ? 0x00000100 : 0x00000000), 0x00000100);
    }
    dev->UpdateWireIns();
}

// Enable DC amplifier conversion.
void RHXController::enableDcAmpConvert(bool enable)
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInDcAmpConvert_S_USB2, (enable ? 1 : 0));
    dev->UpdateWireIns();
}

// Enable auxiliary commands slots 0-3 on all data streams (0-7).  This disables automatic stimulation control on all
// data streams.
void RHXController::enableAuxCommandsOnAllStreams()
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInAuxEnable_S_USB2, 0x00ff, 0x00ff);
    dev->UpdateWireIns();
}

// Enable auxiliary commands slots 0-3 on one selected data stream, and disable auxiliary command slots on all other
// data streams.  This disables automatic stimulation control on the selected stream and enables automatic stimulation control
// on all other streams.
void RHXController::enableAuxCommandsOnOneStream(int stream)
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (stream < 0 || stream >(maxNumDataStreams() - 1)) {
        std::cerr << "Error in RHXController::enableAuxCommandsOnOneStream: stream out of range.\n";
        return;
    }

    dev->SetWireInValue(WireInAuxEnable_S_USB2, 0x0001 << stream, 0x00ff);
    dev->UpdateWireIns();
}

// Assign a particular data stream (0-31) to a DAC channel (0-7).  Setting stream to 32 selects DacManual value.
void RHXController::selectDacDataStream(int dacChannel, int stream)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((dacChannel < 0) || (dacChannel > 7)) {
        std::cerr << "Error in RHXController::selectDacDataStream: dacChannel out of range.\n";
        return;
    }

    int maxStream = 0;
    switch (type) {
    case ControllerRecordUSB2:
        maxStream = 9;
        break;
    case ControllerRecordUSB3:
        maxStream = 32;
        break;
    case ControllerStimRecordUSB2:
        maxStream = 8;
        break;
    }

    if (stream < 0 || stream > maxStream) {
        std::cerr << "Error in RHXController::selectDacDataStream: stream out of range.\n";
        return;
    }

    unsigned int mask = 0x01e0;
    if (type == ControllerRecordUSB3) mask = 0x07e0;

    switch (dacChannel) {
    case 0:
        dev->SetWireInValue(WireInDacSource1, stream << 5, mask);
        break;
    case 1:
        dev->SetWireInValue(WireInDacSource2, stream << 5, mask);
        break;
    case 2:
        dev->SetWireInValue(WireInDacSource3, stream << 5, mask);
        break;
    case 3:
        dev->SetWireInValue(WireInDacSource4, stream << 5, mask);
        break;
    case 4:
        dev->SetWireInValue(WireInDacSource5, stream << 5, mask);
        break;
    case 5:
        dev->SetWireInValue(WireInDacSource6, stream << 5, mask);
        break;
    case 6:
        dev->SetWireInValue(WireInDacSource7, stream << 5, mask);
        break;
    case 7:
        dev->SetWireInValue(WireInDacSource8, stream << 5, mask);
        break;
    }
    dev->UpdateWireIns();
}

// Assign a particular amplifier channel (0-31) to a DAC channel (0-7).
void RHXController::selectDacDataChannel(int dacChannel, int dataChannel)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((dacChannel < 0) || (dacChannel > 7)) {
        std::cerr << "Error in RHXController::selectDacDataChannel: dacChannel out of range.\n";
        return;
    }

    if ((dataChannel < 0) || (dataChannel > 31)) {
        std::cerr << "Error in RHXController::selectDacDataChannel: dataChannel out of range.\n";
        return;
    }

    switch (dacChannel) {
    case 0:
        dev->SetWireInValue(WireInDacSource1, dataChannel << 0, 0x001f);
        break;
    case 1:
        dev->SetWireInValue(WireInDacSource2, dataChannel << 0, 0x001f);
        break;
    case 2:
        dev->SetWireInValue(WireInDacSource3, dataChannel << 0, 0x001f);
        break;
    case 3:
        dev->SetWireInValue(WireInDacSource4, dataChannel << 0, 0x001f);
        break;
    case 4:
        dev->SetWireInValue(WireInDacSource5, dataChannel << 0, 0x001f);
        break;
    case 5:
        dev->SetWireInValue(WireInDacSource6, dataChannel << 0, 0x001f);
        break;
    case 6:
        dev->SetWireInValue(WireInDacSource7, dataChannel << 0, 0x001f);
        break;
    case 7:
        dev->SetWireInValue(WireInDacSource8, dataChannel << 0, 0x001f);
        break;
    }
    dev->UpdateWireIns();
}

// Specify a command sequence length (endIndex = 0-1023) and command loop index (0-1023) for a particular auxiliary
// command slot (AuxCmd1, AuxCmd2, or AuxCmd3).
void RHXController::selectAuxCommandLength(AuxCmdSlot auxCommandSlot, int loopIndex, int endIndex)
{

    //std::cout << "RHXController::selectAuxCommandLength " << auxCommandSlot  << " " << loopIndex << " " << endIndex << std::endl;

    std::lock_guard<std::mutex> lockOk(okMutex);
    int maxIndex = (type == ControllerStimRecordUSB2) ? 8192 : 1024;

    if (loopIndex < 0 || loopIndex > maxIndex - 1) {
        std::cerr << "Error in RHXController::selectAuxCommandLength: loopIndex out of range.\n";
        return;
    }

    if (endIndex < 0 || endIndex > maxIndex - 1) {
        std::cerr << "Error in RHXController::selectAuxCommandLength: endIndex out of range.\n";
        return;
    }

    switch (type) {
    case (ControllerRecordUSB2):
        switch (auxCommandSlot) {
        case AuxCmd1:
            dev->SetWireInValue(WireInAuxCmdLoop1_R_USB2, loopIndex);
            dev->SetWireInValue(WireInAuxCmdLength1_R_USB2, endIndex);
            break;
        case AuxCmd2:
            dev->SetWireInValue(WireInAuxCmdLoop2_R_USB2, loopIndex);
            dev->SetWireInValue(WireInAuxCmdLength2_R_USB2, endIndex);
            break;
        case AuxCmd3:
            dev->SetWireInValue(WireInAuxCmdLoop3_R_USB2, loopIndex);
            dev->SetWireInValue(WireInAuxCmdLength3_R_USB2, endIndex);
            break;
        case AuxCmd4:
            // Should not be reached, as AuxCmd4 is Stim-only.
            break;
        }
        dev->UpdateWireIns();
        break;
    case (ControllerRecordUSB3):
        switch (auxCommandSlot) {
        case AuxCmd1:
            dev->SetWireInValue(WireInAuxCmdLoop_R_USB3, loopIndex, 0x000003ff);
            dev->SetWireInValue(WireInAuxCmdLength_R_USB3, endIndex, 0x000003ff);
            break;
        case AuxCmd2:
            dev->SetWireInValue(WireInAuxCmdLoop_R_USB3, loopIndex << 10, 0x000003ff << 10);
            dev->SetWireInValue(WireInAuxCmdLength_R_USB3, endIndex << 10, 0x000003ff << 10);
            break;
        case AuxCmd3:
            dev->SetWireInValue(WireInAuxCmdLoop_R_USB3, loopIndex << 20, 0x000003ff << 20);
            dev->SetWireInValue(WireInAuxCmdLength_R_USB3, endIndex << 20, 0x000003ff << 20);
            break;
        case AuxCmd4:
            // Should not be reached, as AuxCmd4 is Stim-only.
            break;
        }
        dev->UpdateWireIns();
        break;
    case (ControllerStimRecordUSB2)://sumedh
        int auxCommandIndex = (int)auxCommandSlot;
        if ((auxCommandIndex < 0) || (auxCommandIndex > 3)) {
            std::cerr << "Error in RHXController::selectAuxCommandLength: auxCommandSlot out of range.\n";
        }

        dev->SetWireInValue(WireInMultiUse, loopIndex);
        dev->UpdateWireIns();
        dev->ActivateTriggerIn(TrigInAuxCmdLength_S_USB2, auxCommandIndex + 4);
        dev->SetWireInValue(WireInMultiUse, endIndex);
        dev->UpdateWireIns();
        dev->ActivateTriggerIn(TrigInAuxCmdLength_S_USB2, auxCommandIndex);
        break;
    }
}

// Select an auxiliary command slot (AuxCmd1, AuxCmd2, or AuxCmd3) and bank (0-15) for a particular SPI port
// (PortA - PortH) on the FPGA.
void RHXController::selectAuxCommandBank(BoardPort port, AuxCmdSlot auxCommandSlot, int bank)
{
    if (type == ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);
    int bitShift;

    std::cout << "RHXController::selectAuxCommandBank " << port << " " << auxCommandSlot << " " << bank << std::endl;

    if (auxCommandSlot != AuxCmd1 && auxCommandSlot != AuxCmd2 && auxCommandSlot != AuxCmd3) {
        std::cerr << "Error in RHXController::selectAuxCommandBank: auxCommandSlot out of range.\n";
        return;
    }
    if ((bank < 0) || (bank > 15)) {
        std::cerr << "Error in RHXController::selectAuxCommandBank: bank out of range.\n";
        return;
    }

    switch (port) {
    case PortA:
        bitShift = 0;
        break;
    case PortB:
        bitShift = 4;
        break;
    case PortC:
        bitShift = 8;
        break;
    case PortD:
        bitShift = 12;
        break;
    case PortE:
        bitShift = 16;
        break;
    case PortF:
        bitShift = 20;
        break;
    case PortG:
        bitShift = 24;
        break;
    case PortH:
        bitShift = 28;
        break;
    }

    switch (auxCommandSlot) {
    case AuxCmd1:
        dev->SetWireInValue(WireInAuxCmdBank1_R, bank << bitShift, 0x0000000f << bitShift);
        break;
    case AuxCmd2:
        dev->SetWireInValue(WireInAuxCmdBank2_R, bank << bitShift, 0x0000000f << bitShift);
        break;
    case AuxCmd3:
        dev->SetWireInValue(WireInAuxCmdBank3_R, bank << bitShift, 0x0000000f << bitShift);
        break;
    case AuxCmd4:
        // Should not be reached, as AuxCmd4 is Stim-only.
        break;

    }
    dev->UpdateWireIns();
}

// Return 4-bit "board mode" input.
int RHXController::getBoardMode()
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    return getBoardMode(dev);
}

// Return number of SPI ports and if I/O expander board is present.
int RHXController::getNumSPIPorts(bool& expanderBoardDetected)
{
    if (type == ControllerRecordUSB2) {
        expanderBoardDetected = true;
        return 4;
    }
    std::lock_guard<std::mutex> lockOk(okMutex);
    return getNumSPIPorts(dev, (type == ControllerRecordUSB3), expanderBoardDetected);
}

// Set all 16 bits of the digital TTL output lines on the FPGA to zero.  Not used with ControllerStimRecordUSB2.
void RHXController::clearTtlOut()
{
    if (type == ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);
    dev->SetWireInValue(WireInTtlOut_R, 0x0000);
    dev->UpdateWireIns();
}

// Reset stimulation sequencer units.  This is typically called when data acquisition is stopped.
// It is possible that a stimulation sequencer could be in the middle of playing out a long pulse train
// (e.g., 100 stimulation pulses).  If this function is not called, the pulse train will resume after data acquisition
// is restarted.
void RHXController::resetSequencers()
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->ActivateTriggerIn(TrigInSpiStart, 1);
}

// Set a particular stimulation control register.
void RHXController::programStimReg(int stream, int channel, StimRegister reg, int value)
{
    if (type != ControllerStimRecordUSB2) return;
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInStimRegAddr_S_USB2, (stream << 8) + (channel << 4) + reg);
    dev->SetWireInValue(WireInStimRegWord_S_USB2, value);
    dev->UpdateWireIns();
    dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 1);
}

// Upload an auxiliary command list to a particular command slot and RAM bank (0-15) on the FPGA.
void RHXController::uploadCommandList(const std::vector<unsigned int> &commandList, AuxCmdSlot auxCommandSlot, int bank)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (type != ControllerStimRecordUSB2) {
        if (auxCommandSlot != AuxCmd1 && auxCommandSlot != AuxCmd2 && auxCommandSlot != AuxCmd3) {
            std::cerr << "Error in RHXController::uploadCommandList: auxCommandSlot out of range.\n";
            return;
        }

        if ((bank < 0) || (bank > 15)) {
            std::cerr << "Error in RHXController::uploadCommandList: bank out of range.\n";
            return;
        }

        for (unsigned int i = 0; i < commandList.size(); ++i) {
            dev->SetWireInValue(WireInCmdRamData_R, commandList[i]);
            dev->SetWireInValue(WireInCmdRamAddr_R, i);
            dev->SetWireInValue(WireInCmdRamBank_R, bank);
            dev->UpdateWireIns();
            if (type == ControllerRecordUSB2) {
                switch (auxCommandSlot) {
                case AuxCmd1:
                    dev->ActivateTriggerIn(TrigInRamWrite_R_USB2, 0);
                    break;
                case AuxCmd2:
                    dev->ActivateTriggerIn(TrigInRamWrite_R_USB2, 1);
                    break;
                case AuxCmd3:
                    dev->ActivateTriggerIn(TrigInRamWrite_R_USB2, 2);
                    break;
                case AuxCmd4:
                    // Should not be reached, as AuxCmd4 is Stim-only.
                    break;

                }
            } else {
                switch (auxCommandSlot) {
                case AuxCmd1:
                    dev->ActivateTriggerIn(TrigInConfig_USB3, 1);
                    break;
                case AuxCmd2:
                    dev->ActivateTriggerIn(TrigInConfig_USB3, 2);
                    break;
                case AuxCmd3:
                    dev->ActivateTriggerIn(TrigInConfig_USB3, 3);
                    break;
                case AuxCmd4:
                    // Should not be reached, as AuxCmd4 is Stim-only.
                    break;
                }
            }
        }
    } else {
        for (unsigned int i = 0; i < commandList.size(); ++i) {
            commandBufferMsw[2 * i] = (uint8_t)((commandList[i] & 0x00ff0000) >> 16);
            commandBufferMsw[2 * i + 1] = (uint8_t)((commandList[i] & 0xff000000) >> 24);
            commandBufferLsw[2 * i] = (uint8_t)((commandList[i] & 0x000000ff) >> 0);
            commandBufferLsw[2 * i + 1] = (uint8_t)((commandList[i] & 0x0000ff00) >> 8);
        }

        switch (auxCommandSlot) {
        case AuxCmd1:
            dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 0);
            dev->WriteToPipeIn(PipeInAuxCmd1Msw_S_USB2, 2 * (int)commandList.size(), commandBufferMsw);
            dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 0);
            dev->WriteToPipeIn(PipeInAuxCmd1Lsw_S_USB2, 2 * (int)commandList.size(), commandBufferLsw);
            break;
        case AuxCmd2:
            dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 0);
            dev->WriteToPipeIn(PipeInAuxCmd2Msw_S_USB2, 2 * (int)commandList.size(), commandBufferMsw);
            dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 0);
            dev->WriteToPipeIn(PipeInAuxCmd2Lsw_S_USB2, 2 * (int)commandList.size(), commandBufferLsw);
            break;
        case AuxCmd3:
            dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 0);
            dev->WriteToPipeIn(PipeInAuxCmd3Msw_S_USB2, 2 * (int)commandList.size(), commandBufferMsw);
            dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 0);
            dev->WriteToPipeIn(PipeInAuxCmd3Lsw_S_USB2, 2 * (int)commandList.size(), commandBufferLsw);
            break;
        case AuxCmd4:
            dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 0);
            dev->WriteToPipeIn(PipeInAuxCmd4Msw_S_USB2, 2 * (int)commandList.size(), commandBufferMsw);
            dev->ActivateTriggerIn(TrigInRamAddrReset_S_USB2, 0);
            dev->WriteToPipeIn(PipeInAuxCmd4Lsw_S_USB2, 2 * (int)commandList.size(), commandBufferLsw);
            break;
        default:
            std::cerr << "Error in RHXController::uploadCommandList: auxCommandSlot out of range.\n";
            break;
        }
    }
}

// Simple board reset
void RHXController::resetBoard(okCFrontPanel* dev_)
{
    dev_->SetWireInValue(endPointWireInResetRun(), 0x01, 0x01);
    dev_->UpdateWireIns();
    dev_->SetWireInValue(endPointWireInResetRun(), 0x00, 0x01);
    dev_->UpdateWireIns();
}

// Return 4-bit "board mode" input.
int RHXController::getBoardMode(okCFrontPanel* dev_)
{
    dev_->UpdateWireOuts();
    return dev_->GetWireOutValue(endPointWireOutBoardMode());
}

// Return number of SPI ports and if I/O expander board is present.
int RHXController::getNumSPIPorts(okCFrontPanel* dev_, bool isUSB3, bool& expanderBoardDetected)
{
    bool spiPortPresent[8];
    bool userId[3];
    bool serialId[4];
    bool digOutVoltageLevel;

    int WireOutSerialDigitalIn = endPointWireOutSerialDigitalIn(isUSB3);
    int WireInSerialDigitalInCntl = endPointWireInSerialDigitalInCntl(isUSB3);

    dev_->UpdateWireOuts();
    expanderBoardDetected = (dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x04) != 0;
    // int expanderBoardIdNumber = ((dev->GetWireOutValue(WireOutSerialDigitalIn) & 0x08) ? 1 : 0);

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 2);  // Load digital in shift registers on falling edge of serial_LOAD
    spiPortPresent[7] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    spiPortPresent[6] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    spiPortPresent[5] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    spiPortPresent[4] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    spiPortPresent[3] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    spiPortPresent[2] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    spiPortPresent[1] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    spiPortPresent[0] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    digOutVoltageLevel = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    userId[2] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    userId[1] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    userId[0] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    serialId[3] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    serialId[2] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    serialId[1] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    pulseWireIn(dev_, WireInSerialDigitalInCntl, 1);
    serialId[0] = dev_->GetWireOutValue(WireOutSerialDigitalIn) & 0x01;

    int numPorts = 0;
    for (int i = 0; i < 8; i++) {
        if (spiPortPresent[i]) {
            numPorts++;
        }
    }

//    std::cout << "expanderBoardDetected: " << expanderBoardDetected << '\n';
//    std::cout << "expanderBoardId: " << expanderBoardIdNumber << '\n';
//    std::cout << "spiPortPresent: " << spiPortPresent[7] << spiPortPresent[6] << spiPortPresent[5] << spiPortPresent[4] << spiPortPresent[3] << spiPortPresent[2] << spiPortPresent[1] << spiPortPresent[0] << '\n';
//    std::cout << "serialId: " << serialId[3] << serialId[2] << serialId[1] << serialId[0] << '\n';
//    std::cout << "userId: " << userId[2] << userId[1] << userId[0] << '\n';
//    std::cout << "digOutVoltageLevel: " << digOutVoltageLevel << '\n';

    return numPorts;
}

// Return the number of 16-bit words in the USB FIFO.  The user should never attempt to read more data than the FIFO
// currently contains, as it is not protected against underflow.
unsigned int RHXController::numWordsInFifo()
{
    dev->UpdateWireOuts();

    if (type == ControllerRecordUSB3) {
        lastNumWordsInFifo = dev->GetWireOutValue(WireOutNumWords_USB3);
    } else {
        lastNumWordsInFifo = (dev->GetWireOutValue(WireOutNumWordsMsb_USB2) << 16) +
                dev->GetWireOutValue(WireOutNumWordsLsb_USB2);
    }
    numWordsHasBeenUpdated = true;

    return lastNumWordsInFifo;
}

// Is variable-frequency clock DCM programming done?
bool RHXController::isDcmProgDone() const
{
    dev->UpdateWireOuts();
    int value = dev->GetWireOutValue(WireOutDataClkLocked);
    return ((value & 0x0002) > 1);
}

// Is variable-frequency clock PLL locked?
bool RHXController::isDataClockLocked() const
{
    dev->UpdateWireOuts();
    int value = dev->GetWireOutValue(WireOutDataClkLocked);
    return ((value & 0x0001) > 0);
}

// Force all data streams off, used in FPGA initialization.
void RHXController::forceAllDataStreamsOff()
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    dev->SetWireInValue(WireInDataStreamEn, 0x00000000);
    dev->UpdateWireIns();
}

// Manually pulse WireIns.
void RHXController::pulseWireIn(okCFrontPanel* dev_, int wireIn, unsigned int value)
{
    dev_->SetWireInValue(wireIn, value);
    dev_->UpdateWireIns();
    dev_->SetWireInValue(wireIn, 0);
    dev_->UpdateWireIns();

    dev_->UpdateWireOuts();
}

// Return the EndPoint address for WireInSerialDigitalInCntl (depending on USB2 or USB3).
int RHXController::endPointWireInSerialDigitalInCntl(bool isUSB3)
{
    if (isUSB3) return (int)WireInSerialDigitalInCntl_R_USB3;
    else return (int)WireInSerialDigitalInCntl_S_USB2;
}

// Return the EndPoint address for WireOutSerialDigitalIn (depending on USB2 or USB3).
int RHXController::endPointWireOutSerialDigitalIn(bool isUSB3)
{
    if (isUSB3) return (int)WireOutSerialDigitalIn_R_USB3;
    else return (int)WireOutSerialDigitalIn_S_USB2;
}

