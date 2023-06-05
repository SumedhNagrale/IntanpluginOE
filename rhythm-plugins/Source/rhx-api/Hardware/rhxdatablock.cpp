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
#include <cstring>
#include "rhxdatablock.h"

RHXDataBlock::RHXDataBlock(ControllerType type_, int numDataStreams_) :
    type(type_),
    numDataStreams(numDataStreams_),
    timeStampInternal(nullptr),
    amplifierDataInternal(nullptr),
    auxiliaryDataInternal(nullptr),
    boardAdcDataInternal(nullptr),
    ttlInInternal(nullptr),
    ttlOutInternal(nullptr),
    dcAmplifierDataInternal(nullptr),
    complianceLimitInternal(nullptr),
    stimOnInternal(nullptr),
    stimPolInternal(nullptr),
    ampSettleInternal(nullptr),
    chargeRecovInternal(nullptr),
    boardDacDataInternal(nullptr)
{
    allocateMemory();
}

RHXDataBlock::~RHXDataBlock()
{
    if (timeStampInternal) delete [] timeStampInternal;
    if (amplifierDataInternal) delete [] amplifierDataInternal;
    if (auxiliaryDataInternal) delete [] auxiliaryDataInternal;
    if (boardAdcDataInternal) delete [] boardAdcDataInternal;
    if (ttlInInternal) delete [] ttlInInternal;
    if (ttlOutInternal) delete [] ttlOutInternal;
    if (dcAmplifierDataInternal) delete [] dcAmplifierDataInternal;
    if (complianceLimitInternal) delete [] complianceLimitInternal;
    if (stimOnInternal) delete [] stimOnInternal;
    if (stimPolInternal) delete [] stimPolInternal;
    if (ampSettleInternal) delete [] ampSettleInternal;
    if (chargeRecovInternal) delete [] chargeRecovInternal;
    if (boardDacDataInternal) delete [] boardDacDataInternal;
}

// Copy constructor
RHXDataBlock::RHXDataBlock(const RHXDataBlock &obj)
{
    type = obj.type;
    numDataStreams = obj.numDataStreams;
    allocateMemory();
    std::memcpy(timeStampInternal, obj.timeStampInternal, sizeof(uint32_t) * samplesPerDataBlock());
    std::memcpy(amplifierDataInternal, obj.amplifierDataInternal,
                sizeof(int) * numDataStreams * channelsPerStream() * samplesPerDataBlock());
    std::memcpy(auxiliaryDataInternal, obj.auxiliaryDataInternal,
                sizeof(int) * numDataStreams * numAuxChannels() * samplesPerDataBlock());
    std::memcpy(boardAdcDataInternal, obj.boardAdcDataInternal, sizeof(int) * 8 * samplesPerDataBlock());
    std::memcpy(ttlInInternal, obj.ttlInInternal, sizeof(int) * samplesPerDataBlock());
    std::memcpy(ttlOutInternal, obj.ttlOutInternal, sizeof(int) * samplesPerDataBlock());
    if (type == ControllerStimRecordUSB2) {
        std::memcpy(dcAmplifierDataInternal, obj.dcAmplifierDataInternal,
                    sizeof(int) * numDataStreams * channelsPerStream() * samplesPerDataBlock());
        std::memcpy(complianceLimitInternal, obj.complianceLimitInternal,
                    sizeof(int) * numDataStreams * channelsPerStream() * samplesPerDataBlock());
        std::memcpy(stimOnInternal, obj.stimOnInternal, sizeof(int) * numDataStreams * samplesPerDataBlock());
        std::memcpy(stimPolInternal, obj.stimPolInternal, sizeof(int) * numDataStreams * samplesPerDataBlock());
        std::memcpy(ampSettleInternal, obj.ampSettleInternal, sizeof(int) * numDataStreams * samplesPerDataBlock());
        std::memcpy(chargeRecovInternal, obj.chargeRecovInternal, sizeof(int) * numDataStreams * samplesPerDataBlock());
        std::memcpy(boardDacDataInternal, obj.boardDacDataInternal, sizeof(int) * 8 * samplesPerDataBlock());
    }
}

void RHXDataBlock::allocateMemory()
{
    timeStampInternal = new uint32_t [samplesPerDataBlock()];
    amplifierDataInternal = new int [numDataStreams * channelsPerStream() * samplesPerDataBlock()];
    auxiliaryDataInternal = new int [numDataStreams * numAuxChannels() * samplesPerDataBlock()];
    boardAdcDataInternal = new int [8 * samplesPerDataBlock()];
    ttlInInternal = new int [samplesPerDataBlock()];
    ttlOutInternal = new int [samplesPerDataBlock()];
    if (type == ControllerStimRecordUSB2) {
        dcAmplifierDataInternal = new int [numDataStreams * channelsPerStream() * samplesPerDataBlock()];
        complianceLimitInternal = new int [numDataStreams * channelsPerStream() * samplesPerDataBlock()];
        stimOnInternal = new int [numDataStreams * samplesPerDataBlock()];
        stimPolInternal = new int [numDataStreams * samplesPerDataBlock()];
        ampSettleInternal = new int [numDataStreams * samplesPerDataBlock()];
        chargeRecovInternal = new int [numDataStreams * samplesPerDataBlock()];
        boardDacDataInternal = new int [8 * samplesPerDataBlock()];
    }
}

uint32_t RHXDataBlock::timeStamp(int t) const
{
    return timeStampInternal[t];
}

int RHXDataBlock::amplifierData(int stream, int channel, int t) const
{
    return amplifierDataInternal[(t * numDataStreams * channelsPerStream()) + (channel * numDataStreams) + stream];
}

int RHXDataBlock::auxiliaryData(int stream, int channel, int t) const
{
    return auxiliaryDataInternal[(t * numDataStreams * numAuxChannels()) + (channel * numDataStreams) + stream];
}

int RHXDataBlock::boardAdcData(int channel, int t) const
{
    return boardAdcDataInternal[(t * 8) + channel];
}

int RHXDataBlock::ttlIn(int channel, int t) const
{
    return (ttlInInternal[t] & (1 << channel)) ? 1 : 0;
}

int RHXDataBlock::ttlOut(int channel, int t) const
{
    return (ttlOutInternal[t] & (1 << channel)) ? 1 : 0;
}

int RHXDataBlock::dcAmplifierData(int stream, int channel, int t) const
{
    return dcAmplifierDataInternal[(t * numDataStreams * channelsPerStream()) + (channel * numDataStreams) + stream];
}

int RHXDataBlock::complianceLimit(int stream, int channel, int t) const
{
    return complianceLimitInternal[(t * numDataStreams * channelsPerStream()) + (stream * channelsPerStream()) + channel];
}

int RHXDataBlock::stimOn(int stream, int channel, int t) const
{
    return (stimOnInternal[(t * numDataStreams) + stream] & (1 << channel)) ? 1 : 0;
}

int RHXDataBlock::stimPol(int stream, int channel, int t) const
{
    return (stimPolInternal[(t * numDataStreams) + stream] & (1 << channel)) ? 1 : 0;
}

int RHXDataBlock::ampSettle(int stream, int channel, int t) const
{
    return (ampSettleInternal[(t * numDataStreams) + stream] & (1 << channel)) ? 1 : 0;
}

int RHXDataBlock::chargeRecov(int stream, int channel, int t) const
{
    return (chargeRecovInternal[(t * numDataStreams) + stream] & (1 << channel)) ? 1 : 0;
}

int RHXDataBlock::boardDacData(int channel, int t) const
{
    return boardDacDataInternal[(t * 8) + channel];
}
//sumedh entire function replaced
int RHXDataBlock::samplesPerDataBlock(ControllerType type_ )
{
//    switch (type_) {
//    case ControllerRecordUSB2:
//        // return 60;
//    case ControllerStimRecordUSB2:
//    case ControllerRecordUSB3:
//        return 128;
//    default:
//        return 0;
//    }
    return 128;
}

int RHXDataBlock::samplesPerDataBlock() const
{
//    return samplesPerDataBlock(type);
    return 128;
}

// Return the number of RHX data blocks that should be read over the USB interface each time for an approximate
// USB read rate of 30 Hz.
int RHXDataBlock::blocksFor30Hz(AmplifierSampleRate rate)
{
    switch (rate) {
    case SampleRate30000Hz:
        return 8;
    case SampleRate25000Hz:
        return 6;
    case SampleRate20000Hz:
        return 5;
    case SampleRate15000Hz:
        return 4;
    case SampleRate12500Hz:
    case SampleRate10000Hz:
        return 3;
    case SampleRate8000Hz:
    case SampleRate6250Hz:
        return 2;
    case SampleRate5000Hz:
    case SampleRate4000Hz:
    case SampleRate3333Hz:
    case SampleRate3000Hz:
    case SampleRate2500Hz:
    case SampleRate2000Hz:
    case SampleRate1500Hz:
    case SampleRate1250Hz:
    case SampleRate1000Hz:
    default:
        return 1;
    }
}

int RHXDataBlock::channelsPerStream(ControllerType type_)
{
    switch (type_) {
    case ControllerRecordUSB2:
    case ControllerRecordUSB3:
        return 32;
    case ControllerStimRecordUSB2:
        return 16;
    default:
        return 0;
    }
}

int RHXDataBlock::numAuxChannels(ControllerType type_)
{
    switch (type_) {
    case ControllerRecordUSB2:
    case ControllerRecordUSB3:
        return 3;
    case ControllerStimRecordUSB2:
        return 4;
    default:
        return 0;
    }
}

unsigned int RHXDataBlock::dataBlockSizeInWords(ControllerType type_, int numDataStreams_)
{
    switch (type_) {
    case ControllerRecordUSB2:
        return samplesPerDataBlock(type_) * (4 + 2 + numDataStreams_ * (channelsPerStream(type_) + numAuxChannels(type_) + 1) + 8 + 2);
        // 4 = magic number; 2 = time stamp; 36 = (32 amp channels + 3 aux commands + 1 filler word); 8 = ADCs; 2 = TTL in/out
    case ControllerRecordUSB3:
        return samplesPerDataBlock(type_) * (4 + 2 + (numDataStreams_ * (channelsPerStream(type_) + numAuxChannels(type_))) + (numDataStreams_ % 4) + 8 + 2);
        // 4 = magic number; 2 = time stamp; 35 = (32 amp channels + 3 aux commands); 0-3 filler words; 8 = ADCs; 2 = TTL in/out
    case ControllerStimRecordUSB2:
        return samplesPerDataBlock(type_) * (4 + 2 + numDataStreams_ * (2 * (channelsPerStream(type_) + numAuxChannels(type_)) + 4) + 8 + 8 + 2);
        // 4 = magic number; 2 = time stamp; 20 = (16 amp channels + 4 aux commands, each 32 bit results);
        // 4 = stim control params; 8 = DACs; 8 = ADCs; 2 = TTL in/out
    default:
        return 0;
    }
}

uint64_t RHXDataBlock::headerMagicNumber(ControllerType type_)
{
    switch (type_) {
    case ControllerRecordUSB2:
        return HeaderRecordUSB2;
    case ControllerStimRecordUSB2:
        return HeaderStimRecordUSB2;
    case ControllerRecordUSB3:
        return HeaderRecordUSB3;
    default:
        return 0;
    }
}

uint64_t RHXDataBlock::headerMagicNumber() const
{
    return headerMagicNumber(type);
}

bool RHXDataBlock::checkUsbHeader(const uint8_t* usbBuffer, int index, ControllerType type_)
{
    uint64_t header = headerMagicNumber(type_);

    // Just check first byte initially to speed up cases where header doesn't match.
    if (usbBuffer[index] != (uint8_t) (header & 0xffU)) return false;

    uint64_t x1 = usbBuffer[index];
    uint64_t x2 = usbBuffer[index + 1];
    uint64_t x3 = usbBuffer[index + 2];
    uint64_t x4 = usbBuffer[index + 3];
    uint64_t x5 = usbBuffer[index + 4];
    uint64_t x6 = usbBuffer[index + 5];
    uint64_t x7 = usbBuffer[index + 6];
    uint64_t x8 = usbBuffer[index + 7];

    uint64_t usbHeader = (x8 << 56) + (x7 << 48) + (x6 << 40) + (x5 << 32) + (x4 << 24) + (x3 << 16) + (x2 << 8) + (x1 << 0);

    return (usbHeader == header);
}

bool RHXDataBlock::checkUsbHeader(const uint8_t* usbBuffer, int index) const
{
    return checkUsbHeader(usbBuffer, index, type);
}

// This function assumes that a command list created either by createCommandListRHDRegisterConfig
// or createCommandListRHSRegisterConfig from RHXRegisters has been uploaded and run, and the resulting
// RHXDataBlock read first.
int RHXDataBlock::getChipID(int stream, int auxCmdSlot, int &register59Value) const
{
    bool intanChipPresent;

    if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
        // First, check ROM registers 32-36 to verify that they hold 'INTAN', and
        // the initial chip name ROM registers 24-26 that hold 'RHD'.
        // This is just used to verify that we are getting good data over the SPI
        // communication channel.
        intanChipPresent = ((char) auxiliaryData(stream, auxCmdSlot, 32) == 'I' &&
                            (char) auxiliaryData(stream, auxCmdSlot, 33) == 'N' &&
                            (char) auxiliaryData(stream, auxCmdSlot, 34) == 'T' &&
                            (char) auxiliaryData(stream, auxCmdSlot, 35) == 'A' &&
                            (char) auxiliaryData(stream, auxCmdSlot, 36) == 'N' &&
                            (char) auxiliaryData(stream, auxCmdSlot, 24) == 'R' &&
                            (char) auxiliaryData(stream, auxCmdSlot, 25) == 'H' &&
                            (char) auxiliaryData(stream, auxCmdSlot, 26) == 'D');

        // If the SPI communication is bad, return -1.  Otherwise, return the Intan
        // chip ID number stored in ROM regstier 63.
        if (!intanChipPresent) {
            register59Value = -1;
            return -1;
        } else {
            register59Value = auxiliaryData(stream, auxCmdSlot, 23); // Register 59
            return auxiliaryData(stream, auxCmdSlot, 19); // chip ID (Register 63)
        }
    } else if (type == ControllerStimRecordUSB2) {
        register59Value = -1; // Only used for RHD2164 chips
        // First, check ROM registers 251-253 to verify that they hold 'INTAN'.
        // This is just used to verify that we are getting good data over the SPI
        // communication channel.
        intanChipPresent = ((char) ((auxiliaryData(stream, auxCmdSlot, 61) & 0xff00) >> 8) == 'I' &&
                            (char) ((auxiliaryData(stream, auxCmdSlot, 61) & 0x00ff) >> 0) == 'N' &&
                            (char) ((auxiliaryData(stream, auxCmdSlot, 60) & 0xff00) >> 8) == 'T' &&
                            (char) ((auxiliaryData(stream, auxCmdSlot, 60) & 0x00ff) >> 0) == 'A' &&
                            (char) ((auxiliaryData(stream, auxCmdSlot, 59) & 0xff00) >> 8) == 'N' &&
                            (char) ((auxiliaryData(stream, auxCmdSlot, 59) & 0x00ff) >> 0) == 0);

        if (!intanChipPresent) {
            return -1;
        } else {
            return auxiliaryData(stream, auxCmdSlot, 57); // chip ID (Register 255)
        }
    }
    return -1;
}

// Print the contents of RHD2000 registers from a selected USB data stream (0-7)
// to the console.
void RHXDataBlock::print(int stream) const
{
    const int RamOffset = 37;

    std::cout << std::endl;
    std::cout << "RHX  Data Block contents:" << std::endl;
    std::cout << "  ROM contents:" << std::endl;
    std::cout << "    Chip Name: " <<
        (char)auxiliaryData(stream, 2, 50) <<
        (char)auxiliaryData(stream, 2, 25) <<
        (char)auxiliaryData(stream, 2, 26) <<
        (char)auxiliaryData(stream, 2, 27) <<
        (char)auxiliaryData(stream, 2, 28) <<
        (char)auxiliaryData(stream, 2, 29) <<
        (char)auxiliaryData(stream, 2, 30) <<
        (char)auxiliaryData(stream, 2, 31) << std::endl;
    std::cout << "    Company Name:" <<
        (char)auxiliaryData(stream, 2, 32) <<
        (char)auxiliaryData(stream, 2, 33) <<
        (char)auxiliaryData(stream, 2, 34) <<
        (char)auxiliaryData(stream, 2, 35) <<
        (char)auxiliaryData(stream, 2, 36) << std::endl;
    std::cout << "    Intan Chip ID: " << auxiliaryData(stream, 2, 19) << std::endl;
    std::cout << "    Number of Amps: " << auxiliaryData(stream, 2, 20) << std::endl;
    std::cout << "    Unipolar/Bipolar Amps: ";
    switch (auxiliaryData(stream, 2, 21)) {
    case 0:
        std::cout << "bipolar";
        break;
    case 1:
        std::cout << "unipolar";
        break;
    default:
        std::cout << "UNKNOWN";
    }
    std::cout << std::endl;
    std::cout << "    Die Revision: " << auxiliaryData(stream, 2, 22) << std::endl;
    std::cout << "    Future Expansion Register: " << auxiliaryData(stream, 2, 23) << std::endl;

    std::cout << "  RAM contents:" << std::endl;
    std::cout << "    ADC reference BW:      " << ((auxiliaryData(stream,2,RamOffset + 0) & 0xc0) >> 6) << std::endl;
    std::cout << "    amp fast settle:       " << ((auxiliaryData(stream, 2, RamOffset + 0) & 0x20) >> 5) << std::endl;
    std::cout << "    amp Vref enable:       " << ((auxiliaryData(stream, 2, RamOffset + 0) & 0x10) >> 4) << std::endl;
    std::cout << "    ADC comparator bias:   " << ((auxiliaryData(stream, 2, RamOffset + 0) & 0x0c) >> 2) << std::endl;
    std::cout << "    ADC comparator select: " << ((auxiliaryData(stream, 2, RamOffset + 0) & 0x03) >> 0) << std::endl;
    std::cout << "    VDD sense enable:      " << ((auxiliaryData(stream, 2, RamOffset + 1) & 0x40) >> 6) << std::endl;
    std::cout << "    ADC buffer bias:       " << ((auxiliaryData(stream, 2, RamOffset + 1) & 0x3f) >> 0) << std::endl;
    std::cout << "    MUX bias:              " << ((auxiliaryData(stream, 2, RamOffset + 2) & 0x3f) >> 0) << std::endl;
    std::cout << "    MUX load:              " << ((auxiliaryData(stream, 2, RamOffset + 3) & 0xe0) >> 5) << std::endl;
    std::cout << "    tempS2, tempS1:        " << ((auxiliaryData(stream, 2, RamOffset + 3) & 0x10) >> 4) << "," <<
        ((auxiliaryData(stream, 2, RamOffset +30) & 0x08) >> 3) << std::endl;
    /*std::cout << "    tempen:                " << ((auxiliaryData[stream][2][RamOffset + 3] & 0x04) >> 2) << std::endl;
    std::cout << "    digout HiZ:            " << ((auxiliaryData[stream][2][RamOffset + 3] & 0x02) >> 1) << std::endl;
    std::cout << "    digout:                " << ((auxiliaryData[stream][2][RamOffset + 3] & 0x01) >> 0) << std::endl;
    std::cout << "    weak MISO:             " << ((auxiliaryData[stream][2][RamOffset + 4] & 0x80) >> 7) << std::endl;
    std::cout << "    twoscomp:              " << ((auxiliaryData[stream][2][RamOffset + 4] & 0x40) >> 6) << std::endl;
    std::cout << "    absmode:               " << ((auxiliaryData[stream][2][RamOffset + 4] & 0x20) >> 5) << std::endl;
    std::cout << "    DSPen:                 " << ((auxiliaryData[stream][2][RamOffset + 4] & 0x10) >> 4) << std::endl;
    std::cout << "    DSP cutoff freq:       " << ((auxiliaryData[stream][2][RamOffset + 4] & 0x0f) >> 0) << std::endl;
    std::cout << "    Zcheck DAC power:      " << ((auxiliaryData[stream][2][RamOffset + 5] & 0x40) >> 6) << std::endl;
    std::cout << "    Zcheck load:           " << ((auxiliaryData[stream][2][RamOffset + 5] & 0x20) >> 5) << std::endl;
    std::cout << "    Zcheck scale:          " << ((auxiliaryData[stream][2][RamOffset + 5] & 0x18) >> 3) << std::endl;
    std::cout << "    Zcheck conn all:       " << ((auxiliaryData[stream][2][RamOffset + 5] & 0x04) >> 2) << std::endl;
    std::cout << "    Zcheck sel pol:        " << ((auxiliaryData[stream][2][RamOffset + 5] & 0x02) >> 1) << std::endl;
    std::cout << "    Zcheck en:             " << ((auxiliaryData[stream][2][RamOffset + 5] & 0x01) >> 0) << std::endl;
    std::cout << "    Zcheck DAC:            " << ((auxiliaryData[stream][2][RamOffset + 6] & 0xff) >> 0) << std::endl;
    std::cout << "    Zcheck select:         " << ((auxiliaryData[stream][2][RamOffset + 7] & 0x3f) >> 0) << std::endl;
    std::cout << "    ADC aux1 en:           " << ((auxiliaryData[stream][2][RamOffset + 9] & 0x80) >> 7) << std::endl;
    std::cout << "    ADC aux2 en:           " << ((auxiliaryData[stream][2][RamOffset + 11] & 0x80) >> 7) << std::endl;
    std::cout << "    ADC aux3 en:           " << ((auxiliaryData[stream][2][RamOffset + 13] & 0x80) >> 7) << std::endl;
    std::cout << "    offchip RH1:           " << ((auxiliaryData[stream][2][RamOffset + 8] & 0x80) >> 7) << std::endl;
    std::cout << "    offchip RH2:           " << ((auxiliaryData[stream][2][RamOffset + 10] & 0x80) >> 7) << std::endl;
    std::cout << "    offchip RL:            " << ((auxiliaryData[stream][2][RamOffset + 12] & 0x80) >> 7) << std::endl;

    int rH1Dac1 = auxiliaryData[stream][2][RamOffset + 8] & 0x3f;
    int rH1Dac2 = auxiliaryData[stream][2][RamOffset + 9] & 0x1f;
    int rH2Dac1 = auxiliaryData[stream][2][RamOffset + 10] & 0x3f;
    int rH2Dac2 = auxiliaryData[stream][2][RamOffset + 11] & 0x1f;
    int rLDac1 = auxiliaryData[stream][2][RamOffset + 12] & 0x7f;
    int rLDac2 = auxiliaryData[stream][2][RamOffset + 13] & 0x3f;
    int rLDac3 = auxiliaryData[stream][2][RamOffset + 13] & 0x40 >> 6;

    double rH1 = 2630.0 + rH1Dac2 * 30800.0 + rH1Dac1 * 590.0;
    double rH2 = 8200.0 + rH2Dac2 * 38400.0 + rH2Dac1 * 730.0;
    double rL = 3300.0 + rLDac3 * 3000000.0 + rLDac2 * 15400.0 + rLDac1 * 190.0;

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "    RH1 DAC1, DAC2:        " << rH1Dac1 << " " << rH1Dac2 << " = " << (rH1 / 1000) <<
        " kOhm" << std::endl;
    std::cout << "    RH2 DAC1, DAC2:        " << rH2Dac1 << " " << rH2Dac2 << " = " << (rH2 / 1000) <<
        " kOhm" << std::endl;
    std::cout << "    RL DAC1, DAC2, DAC3:   " << rLDac1 << " " << rLDac2 << " " << rLDac3 << " = " <<
        (rL / 1000) << " kOhm" << std::endl;

    std::cout << "    amp power[31:0]:       " <<
        ((auxiliaryData[stream][2][RamOffset + 17] & 0x80) >> 7) <<
        ((auxiliaryData[stream][2][RamOffset + 17] & 0x40) >> 6) <<
        ((auxiliaryData[stream][2][RamOffset + 17] & 0x20) >> 5) <<
        ((auxiliaryData[stream][2][RamOffset + 17] & 0x10) >> 4) <<
        ((auxiliaryData[stream][2][RamOffset + 17] & 0x08) >> 3) <<
        ((auxiliaryData[stream][2][RamOffset + 17] & 0x04) >> 2) <<
        ((auxiliaryData[stream][2][RamOffset + 17] & 0x02) >> 1) <<
        ((auxiliaryData[stream][2][RamOffset + 17] & 0x01) >> 0) << " " <<
        ((auxiliaryData[stream][2][RamOffset + 16] & 0x80) >> 7) <<
        ((auxiliaryData[stream][2][RamOffset + 16] & 0x40) >> 6) <<
        ((auxiliaryData[stream][2][RamOffset + 16] & 0x20) >> 5) <<
        ((auxiliaryData[stream][2][RamOffset + 16] & 0x10) >> 4) <<
        ((auxiliaryData[stream][2][RamOffset + 16] & 0x08) >> 3) <<
        ((auxiliaryData[stream][2][RamOffset + 16] & 0x04) >> 2) <<
        ((auxiliaryData[stream][2][RamOffset + 16] & 0x02) >> 1) <<
        ((auxiliaryData[stream][2][RamOffset + 16] & 0x01) >> 0) << " " <<
        ((auxiliaryData[stream][2][RamOffset + 15] & 0x80) >> 7) <<
        ((auxiliaryData[stream][2][RamOffset + 15] & 0x40) >> 6) <<
        ((auxiliaryData[stream][2][RamOffset + 15] & 0x20) >> 5) <<
        ((auxiliaryData[stream][2][RamOffset + 15] & 0x10) >> 4) <<
        ((auxiliaryData[stream][2][RamOffset + 15] & 0x08) >> 3) <<
        ((auxiliaryData[stream][2][RamOffset + 15] & 0x04) >> 2) <<
        ((auxiliaryData[stream][2][RamOffset + 15] & 0x02) >> 1) <<
        ((auxiliaryData[stream][2][RamOffset + 15] & 0x01) >> 0) << " " <<
        ((auxiliaryData[stream][2][RamOffset + 14] & 0x80) >> 7) <<
        ((auxiliaryData[stream][2][RamOffset + 14] & 0x40) >> 6) <<
        ((auxiliaryData[stream][2][RamOffset + 14] & 0x20) >> 5) <<
        ((auxiliaryData[stream][2][RamOffset + 14] & 0x10) >> 4) <<
        ((auxiliaryData[stream][2][RamOffset + 14] & 0x08) >> 3) <<
        ((auxiliaryData[stream][2][RamOffset + 14] & 0x04) >> 2) <<
        ((auxiliaryData[stream][2][RamOffset + 14] & 0x02) >> 1) <<
        ((auxiliaryData[stream][2][RamOffset + 14] & 0x01) >> 0) << std::endl;

    std::cout << std::endl;

    int tempA = auxiliaryData[stream][1][12];
    int tempB = auxiliaryData[stream][1][20];
    int vddSample = auxiliaryData[stream][1][28];

    double tempUnitsC = ((double)(tempB - tempA)) / 98.9 - 273.15;
    double tempUnitsF = (9.0 / 5.0) * tempUnitsC + 32.0;

    double vddSense = 0.0000748 * ((double)vddSample);

    std::cout << std::setprecision(1);
    std::cout << "  Temperature sensor (only one reading): " << tempUnitsC << " C (" <<
        tempUnitsF << " F)" << std::endl;

    std::cout << std::setprecision(2);
    std::cout << "  Supply voltage sensor                : " << vddSense << " V" << std::endl;

    std::cout << std::setprecision(6);
    std::cout.unsetf(std::ios::floatfield);
    std::cout << std::endl;*/
}

// Write contents of data block to a binary output stream (saveOut) in little endian format.
void RHXDataBlock::write(std::ofstream& saveOut, int numDataStreams) const
{
    int t, channel, stream, i;

    for (t = 0; t < samplesPerDataBlock(); ++t) {
        writeWordLittleEndian(saveOut, timeStampInternal[t]);
        for (channel = 0; channel < 32; ++channel) {
            for (stream = 0; stream < numDataStreams; ++stream) {
                writeWordLittleEndian(saveOut, amplifierData(stream, channel, t));
            }
        }
        for (channel = 0; channel < 3; ++channel) {
            for (stream = 0; stream < numDataStreams; ++stream) {
                writeWordLittleEndian(saveOut, auxiliaryData(stream, channel, t));
            }
        }
        for (i = 0; i < 8; ++i) {
            writeWordLittleEndian(saveOut, boardAdcData(i, t));
        }
        writeWordLittleEndian(saveOut, ttlInInternal[t]);
        writeWordLittleEndian(saveOut, ttlOutInternal[t]);
    }
}


void RHXDataBlock::fillFromUsbBuffer(uint8_t* usbBuffer, int blockIndex)
{
    int ampIndex = 0;
    int dcAmpIndex = 0;
    int complianceIndex = 0;
    int stimOnIndex = 0;
    int stimPolIndex = 0;
    int ampSettleIndex = 0;
    int chargeRecovIndex = 0;
    int dacIndex = 0;
    int adcIndex = 0;
    int index = blockIndex * BytesPerWord * dataBlockSizeInWords();
    int highWord, index1, index2;

    for (int t = 0; t < samplesPerDataBlock(); ++t) {
        
        //std::cout << "Sample number: " << t << std::endl;
        //std::cout << "Index: " << index << std::endl;

        if (!checkUsbHeader(usbBuffer, index)) {
            std::cerr << "rhxdatablock Error in RHXDataBlock::fillFromUsbBuffer: Incorrect header.\n";
        }
        index += 8;
        timeStampInternal[t] = convertUsbTimeStamp(usbBuffer, index);
        index += 4;

        // Read auxiliary command results 0-2 (for stim/record controller, read auxiliary command results 1-3)
        index1 = t * numDataStreams * numAuxChannels();
        for (int channel = numAuxChannels() - 3; channel < numAuxChannels(); ++channel) {
            index2 = channel * numDataStreams;
            for (int stream = 0; stream < numDataStreams; ++stream) {
                auxiliaryDataInternal[index1 + index2 + stream] = convertUsbWord(usbBuffer, index);
                index += 2;
                if (type == ControllerStimRecordUSB2) {
                    if (channel == 2) {
                        highWord = convertUsbWord(usbBuffer,index); // The top 16 bits will be either all 1's (results of a WRITE command)
                                                                    // or all 0's (results of a READ command)
                        if (highWord == 0) {  // update compliance limit only if a 'read' command was executed, denoting a read from Register 40
                            for (int ch = 0; ch < channelsPerStream(); ++ch) {
                                complianceLimitInternal[complianceIndex++] = (auxiliaryDataInternal[index1 + (2 * numDataStreams) + stream] & (1 << ch)) ? 1 : 0;
                            }
                        } else {
                            for (int ch = 0; ch < channelsPerStream(); ++ch) {
                                complianceLimitInternal[complianceIndex++] = 0;  // if Register 40 was not read, assume no compliance limit violations
                            }
                        }
                    }
                    index += 2;
                }
            }
        }

        // Read amplifier channels.
        for (int channel = 0; channel < channelsPerStream(); ++channel) {
            for (int stream = 0; stream < numDataStreams; ++stream) {
                if (type == ControllerStimRecordUSB2) {
                    dcAmplifierDataInternal[dcAmpIndex++] = convertUsbWord(usbBuffer, index);
                    index += 2;
                }
                amplifierDataInternal[ampIndex++] = convertUsbWord(usbBuffer, index);
                index += 2;
            }
        }

        if (type == ControllerStimRecordUSB2) {
            // Read auxiliary command 0 results (see above for auxiliary command 1-3 results).
            for (int stream = 0; stream < numDataStreams; ++stream) {
                auxiliaryDataInternal[index1 + stream] = convertUsbWord(usbBuffer, index);
                index += 2;
                index += 2; // We are skipping the top 16 bits here since they will typically be either all 1's (results of a WRITE command)
                            // or all 0's (results of a READ command).
            }

            // Read stimulation control parameters.
            for (int stream = 0; stream < numDataStreams; ++stream) {
                stimOnInternal[stimOnIndex++] = convertUsbWord(usbBuffer, index);
                index += 2;
            }

            for (int stream = 0; stream < numDataStreams; ++stream) {
                stimPolInternal[stimPolIndex++] = convertUsbWord(usbBuffer, index);
                index += 2;
            }

            for (int stream = 0; stream < numDataStreams; ++stream) {
                ampSettleInternal[ampSettleIndex++] = convertUsbWord(usbBuffer, index);
                index += 2;
            }

            for (int stream = 0; stream < numDataStreams; ++stream) {
                chargeRecovInternal[chargeRecovIndex++] = convertUsbWord(usbBuffer, index);
                index += 2;
            }

            // Read from DACs.
            for (int i = 0; i < 8; ++i) {
                boardDacDataInternal[dacIndex++] = convertUsbWord(usbBuffer, index);
                index += 2;
            }
        }

        // Skip filler words in each data stream.
        if (type == ControllerRecordUSB2) { //sumedh condition changed
            index += 2 * numDataStreams;
        } else if (type == ControllerRecordUSB3) { //sumedh condition changed
            index += 2 * (numDataStreams % 4);
        }

        // Read from ADCs.
        for (int i = 0; i < 8; ++i) {
            boardAdcDataInternal[adcIndex++] = convertUsbWord(usbBuffer, index);
            index += 2;
        }

        // Read TTL input and output values.
        ttlInInternal[t] = convertUsbWord(usbBuffer, index);
        index += 2;

        ttlOutInternal[t] = convertUsbWord(usbBuffer, index);
        index += 2;

    }
}
