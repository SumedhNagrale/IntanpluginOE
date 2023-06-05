/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2020 Open Ephys

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


#ifndef __DEVICETHREAD_H_2C4CBD67__
#define __DEVICETHREAD_H_2C4CBD67__

#include <DataThreadHeaders.h>

#include <stdio.h>
#include <string.h>
#include <array>
#include <atomic>

#define MAX_NUM_DATA_STREAMS 32
#define MAX_NUM_CHANNELS MAX_NUM_DATA_STREAMS * 35

#define CHIP_ID_RHD2132  1
#define CHIP_ID_RHD2216  2
#define CHIP_ID_RHD2164  4
#define CHIP_ID_RHD2164_B  1000
#define REGISTER_59_MISO_A  53
#define REGISTER_59_MISO_B  58
#define RHD2132_16CH_OFFSET 8

#include "../rhx-api/Abstract/abstractrhxcontroller.h"
#include "../rhx-api/Hardware/rhxdatablock.h"
#include "../rhx-api/Hardware/okFrontPanelDLL.h"

namespace RhythmNode
{

	class Headstage;
	class ImpedanceMeter;
	class USBThread;

	enum BoardType
	{
		ACQUISITION_BOARD,
		INTAN_RHD_USB,
        ONI_USB,
		RHD_RECORDING_CONTROLLER,
        RHS_STIM_RECORDING_CONTROLLER
	};

	enum UsbVersion
	{
		USB2 = 2,
		USB3 = 3
	};

	enum ChannelNamingScheme
	{
		GLOBAL_INDEX = 1,
		STREAM_INDEX = 2
	};

	struct Impedances
	{
		Array<int> streams;
		Array<int> channels;
		Array<float> magnitudes;
		Array<float> phases;
		bool valid = false;
	};

	/**
		Communicates with a device running Rhythm or ONI firmware

		@see DataThread, SourceNode
	*/
	class DeviceThread : public DataThread
	{
		friend class ImpedanceMeter;

	public:
        
        /** DataThread factory */
        static DataThread* createDataThread(SourceNode* sn);

        /** Stores the board type (ACQUISITION_BOARD, INTAN_RHD_USB, etc.) */
        static BoardType boardType;
        
		/** Constructor; must specify the type of board used */
		DeviceThread(SourceNode* sn, BoardType boardType);

		/** Destructor */
		~DeviceThread();

		/** Creates the UI for this plugin */
		std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;

		/** Fills the DataBuffer with incoming data */
		bool updateBuffer() override;

		/** Initializes data transfer*/
		bool startAcquisition() override;

		/** Stops data transfer */
		bool stopAcquisition() override;
        
        /** Informs the DataThread about whether to expect saved settings to be loaded*/
        void initialize(bool signalChainIsLoading) override;

		/* Passes the processor's info objects to DataThread, to allow them to be configured */
		void updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
			OwnedArray<EventChannel>* eventChannels,
			OwnedArray<SpikeChannel>* spikeChannels,
			OwnedArray<DataStream>* sourceStreams,
			OwnedArray<DeviceInfo>* devices,
			OwnedArray<ConfigurationObject>* configurationObjects) override;

		/** Updates the measured impedance values for each channel*/
		void impedanceMeasurementFinished();

		/** Returns an array of connected headstages*/
		Array<const Headstage*> getConnectedHeadstages();

		/** Sets the method for determining channel names*/
		void setNamingScheme(ChannelNamingScheme scheme);

		/** Gets the method for determining channel names*/
		ChannelNamingScheme getNamingScheme();

		/** Allow the thread to respond to messages sent by other plugins */
		void handleBroadcastMessage(String msg) override;

        /** Sets the channel count for a particular headstage */
		void setNumChannels(int hsNum, int nChannels);

        /** Returns the total number of channels for this device */
		int getNumChannels();

        /** Returns the total number of channels for a given type (HEADSTAGE, AUX, ADC) */
		int getNumDataOutputs(ContinuousChannel::Type type);

        /** Returns true if a particular headstage is enabled*/
		bool isHeadstageEnabled(int hsNum) const;

        /** Returns the channel count for a particular headstage */
		int getChannelsInHeadstage(int hsNum) const;

		/** Returns number of 16-bit words in a USB data block*/
		unsigned int calculateDataBlockSizeInWords(int numDataStreams, bool usb3, int nSamples);

		/* Gets the absolute channel index from the headstage channel index*/
		int getChannelFromHeadstage(int hs, int ch);

		/*Gets the headstage relative channel index from the absolute channel index*/
		int getHeadstageChannel(int& hs, int ch) const;

		// for communication with SourceNode processors:
		bool foundInputSource() override;

        /** Scans all available ports for active headstages */
		void scanPorts();

        /** Saves measured impedances to a file */
		void saveImpedances(File& file);

		String getChannelName(int ch) const;

		float getAdcBitVolts(int channelNum) const;

		void setSampleRate(int index, bool temporary = false);

		double setUpperBandwidth(double upper); // set desired BW, returns actual BW
		double setLowerBandwidth(double lower);

		double setDspCutoffFreq(double freq);
		double getDspCutoffFreq() const;

		void setDSPOffset(bool state);

		int setNoiseSlicerLevel(int level);
		void setFastTTLSettle(bool state, int channel);
		void setTTLoutputMode(bool state);
		void setDAChpf(float cutoff, bool enabled);

		void enableAuxs(bool);
		void enableAdcs(bool);

		int TTL_OUTPUT_STATE[16];

		bool isAuxEnabled();
		bool isAcquisitionActive() const;

		Array<int> getDACchannels() const;

		void setDACchannel(int dacOutput, int channel);
		void setDACthreshold(int dacOutput, float threshold);

		int getHeadstageChannels(int hsNum) const;
		int getActiveChannelsInHeadstage(int hsNum) const;

		void runImpedanceTest(double frequency);

		void enableBoardLeds(bool enable);

		int setClockDivider(int divide_ratio);

		void setAdcRange(int adcChannel, short rangeType);

		short getAdcRange(int adcChannel) const;
		void updateChipCommandLists(bool updateStimParams);
        /**
            A timer that stores the on/off times of digital output events
         */
		class DigitalOutputTimer : public Timer
		{
		public:

			/** Constructor */
			DigitalOutputTimer(DeviceThread*, int tllLine, int eventDurationMs);

			/** Destructor*/
			~DigitalOutputTimer() { }

			/** Sends signal to turn off event channel*/
			void timerCallback();

		private:
			DeviceThread* board;

			int tllOutputLine;
		};

		struct DigitalOutputCommand {
			int ttlLine;
			bool state;
		};

		void addDigitalOutputCommand(DigitalOutputTimer* timerToDelete,
			int ttlLine,
			bool state);

		/** List of enabled streams */
		Array<int> enabledStreams;

	private:
        
        /** Pointer to the device object */
        std::unique_ptr<AbstractRHXController> device;
        
        /** Array of headstage objects*/
        OwnedArray<Headstage> headstages;

        /** Queue for digital output commands */
		std::queue<DigitalOutputCommand> digitalOutputCommands;

        /** Array of digital output times */
		OwnedArray<DigitalOutputTimer> digitalOutputTimers;
        
        /** Thread for impedance measurement */
        ScopedPointer<ImpedanceMeter> impedanceThread;

		/** Thread for data acquisition */
		ScopedPointer<USBThread> usbThread;

		bool enableHeadstage(int hsNum, bool enabled, int nStr = 1, int strChans = 32);

		void setCableLength(int hsNum, float length);

		std::unique_ptr<RHXDataBlock> dataBlock;
		UsbVersion usbVersion;

		std::unique_ptr<okCFrontPanel> frontPanelLib;

		/** True if device is available*/
		bool deviceFound;

		/** True if data is streaming*/
		bool isTransmitting;

		/** True if change in settings is needed during acquisition*/
		bool updateSettingsDuringAcquisition;

		int* dacChannels, *dacStream;
		float* dacThresholds;
		bool* dacChannelsToUpdate;
		Array<int> chipId;

		Array<int> numChannelsPerDataStream;

		ChannelNamingScheme channelNamingScheme;

		/** ADC info */
		std::array<std::atomic_short, 8> adcRangeSettings;
		Array<float> adcBitVolts;
		StringArray adcChannelNames;
		StringArray ttlLineNames;

		/** Impedance data*/
		Impedances impedances;

        /** Stores names of channels */
		StringArray channelNames;

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

		} settings;

		unsigned int blockSize;

		//uint8_t bufferPtr[500000];

		/** Data buffers*/
		float thisSample[MAX_NUM_CHANNELS];

		float auxBuffer[MAX_NUM_CHANNELS]; // aux inputs are only sampled every 4th sample, so use this to buffer the
										   // samples so they can be handles just like the regular neural channels later

		float auxSamples[MAX_NUM_DATA_STREAMS][3];

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceThread);
	};

}
#endif  // __DEVICETHREAD_H_2C4CBD67__
