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

#ifndef AbstractDevice_hpp
#define AbstractDevice_hpp

#include <stdio.h>
#include <vector>

#include "Headstage.h"

enum DeviceParameter
{
    SAMPLE_RATE,
    DSP_CUTOFF_FREQ,
    ENABLE_LEDS,
    CLOCK_DIVIDE_FACTOR,
    ENABLE_DAC,
    
    
};

/** Cable length settings */
struct CableLength
{
    float portA = 0.914f;
    float portB = 0.914f;
    float portC = 0.914f;
    float portD = 0.914f;
    float portE = 0.914f;
    float portF = 0.914f;
    float portG = 0.914f;
    float portH = 0.914f;
};

/** Dsp settings*/
struct Dsp
{
    bool enabled = true;
    double cutoffFreq = 0.5;
    double upperBandwidth = 7500.0f;
    double lowerBandwidth = 1.0f;
};

/** struct containing board settings*/
struct Settings
{
    bool acquireAux = false;
    bool acquireAdc = false;

    bool fastSettleEnabled = false;
    bool fastTTLSettleEnabled = false;
    int fastSettleTTLChannel = -1;
    bool ttlMode = false;

    Dsp dsp;

    int noiseSlicerLevel;

    bool desiredDAChpfState;
    double desiredDAChpf;
    float boardSampleRate = 30000.f;
    int savedSampleRateIndex = 16;

    CableLength cableLength;

    int audioOutputL = -1;
    int audioOutputR = -1;
    bool ledsEnabled = true;
    bool newScan = true;
    int numberingScheme = 1;
    uint16 clockDivideFactor;
    
    int samplesPerDataBlock;

} settings;

enum ErrorCode
{
    SUCCESS = 0,
    OPAL_KELLY_LIBRARY_NOT_FOUND,
    NO_DEVICE_FOUND,
    BITFILE_NOT_FOUND,
    FAILED_TO_START_ACQUISITION,
};

struct DataBlock {
    std::vector<float> samples;
    std::vector<int64> sampleNumbers;
    std::vector<double> timestamps;
    std::vector<uint64> ttlEventWords;
};

struct Impedances
{
    std::vector<int> streams;
    std::vector<int> channels;
    std::vector<float> magnitudes;
    std::vector<float> phases;
    bool valid = false;
};

/**

    Abstract interface for communicating with a
     data acquisition device.
 
 */
class AbstractDevice
{
public:
    
    /** Constructor */
    AbstractDevice() { }
    
    /** Destructor */
    ~AbstractDevice() { }
    
    /** Opens device, returns error code */
    virtual ErrorCode open() = 0;
    
    /** Closes device, returns error code */
    virtual ErrorCode close() = 0;
    
    /** Initializes device, returns error code*/
    virtual ErrorCode initialize() = 0;
    
    /** Starts streaming data*/
    virtual ErrorCode startAcquisition() = 0;
    
    /** Stops streaming data  */
    virtual ErrorCode stopAcquisition() = 0;
    
    /** Sets a parameter */
    virtual ErrorCode setParameter(DeviceParameter parameter, double value, int index = -1) = 0;
    
    /** Gets available sample rates */
    virtual std::vector<float> getAvailableSampleRates() = 0;
    
    /** Updates the headstage array with connected headstage info */
    virtual ErrorCode detectHeadstages(std::vector<RhythmNode::Headstage>&) = 0;
    
    /** Reads in a block of data*/
    virtual ErrorCode fillDataBlock(DataBlock*) = 0;
    
    /** Sets state of digital output lines */
    virtual ErrorCode setDigitalOut(int* states, int numLines = 8) = 0;
    
    /** Conducts impedance measurement (called from a separate thread)*/
    virtual ErrorCode measureImpedances(Impedances&);
   
    /** Device settings struct */
    Settings settings;

protected:
    
    ErrorCode errorCode;
    
};

#endif /* AbstractDevice_hpp */
