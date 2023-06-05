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


#ifndef OniDevice_hpp
#define OniDevice_hpp

#include <stdio.h>

#include "oni.h"

#include "../rhx-api/Abstract/abstractrhxcontroller.h"

/**

    Class for communicating with an ONI Device
 
 */

class OniDevice : public AbstractRHXController
{

public:

    /** Constructor */
    OniDevice();

    /** Destructor */
    ~OniDevice();

    /** Returns true if device generates synthetic data */
    bool isSynthetic() const override { return false; }

    /** Opens the connection to the board */
    int open(const std::string& boardSerialNumber, const char* libraryFilePath) override;

    /** Upload the configuration file (bitfile) to the FPGA. Return true if successful.*/
    bool uploadFPGABitfile(const std::string& filename) override;

    /** Reset FPGA. This clears all auxiliary command RAM banks, clears the USB FIFO, 
        and resets the per-channel samplingrate to 30.0 kS/s/ch. */
    void resetBoard() override;

    /** Initiate SPI data acquisition. */
    void run() override;

    /** Is the FPGA currently running?*/
    bool isRunning() override;

    /**  Flush all remaining data out of the FIFO.  
         This function should only be called when SPI data acquisition has been stopped. */
    void flush() override;

    /** Low-level FPGA reset.  Call when closing application to make sure everything has stopped.*/
    void resetFpga() override;

    /** Read one ONI data frame */
    int readFrame(oni_frame_t** frame);

    /** Read data block from the USB interface, if one is available. Return true if data block was available.*/
    bool readDataBlock(RHXDataBlock* dataBlock) override;

    /** Read a certain number of USB data blocks, if the specified number is available, 
        and append them to queue. Return true if data blocks were available. */
    bool readDataBlocks(int numBlocks, std::deque<RHXDataBlock*>& dataQueue) override;

    /** Read a certain number of USB data blocks, if the specified number is available, 
        and write the raw bytes to a buffer. Return total number of bytes read.
        
        NOT IMPLEMENTED -- use readFrame() instead */
    long readDataBlocksRaw(int numBlocks, uint8_t* buffer) override { return 0;  }

    /** Set the FPGA to run continuously once started (if continuousMode == true) or 
        to run until maxTimeStep is reached (if continuousMode == false).*/
    void setContinuousRunMode(bool continuousMode) override;

    /** Set maxTimeStep for cases where continuousMode == false.*/
    void setMaxTimeStep(unsigned int maxTimeStep) override;

    /** Set the delay for sampling the MISO line on a particular SPI port(PortA - PortH), 
        in integer clock steps, where each clock step is 1/2800 of a per-channel sampling period.  
        Note: Cable delay must be updated after sampleRate is changed, since cable delay calculations 
        are based on the clock frequency! */
    void setCableDelay(BoardPort port, int delay) override;

    /* Turn on or off DSP settle function in the FPGA. (Only executes when CONVERT commands are sent.) */
    void setDspSettle(bool enabled) override;

    /** Assign a particular data source (e.g., PortA1, PortA2, PortB1,...) to one of the available USB data streams (0-7).*/
    void setDataSource(int stream, BoardDataSource dataSource) override;

    /** Set the 16 bits of the digital TTL output lines on the FPGA high or low according to integer array.*/
    void setTtlOut(const int* ttlOutArray) override;

    /** Set manual value for DACs.*/
    void setDacManual(int value) override;

    /** Turn LEDs on or off(Open Ephys boards only) */
    void enableLeds(bool ledsOn) override;

    /** Set output BNC clock divide factor (Open Ephys boards only) */
    void setClockDivider(int divide_factor) override;
    
    /** Set the eight red LEDs on the Opal Kelly XEM6x10 board according to integer array.
        Not used with Open Ephys boards*/
    void setLedDisplay(const int* ledArray) override { }

    /** Set the eight red LEDs on the front panel SPI ports according to integer array. 
        Not used with ControllerRecordUSB2 or Open Ephys boards.*/
    void setSpiLedDisplay(const int* ledArray) override { }

    /** Set the gain level of all eight DAC channels to 2^gain (gain = 0-7). */
    void setDacGain(int gain) override;

    /** Suppress the noise on DAC channels 0 and 1 (the audio channels) between +16*noiseSuppress
        and -16*noiseSuppress LSBs (noiseSuppress = 0-127).*/
    void setAudioNoiseSuppress(int noiseSuppress) override;

    /** Select which of the TTL inputs 0 - 15 is used to perform a hardware 'fast settle' 
        (blanking) of the amplifiers if external triggering of fast settling is enabled.
        Not used with ControllerStimRecordUSB2 */
    void setExternalFastSettleChannel(int channel) override;

    /** Select which of the TTL inputs 0-15 is used to control the auxiliary digital 
        output pin of the chips connected to a particular SPI port, if external control of auxout is enabled.
        Not used with ControllerStimRecordUSB2 */
    void setExternalDigOutChannel(BoardPort port, int channel) override; 

    /** Set cutoff frequency (in Hz) for optional FPGA-implemented digital high-pass filters 
        associated with DAC outputs on USB interface board.  These one-pole filters can be 
        used to record wideband neural data while viewing only spikes without LFPs on the DAC outputs, 
        for example. This is useful when using the low-latency FPGA thresholds to detect spikes and produce 
        digital pulses on the TTL outputs, for example.*/
    void setDacHighpassFilter(double cutoff) override;

    /** Set thresholds for DAC channels; threshold output signals appear on TTL outputs 0 - 7.
        The parameter 'threshold' corresponds to the RHD/RHS chip ADC output value, and must fall 
        in the range of 0 to 65535, where the 'zero' level is 32768.  If trigPolarity is true, 
        voltages equaling or rising above the threshold produce a high TTL output. If trigPolarity 
        is false, voltages equaling or falling below the threshold produce a high TTL output. */
    void setDacThreshold(int dacChannel, int threshold, bool trigPolarity) override;

    /** Set the TTL output mode of the board. mode = 0: All 16 TTL outputs are under manual control mode = 1:
        Top 8 TTL outputs are under manual control; Bottom 8 TTL outputs are outputs of DAC comparators.
        Not used with ControllerStimRecordUSB2. */
    void setTtlMode(int mode) override;

    /** Select an amplifier channel from a particular data stream to be subtracted from all DAC signals.
        Not used with ControllerRecordUSB2. */
    void setDacRerefSource(int stream, int channel) override;  

    /** Set the given extra states. 
        Only used with ControllerStimRecordUSB2. */
    void setExtraStates(unsigned int extraStates) override { }

    /** Turn on or off automatic stimulation command mode in the FPGA.
        Only used with ControllerStimRecordUSB2. */
    void setStimCmdMode(bool enabled) override { }

    /** Set the voltage threshold to be used for digital triggers on Analog In ports.
        Only used with ControllerStimRecordUSB2. */
    void setAnalogInTriggerThreshold(double voltageThreshold) override { }

    /** Set state of manual stimulation trigger 0-7 (e.g., from keypresses). 
        Only used with ControllerStimRecordUSB2. */
    void setManualStimTrigger(int trigger, bool triggerOn) override { }

    /** The first four boolean parameters determine if global settling should be applied to particular SPI ports A-D.  
        If global settling is enabled, the amp settle function will be applied to ALL channels on a headstage when any one
        channel asserts amp settle. If the last boolean parameter is set true, global settling will be applied across all
        headstages: if any one channel asserts amp settle, then amp settle will be asserted on all channels, across all connected
        headstages.
        Only used with ControllerStimRecordUSB2. */
    void setGlobalSettlePolicy(bool settleWholeHeadstageA, 
                               bool settleWholeHeadstageB, 
                               bool settleWholeHeadstageC, 
                               bool settleWholeHeadstageD, 
                               bool settleAllHeadstages) override { }

    /** Set the function of Digital Out ports 1 - 8.
        true = Digital Out port controlled by DAC threshold-based spike detector ... false = Digital Out port controlled by digital
        sequencer.  Note: Digital Out ports 9-16 are always controlled by a digital sequencer.
        Only used with ControllerStimRecordUSB2. */
    void setTtlOutMode(bool mode1, 
                       bool mode2, 
                       bool mode3, 
                       bool mode4, 
                       bool mode5, 
                       bool mode6, 
                       bool mode7, 
                       bool mode8) override { }

    /** Select amp settle mode for all connected chips: useFastSettle false = amplifier low frequency cutoff select
        (recommended mode) ... useFastSettle true = amplifier fast settle (legacy mode from RHD2000 series chips)
        Only used with ControllerStimRecordUSB2. */
    void setAmpSettleMode(bool useFastSettle) override { }

    /** Select charge recovery mode for all connected chips: useSwitch false = current-limited charge recovery drivers ... 
        useSwitch true = charge recovery switch.
        Only used with ControllerStimRecordUSB2. */
    void setChargeRecoveryMode(bool useSwitch) override { }

    /** Set the per-channel sampling rate of the RHD/RHS chips connected to the FPGA. */
    bool setSampleRate(AmplifierSampleRate newSampleRate) override;

    /** Enable or disable one of the 32 available USB data streams (0-31). */
    void enableDataStream(int stream, bool enabled) override;

    /** Enable or disable DAC channel (0-7).*/
    void enableDac(int dacChannel, bool enabled) override;

    /** Enable external triggering of RHD amplifier hardware 'fast settle' function (blanking).
        If external triggering is enabled, the fast settling of amplifiers on all connected chips will be controlled in real time  
        via one of the 16 TTL inputs.
        Not used with ControllerStimRecordUSB2. */
    void enableExternalFastSettle(bool enable) override;

    /** Enable external control of RHD2000 auxiliary digital output pin(auxout).
        If external control is enabled, the digital output of all chips connected to a selected SPI port will be controlled in
         real time via one of the 16 TTL inputs. 
         Not used with ControllerStimRecordUSB2. */
    void enableExternalDigOut(BoardPort port, bool enable) override;

    /** Enable optional FPGA-implemented digital high-pass filters associated with DAC outputs on USB interface board.
        These one-pole filters can be used to record wideband neural data while viewing only spikes without LFPs on the
        DAC outputs, for example.  This is useful when using the low-latency FPGA thresholds to detect spikes and produce
        digital pulses on the TTL outputs, for example.*/
    void enableDacHighpassFilter(bool enable) override;

    /** Enable DAC rereferencing, where a selected amplifier channel is subtracted from all DACs in real time.*/
    void enableDacReref(bool enabled) override;

    /**  Enable DC amplifier conversion.
         Only used with ControllerStimRecordUSB2. */
    void enableDcAmpConvert(bool enable) override { }

    /** Enable auxiliary commands slots 0-3 on all data streams (0-7).  This disables automatic stimulation control on all
        data streams.
        Only used with ControllerStimRecordUSB2. */
    void enableAuxCommandsOnAllStreams() override { }

    /** Enable auxiliary commands slots 0 - 3 on one selected data stream, and disable auxiliary command slots on all other
        data streams.  This disables automatic stimulation control on the selected stream and enables automatic stimulation control
        on all other streams. 
        Only used with ControllerStimRecordUSB2. */
    void enableAuxCommandsOnOneStream(int stream) override { }

    /** Assign a particular data stream (0-31) to a DAC channel (0-7). 
        Setting stream to 32 selects DacManual value. */
    void selectDacDataStream(int dacChannel, int stream) override;

    /** Assign a particular amplifier channel (0-31) to a DAC channel (0-7).*/
    void selectDacDataChannel(int dacChannel, int dataChannel) override;

    /** Specify a command sequence length (endIndex = 0-1023) and command loop index (0-1023) 
        for a particular auxiliary command slot (AuxCmd1, AuxCmd2, or AuxCmd3).*/
    void selectAuxCommandLength(AuxCmdSlot auxCommandSlot, int loopIndex, int endIndex) override;

    /** Select an auxiliary command slot (AuxCmd1, AuxCmd2, or AuxCmd3) and bank (0-15) for a particular SPI port
        (PortA - PortH) on the FPGA.*/
    void selectAuxCommandBank(BoardPort port, AuxCmdSlot auxCommandSlot, int bank) override;

    /** Return 4-bit "board mode" input */
    int getBoardMode() override;

    /** Return number of SPI ports and if I/O expander board is present.*/
    int getNumSPIPorts(bool& expanderBoardDetected) override;

    /** Set all 16 bits of the digital TTL output lines on the FPGA to zero.  
        Not used with ControllerStimRecordUSB2.*/
    void clearTtlOut() override;

    /** Reset stimulation sequencer units.  This is typically called when data acquisition is stopped. 
        It is possible that a stimulation sequencer could be in the middle of playing out a long pulse train
        (e.g., 100 stimulation pulses).  If this function is not called, the pulse train will resume after data acquisition
        is restarted. 
        Only used with ControllerStimRecordUSB2.
    */
    void resetSequencers() override { }

    /**  Set a particular stimulation control register.
         Only used with ControllerStimRecordUSB2.
     */
    void programStimReg(int stream, int channel, StimRegister reg, int value) override { }

    /** 
        Upload an auxiliary command list to a particular command slot and RAM bank (0-15) on the FPGA.
    */
    void uploadCommandList(const std::vector<unsigned int>& commandList, AuxCmdSlot auxCommandSlot, int bank = 0) override;

private:

    /** Disable copy operator (declaration only) */
    OniDevice(const OniDevice&);

    /** Dsiable assignment operator (declaration only) */
    OniDevice& operator=(const OniDevice&);

    /** Return the number of 16-bit words in the USB FIFO.  The user should never attempt to 
        read more data than the FIFO currently contains, as it is not protected against underflow.*/
    unsigned int numWordsInFifo() override;

    /** Is variable-frequency clock DCM programming done? */
    bool isDcmProgDone() const override;

    /** Is variable-frequency clock PLL locked?*/
    bool isDataClockLocked() const override;

    /** Force all data streams off, used in FPGA initialization. */
    void forceAllDataStreamsOff() override;

    /** Print a command list to the console in readable form. */
    void printCommandList(const std::vector<unsigned int>& commandList) const;

    enum Rhythm_Mode
    {
        SPI_RUN_CONTINUOUS = 1,
        DSP_SETTLE = 2,
        TTL_OUT_MODE = 3,
        LED_ENABLE = 4
    };
    
    enum Rhythm_Registers
    {
        ENABLE = 0,
        MODE,
        MAX_TIMESTEP,
        CABLE_DELAY,
        AUXCMD_BANK_1,
        AUXCMD_BANK_2,
        AUXCMD_BANK_3,
        MAX_AUXCMD_INDEX_1,
        MAX_AUXCMD_INDEX_2,
        MAX_AUXCMD_INDEX_3,
        LOOP_AUXCMD_INDEX_1,
        LOOP_AUXCMD_INDEX_2,
        LOOP_AUXCMD_INDEX_3,
        DATA_STREAM_1_8_SEL,
        DATA_STREAM_9_16_SEL,
        DATA_STREAM_EN,
        EXTERNAL_FAST_SETTLE,
        EXTERNAL_DIGOUT_A,
        EXTERNAL_DIGOUT_B,
        EXTERNAL_DIGOUT_C,
        EXTERNAL_DIGOUT_D,
        SYNC_CLKOUT_DIVIDE,
        DAC_CTL,
        DAC_SEL_1,
        DAC_SEL_2,
        DAC_SEL_3,
        DAC_SEL_4,
        DAC_SEL_5,
        DAC_SEL_6,
        DAC_SEL_7,
        DAC_SEL_8,
        DAC_THRESH_1,
        DAC_THRESH_2,
        DAC_THRESH_3,
        DAC_THRESH_4,
        DAC_THRESH_5,
        DAC_THRESH_6,
        DAC_THRESH_7,
        DAC_THRESH_8,
        HPF,
        SPI_RUNNING
    };
    
    /** ONI device indices*/
    const oni_dev_idx_t DEVICE_RHYTHM = 0x0101;
    const oni_dev_idx_t DEVICE_TTL = 0x0102;
    const oni_dev_idx_t DEVICE_DAC = 0x0103;
   
    /** The ONI context object */
    oni_ctx ctx;

    /** USB block size*/
    const oni_size_t usbReadBlockSize = 24 * 1024;

    /** Helper method for setting the state of one bit*/
    void oni_write_reg_bit(const oni_ctx ctx,
        oni_dev_idx_t dev_idx,
        oni_reg_addr_t addr,
        int bit_index,
        bool state);

    /** Helper method for writing to an address with a mask*/
    int oni_write_reg_mask(const oni_ctx ctx,
        oni_dev_idx_t dev_idx,
        oni_reg_addr_t addr,
        oni_reg_val_t value,
        unsigned int mask);

};

#endif /* OniDevice_hpp */
