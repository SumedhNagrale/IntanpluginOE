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

#ifdef _WIN32
#define NOMINMAX
#endif

#include "../Threads/DeviceThread.h"
#include "../Processing/InputSource/DeviceEditor.h"

#include "../Processing/SourceSupport/ImpedanceMeter.h"
#include "../Processing/SourceSupport//Headstage.h"
#include "../Threads/USBThread.h"

#include "../rhx-api/Hardware/rhxglobals.h"
#include "../rhx-api/Hardware/rhxcontroller.h"
#include "../rhythm-api/RhythmDevice.h"
#include "../oni-api/OniDevice.h"


#if defined(_WIN32)
#define okLIB_NAME "okFrontPanel.dll"
#define okLIB_EXTENSION "*.dll"
#elif defined(__APPLE__)
#define okLIB_NAME "libokFrontPanel.dylib"
#define okLIB_EXTENSION "*.dylib"
#elif defined(__linux__)
#define okLIB_NAME "./libokFrontPanel.so"
#define okLIB_EXTENSION "*.so"
#endif

using namespace RhythmNode;

BoardType DeviceThread::boardType = ACQUISITION_BOARD; // initialize static member

DataThread* DeviceThread::createDataThread(SourceNode *sn)
{
    return new DeviceThread(sn, boardType);
}

DeviceThread::DeviceThread(SourceNode* sn, BoardType boardType_) : DataThread(sn),
    deviceFound(false),
    isTransmitting(false),
    channelNamingScheme(GLOBAL_INDEX),
    updateSettingsDuringAcquisition(false)
{
    // board type and impendence meter
    boardType = boardType_;
    impedanceThread = new ImpedanceMeter(this);
    // What is the adc range here ? there are simply 8 channels what about others? can there be more than 8 
    // this is hardcoded to 8 in the declaration see header file
    for (int i = 0; i < 8; i++)
        adcRangeSettings[i] = 0;
    // What is the maximum number of headstages, maybe this is fine when we have 16 or 8 
    // so what happens if it's other than rhd_recording_controller?? 
    // check this since rhd and rhx are from the same company intan please be kind on yourself and check
    int maxNumHeadstages = (boardType == RHD_RECORDING_CONTROLLER) ? 16 : 8;
    
    for (int i = 0; i < maxNumHeadstages; i++)
        headstages.add(new Headstage(i, maxNumHeadstages));
    // This is BS, what is this 10000? who knows? did this magically show up?
    sourceBuffers.add(new DataBuffer(2, 10000)); // start with 2 channels and automatically resize

    dacStream = new int[8];
    dacChannels = new int[8];
    dacThresholds = new float[8];
    dacChannelsToUpdate = new bool[8];

    // TODO: Check for ONI device and, if found, ignore Opal Kelly library code

#if defined(__APPLE__)
    File appBundle = File::getSpecialLocation(File::currentApplicationFile);
    const String executableDirectory = appBundle.getChildFile("Contents/Resources").getFullPathName();
#else
    File executable = File::getSpecialLocation(File::currentExecutableFile);
    const String executableDirectory = executable.getParentDirectory().getFullPathName();
#endif


    String dirName = executableDirectory;
    String libraryFilePath = dirName;
    libraryFilePath += File::getSeparatorString();
    libraryFilePath += okLIB_NAME;

    std::cout << "DLL search path: " << libraryFilePath << std::endl;

    if (!okFrontPanelDLL_LoadLib(libraryFilePath.getCharPointer())) {
                std::cerr << "FrontPanel DLL could not be loaded.  " <<
                        "Make sure this DLL is in the application start directory." << std::endl;
    }

    char dll_date[32], dll_time[32];
    std::string serialNumber = "";

    okFrontPanelDLL_GetVersion(dll_date, dll_time);

    std::cout << std::endl << "FrontPanel DLL loaded.  Built: " << dll_date << "  " << dll_time << std::endl;

    frontPanelLib = std::make_unique<okCFrontPanel>();

    std::cout << std::endl << "Scanning USB for Opal Kelly devices..." << std::endl << std::endl;

    int nDevices = frontPanelLib->GetDeviceCount(); 

    std::cout << "Found " << nDevices << " Opal Kelly device" << ((nDevices == 1) ? "" : "s") <<
        " connected:" << std::endl;

    std::cout << std::endl;

    for (int i = 0; i < nDevices; ++i)
    {
        okCFrontPanel::BoardModel model = frontPanelLib->GetDeviceListModel(i);

        if (model == OK_PRODUCT_XEM6010LX45 || model == OK_PRODUCT_XEM6310LX45) //the two models we use
        {
            serialNumber = serialNumber = frontPanelLib->GetDeviceListSerial(i);

            if (model == OK_PRODUCT_XEM6310LX45)
                usbVersion = USB3;
            else
                usbVersion = USB2;

            std::cout << "Trying to open device with serial " << serialNumber.c_str() << std::endl;
            std::cout << "USB Version: " << usbVersion << std::endl;
        }
    }

    AmplifierSampleRate ampSampleRate = AmplifierSampleRate::SampleRate30000Hz;

    // Sumedh need to check if the initiation is done correctly, seems like that
    if (boardType == ACQUISITION_BOARD)
    {
        if (usbVersion == USB3)
        {
            device = std::make_unique<RhythmDevice>(ControllerType::ControllerOEOpalKellyUSB3, ampSampleRate);
        }
        else {
            device = std::make_unique<RhythmDevice>(ControllerType::ControllerOEOpalKellyUSB2, ampSampleRate);
        }
    }
    else if (boardType == INTAN_RHD_USB)
        device = std::make_unique<RHXController>(ControllerType::ControllerRecordUSB2, ampSampleRate);
    else if (boardType == RHD_RECORDING_CONTROLLER)
        device = std::make_unique<RHXController>(ControllerType::ControllerRecordUSB3, ampSampleRate);
    else if (boardType == RHS_STIM_RECORDING_CONTROLLER)
        device = std::make_unique<RHXController>(ControllerType::ControllerStimRecordUSB2, ampSampleRate);
    else if (boardType == ONI_USB)
        device = std::make_unique<OniDevice>();

    frontPanelLib.reset();
    
    // 1. attempt to open the device
    int errorCode = device->open(serialNumber.c_str(), libraryFilePath.getCharPointer());
    
    if (errorCode == -1)
    {
        AlertWindow::showMessageBox(AlertWindow::NoIcon,
                                                     "Opal Kelly library not found.",
                                                     "The Opal Kelly library file was not found in the directory of the executable. Please locate it and re-add the plugin to the signal chain.",
                                                     "Ok.");

        return;

    } else if (errorCode == -2)
    {
        AlertWindow::showMessageBox(AlertWindow::NoIcon,
                                                     "No device was found.",
                                                     "Please connect one and re-add the plugin to the signal chain.",
                                                     "Ok.");

        return;
    } 

    String bitfilename;

    bitfilename = executableDirectory;
    bitfilename += File::getSeparatorString();
    bitfilename += "shared";
    bitfilename += File::getSeparatorString();

    if (boardType == ACQUISITION_BOARD)
        bitfilename += usbVersion == USB3 ? "rhd2000_usb3.bit" : "rhd2000.bit";
    else if (boardType == INTAN_RHD_USB)
        bitfilename += "intan_rhd_usb.bit";
    else if (boardType == RHD_RECORDING_CONTROLLER)
        bitfilename += "intan_recording_controller.bit";
    else if (boardType == RHS_STIM_RECORDING_CONTROLLER)
        bitfilename += "intan_stim_record_controller.bit";

    bool success = true;
    
    if (boardType != ONI_USB)
    {

        if (!File(bitfilename).exists())
        {
            AlertWindow::showMessageBox(AlertWindow::NoIcon,
                "No bitfile was found.",
                "Please place one in the appropriate directory and re-add the plugin to the signal chain."
                "Ok.");
        }

        std::cout << "Uploading bitfile: " << bitfilename << std::endl;

        success = device->uploadFPGABitfile(bitfilename.toStdString());

    } 
    // Sumedh We found the device and now we are connecting it to the software
    // Initialize a controller connected to a USB port.
    if (success)
    {
        deviceFound = true;

        LOGD("Initializing device.");

        device->initialize();
        if (device->getType() == ControllerStimRecordUSB2) {
            device->enableDcAmpConvert(true);
            device->setExtraStates(0);
        }
        // what's the point of 30k what happens if there is a different selection here
        // need to change this to take into account all sampling rates or atleast selected
        // then if this is the initialisation, this may not be required at all
        device->setSampleRate(SampleRate30000Hz);
        // update command chip command
        updateChipCommandLists(true);
        //
        if (device->getType() != ControllerStimRecordUSB2) {
            // Select RAM Bank 0 for AuxCmd3 initially, so the ADC is calibrated.
            device->selectAuxCommandBankAllPorts(RHXController::AuxCmd3, 0);
        }

        // Since our longest command sequence is N commands, we run the SPI interface for N samples.
        device->setMaxTimeStep(RHXDataBlock::samplesPerDataBlock(device->getType()));
        device->setContinuousRunMode(false);

        // Start SPI interface.
        device->run();

        // Wait for the N-sample run to complete.
        while (device->isRunning()) {
            //qApp->processEvents();
        }

        // Read the resulting single data block from the USB interface.
        RHXDataBlock dataBlock(device->getType(), device->getNumEnabledDataStreams());
        device->readDataBlock(&dataBlock);

        if (device->getType() != ControllerStimRecordUSB2) {
            // Now that ADC calibration has been performed, we switch to the command sequence that does not execute
            // ADC calibration.
            device->selectAuxCommandBankAllPorts(RHXController::AuxCmd3, true ? 2 : 1);
        }

        // Set default configuration for all eight DACs on controller.
        int dacManualStream = (device->getType() == ControllerRecordUSB3) ? 32 : 8;
        for (int i = 0; i < 8; i++) {
            device->enableDac(i, false);
            device->selectDacDataStream(i, dacManualStream); // Initially point DACs to DacManual1 input
            device->selectDacDataChannel(i, 0);
            setDACthreshold(i, 0);
        }
        device->setDacManual(32768);
        device->setDacGain(0);
        device->setAudioNoiseSuppress(0);

        // Set default SPI cable delay values.
        device->setCableDelay(PortA, 1);
        device->setCableDelay(PortB, 1);
        device->setCableDelay(PortC, 1);
        device->setCableDelay(PortD, 1);
        bool expanderBoardDetected = false;
        if (device->getNumSPIPorts(expanderBoardDetected) > 4) {
            device->setCableDelay(PortE, 1);
            device->setCableDelay(PortF, 1);
            device->setCableDelay(PortG, 1);
            device->setCableDelay(PortH, 1);
        }
        
        
        // change end
        int maxNumHeadstages = (boardType == RHD_RECORDING_CONTROLLER) ? 16 : 8;

        for (int i = 0; i < maxNumHeadstages; i++)
            headstages.add(new Headstage(i, maxNumHeadstages));

        scanPorts();
        
        for (int k = 0; k < 8; k++)
        {
            dacChannelsToUpdate[k] = true;
            dacStream[k] = 0;
            setDACthreshold(k, 65534);
            dacChannels[k] = 0;
            dacThresholds[k] = 0;
        }

        usbThread = new USBThread(device.get());
    }
    
}

DeviceThread::~DeviceThread()
{

    LOGD("Closing device.");
    
    if (deviceFound)
        device->resetFpga();

    delete[] dacStream;
    delete[] dacChannels;
    delete[] dacThresholds;
    delete[] dacChannelsToUpdate;
}

void DeviceThread::initialize(bool signalChainIsLoading)
{

    adcChannelNames.clear();
    ttlLineNames.clear();

    for (int i = 0; i < 8; i++)
    {
        adcChannelNames.add("ADC" + String(i + 1));
        ttlLineNames.add("TTL" + String(i + 1));
    }
}

std::unique_ptr<GenericEditor> DeviceThread::createEditor(SourceNode* sn)
{

    std::unique_ptr<DeviceEditor> editor = std::make_unique<DeviceEditor>(sn, this);

    return editor;
}

void DeviceThread::updateChipCommandLists(bool updateStimParams) {

    // update code
    StimStepSize stepsize;
    if (device->getType() != ControllerStimRecordUSB2) {
        stepsize =  StimStepSizeMin;
    }
    else {
        stepsize = StimStepSize10nA;// (StimStepSize)stimStepSize->getIndex();
    }
    std::cout << "######################## " << stepsize << std::endl;
    RHXRegisters chipRegisters(device->getType(), device->getSampleRate(), stepsize);

    chipRegisters.setDigOutLow(RHXRegisters::DigOut::DigOut1); // Take auxiliary output out of HiZ mode.
    chipRegisters.setDigOutLow(RHXRegisters::DigOut::DigOut2); // Take auxiliary output out of HiZ mode.
    chipRegisters.setDigOutLow(RHXRegisters::DigOut::DigOutOD); // Take auxiliary output out of HiZ mode.

    std::vector<unsigned int> commandList;
    int numCommands = RHXDataBlock::samplesPerDataBlock(device->getType());
    int commandSequenceLength;

    if (device->getType() == ControllerStimRecordUSB2) {
        // Create a command list for the AuxCmd1 slot.  This command sequence programs most of the RAM registers
        // on the RHS2116 chip.
        commandSequenceLength = chipRegisters.createCommandListRHSRegisterConfig(commandList, updateStimParams);
        device->uploadCommandList(commandList, RHXController::AuxCmd1, 0); // RHS - bank doesn't matter
        device->selectAuxCommandLength(RHXController::AuxCmd1, 0, commandSequenceLength - 1);

        // Next, fill the other three command slots with dummy commands
        chipRegisters.createCommandListDummy(commandList, 8192, chipRegisters.createRHXCommand(RHXRegisters::RHXCommandRegRead, 255));
        device->uploadCommandList(commandList, RHXController::AuxCmd2, 0); // RHS - bank doesn't matter
        chipRegisters.createCommandListDummy(commandList, 8192, chipRegisters.createRHXCommand(RHXRegisters::RHXCommandRegRead, 254));
        device->uploadCommandList(commandList, RHXController::AuxCmd3, 0); // RHS - bank doesn't matter
        chipRegisters.createCommandListDummy(commandList, 8192, chipRegisters.createRHXCommand(RHXRegisters::RHXCommandRegRead, 253));
        device->uploadCommandList(commandList, RHXController::AuxCmd4, 0);
    }
    else {
        // Create a command list for the AuxCmd1 slot.  This command sequence will continuously
        // update Register 3, which controls the auxiliary digital output pin on each chip.
        // This permits real-time control of the digital output pin on chips on each SPI port.
        commandSequenceLength = chipRegisters.createCommandListRHDUpdateDigOut(commandList, numCommands);
        device->uploadCommandList(commandList, RHXController::AuxCmd1, 0);
        device->selectAuxCommandLength(RHXController::AuxCmd1, 0, commandSequenceLength - 1);
        device->selectAuxCommandBankAllPorts(RHXController::AuxCmd1, 0);

        // Next, we'll create a command list for the AuxCmd2 slot.  This command sequence
        // will sample the temperature sensor and other auxiliary ADC inputs.
        commandSequenceLength = chipRegisters.createCommandListRHDSampleAuxIns(commandList, numCommands);
        device->uploadCommandList(commandList, RHXController::AuxCmd2, 0);
        device->selectAuxCommandLength(RHXController::AuxCmd2, 0, commandSequenceLength - 1);
        device->selectAuxCommandBankAllPorts(RHXController::AuxCmd2, 0);
    }

    //// Set amplifier bandwidth parameters.
    ////state->holdUpdate();
    //setDspCutoffFreq(getDspCutoffFreq()); //setting this
    ////state->actualDspCutoffFreq->setValueWithLimits(chipRegisters.setDspCutoffFreq(state->desiredDspCutoffFreq->getValue()));
    //state->actualDspCutoffFreq->setValueWithLimits(setDspCutoffFreq(state->desiredDspCutoffFreq->getValue()));
    //state->actualLowerBandwidth->setValueWithLimits(chipRegisters.setLowerBandwidth(state->desiredLowerBandwidth->getValue(), 0));
    //state->actualLowerSettleBandwidth->setValueWithLimits(chipRegisters.setLowerBandwidth(state->desiredLowerSettleBandwidth->getValue(), 1));
    //state->actualUpperBandwidth->setValueWithLimits(chipRegisters.setUpperBandwidth(state->desiredUpperBandwidth->getValue()));
    //chipRegisters.enableDsp(state->dspEnabled->getValue());
    //state->releaseUpdate();

    if (device->getType() == ControllerStimRecordUSB2) {
        commandSequenceLength = chipRegisters.createCommandListRHSRegisterConfig(commandList, updateStimParams);
        // Upload version with no ADC calibration to AuxCmd1 RAM Bank.
        device->uploadCommandList(commandList, RHXController::AuxCmd1, 0); // RHS - bank doesn't matter
        device->selectAuxCommandLength(RHXController::AuxCmd1, 0, commandSequenceLength - 1);

        // Run system once for changes to take effect.
        device->setContinuousRunMode(false);
        device->setMaxTimeStep(RHXDataBlock::samplesPerDataBlock(device->getType()));
        std::cout << "SPI interface started " << std::endl;
        // Start SPI interface.
        device->run();
        std::cout << "SPI interface established, should be connected" << std::endl;
        // Wait for the 128-sample run to complete.
        while (device->isRunning()) {
            //qApp->processEvents();
        }

        device->flush();
    }
    else {
        // For the AuxCmd3 slot, we will create three command sequences.  All sequences
        // will configure and read back the RHD2000 chip registers, but one sequence will
        // also run ADC calibration.  Another sequence will enable amplifier 'fast settle'.

        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, true, numCommands);
        // Upload version with ADC calibration to AuxCmd3 RAM Bank 0.
        device->uploadCommandList(commandList, RHXController::AuxCmd3, 0);
        device->selectAuxCommandLength(RHXController::AuxCmd3, 0, commandSequenceLength - 1);

        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false, numCommands);
        // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
        device->uploadCommandList(commandList, RHXController::AuxCmd3, 1);
        device->selectAuxCommandLength(RHXController::AuxCmd3, 0, commandSequenceLength - 1);

        chipRegisters.setFastSettle(true);
        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false, numCommands);
        // Upload version with fast settle enabled to AuxCmd3 RAM Bank 2.
        device->uploadCommandList(commandList, RHXController::AuxCmd3, 2);
        device->selectAuxCommandLength(RHXController::AuxCmd3, 0, commandSequenceLength - 1);
        chipRegisters.setFastSettle(false);

        device->selectAuxCommandBankAllPorts(RHXController::AuxCmd3, false ? 2 : 1);
    }

   /* setDacHighpassFilterEnabled(state->analogOutHighpassFilterEnabled->getValue());
    setDacHighpassFilterFrequency(state->analogOutHighpassFilterFrequency->getValue());*/
}

void DeviceThread::handleBroadcastMessage(String msg)
{
    // Broadcast messages allow other plugins (or remote processes)
    // to interact with this plugin while acquisition is active.
    //
    // Available commands:
    // -------------------
    //
    // Trigger outputs of the Open Ephys Acquisition Board
    //   ACQBOARD TRIGGER 0 100   <-- triggers ~100 ms pulse on line 0
    //
    // Configure the stimulus trigger for an RHS Controller
    //   RHS STIM_TRIGGER 1 10 3  <-- set stream 1, channel 10 to trigger on digital input line 3
    //
    // Configure the stimulus shape for an RHS Controller
    //   RHS STIM_SHAPE 1 10 1 BIPHASIC   <--- set stream 1, channel 10 to deliver 1 biphasic pulse
    //

    StringArray parts = StringArray::fromTokens(msg, " ", "");

    if (parts[0].equalsIgnoreCase("ACQBOARD"))
    {
        if (parts.size() > 1)
        {
            String command = parts[1];

            if (command.equalsIgnoreCase("TRIGGER"))
            {
                if (parts.size() == 4)
                {
                    int ttlLine = parts[2].getIntValue() - 1;

                    if (ttlLine < 0 || ttlLine > 7)
                        return;

                    int eventDurationMs = parts[3].getIntValue();

                    if (eventDurationMs < 10 || eventDurationMs > 5000)
                        return;

                    DigitalOutputCommand command;
                    command.ttlLine = ttlLine;
                    command.state = true;

                    digitalOutputCommands.push(command);

                    DigitalOutputTimer* timer = new DigitalOutputTimer(this, ttlLine, eventDurationMs);

                    digitalOutputTimers.add(timer);

                }
            }
        }
    }
    else if (parts[0].equalsIgnoreCase("RHS") && boardType == RHS_STIM_RECORDING_CONTROLLER)
    {
        if (parts.size() > 1)
        {
            String command = parts[1];

            if (command.equalsIgnoreCase("STIM_TRIGGER"))
            {
                if (parts.size() == 5)
                {
                    int stream = parts[2].getIntValue();
                    int channel = parts[3].getIntValue();
                    int trigger_line = parts[4].getIntValue();

                    if (stream < 0 || stream > enabledStreams.size() - 1)
                        return;

                    if (channel < 0 || channel > 15)
                        return;

                    if (trigger_line < 0 || trigger_line > 7)
                        return;

                    device->configureStimTrigger(stream, channel, trigger_line, true, true, false);
                }
            }
            else if (command.equalsIgnoreCase("STIM_SHAPE"))
            {
                if (parts.size() == 6)
                {
                    int stream = parts[2].getIntValue();
                    int channel = parts[3].getIntValue();
                    int num_pulses = parts[4].getIntValue();

                    String shapeCommand = parts[5];
                    StimShape shape;

                    if (shapeCommand.equalsIgnoreCase("BIPHASIC"))
                        shape = Biphasic;
                    else if (shapeCommand.equalsIgnoreCase("BIPHASIC_WITH_DELAY"))
                        shape = BiphasicWithInterphaseDelay;
                    else if (shapeCommand.equalsIgnoreCase("TRIPHASIC"))
                        shape = Triphasic;
                    else if (shapeCommand.equalsIgnoreCase("MONOPHASIC"))
                        shape = Monophasic;
                    else
                        return;

                    if (stream < 0 || stream > enabledStreams.size() - 1)
                        return;

                    if (channel < 0 || channel > 15)
                        return;

                    if (num_pulses < 0 || num_pulses > 10)
                        return;

                    device->configureStimPulses(stream, channel, num_pulses, shape, true);
                }
            }

        }

    }

}


void DeviceThread::addDigitalOutputCommand(DigitalOutputTimer* timerToDelete, int ttlLine, bool state)
{
    DigitalOutputCommand command;
    command.ttlLine = ttlLine;
    command.state = state;

    digitalOutputCommands.push(command);

    digitalOutputTimers.removeObject(timerToDelete);
}

DeviceThread::DigitalOutputTimer::DigitalOutputTimer(DeviceThread* board_, int ttlLine_, int eventDurationMs)
    : board(board_)
{

    tllOutputLine = ttlLine_;

    startTimer(eventDurationMs);
}

void DeviceThread::DigitalOutputTimer::timerCallback()
{
    stopTimer();

    board->addDigitalOutputCommand(this, tllOutputLine, false);
}

void DeviceThread::setDACthreshold(int dacOutput, float threshold)
{
    dacThresholds[dacOutput]= threshold;
    dacChannelsToUpdate[dacOutput] = true;
    updateSettingsDuringAcquisition = true;

    //evalBoard->setDacThresholdVoltage(dacOutput,threshold);
}

void DeviceThread::setDACchannel(int dacOutput, int channel)
{
    if (channel < getNumDataOutputs(ContinuousChannel::ELECTRODE))
    {
        int channelCount = 0;

        for (int i = 0; i < enabledStreams.size(); i++)
        {
            if (channel < channelCount + numChannelsPerDataStream[i])
            {
                dacChannels[dacOutput] = channel - channelCount;
                dacStream[dacOutput] = i;
                break;
            }
            else
            {
                channelCount += numChannelsPerDataStream[i];
            }
        }
        dacChannelsToUpdate[dacOutput] = true;
        updateSettingsDuringAcquisition = true;
    }
}

Array<int> DeviceThread::getDACchannels() const
{
    Array<int> dacChannelsArray;

    for (int k = 0; k < 8; ++k)
    {
        dacChannelsArray.add (dacChannels[k]);
    }

    return dacChannelsArray;
}


void DeviceThread::scanPorts()
{
    
    LOGD("Scanning ports.");

    if (!deviceFound) //Safety to avoid crashes if board not present
    {
        return;
    }

    LOGD("Stopping impedance thread.");

    impedanceThread->stopThreadSafely();

    //Clear previous known streams
    enabledStreams.clear();

    std::vector<ChipType> chipType;
    std::vector<int> portIndex;
    std::vector<int> commandStream;
    std::vector<int> numChannelsOnPort;

    LOGD("Searching for connected chips...");
    int warningCode = device->findConnectedChips(chipType, portIndex, commandStream, numChannelsOnPort);
    std::cout << "1 == all fine, ports scanned, Warning code :: " << warningCode << std::endl;
    if (warningCode == -1)
    {
        std::cerr << "Error in DeviceThread, This RHD USB interface board can support only 256 amplifier channels. More than 256 total amplifier channels are currently connected. Amplifier chips exceeding this limit will not appear in the GUI.";
    }
    else if (warningCode == -2)
    {
        std::cerr << "This RHD USB interface board can support only 256 amplifier channels.More than 256 total amplifier channels are currently connected.  (Each RHD2216 chip counts as 32 channels.).Amplifier chips exceeding this limit will not appear in the GUI.\n";
    }

    int numDataStreams = 0;
    for (int i = 0; i < (int)portIndex.size(); ++i) {
        if (portIndex[i] != -1) ++numDataStreams;
    }
    std::cout << "From the port we have numDataStreams " << numDataStreams << std::endl;
    // initialize headstage objects

    for (auto headstage : headstages)
    {
        headstage->setNumStreams(0); // reset stream count
    }

    chipId.clear();

  
    for (int i = 0; i < chipType.size(); i++)
    {
        LOGC(int(chipType[i]), " ", portIndex[i], " ", commandStream[i]);

        chipId.add(chipType[i]);

        if (commandStream[i] > -1)
        {
            if (chipType[i] == RHD2164Chip)
            {
                enableHeadstage(commandStream[i] / 2, true, 2, 32);
            }
            else if (chipType[i] == RHD2132Chip)
            {
                enableHeadstage(commandStream[i] / 2, true, 1, 32);
            }
            else if (chipType[i] == RHD2216Chip)
            {
                enableHeadstage(commandStream[i] / 2, true, 1, 16);
            }
            else if (chipType[i] == RHS2116Chip)
            {
                enableHeadstage(commandStream[i] / 2, true, 1, 16);
            }
        }

    }

    std::cout << "Chip IDs: " << std::endl;
    for (int i = 0; i < chipId.size(); i++)
        std::cout << chipId[i] << std::endl;

    for (int i = 0; i < enabledStreams.size(); i++)
    {
        //std::cout << "Enabling stream " << i << " with source " << enabledStreams[i] << std::endl;
        device->enableDataStream(i, true);
        device->setDataSource(i, BoardDataSource(enabledStreams[i]));
    }
}



void DeviceThread::updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
    OwnedArray<EventChannel>* eventChannels,
    OwnedArray<SpikeChannel>* spikeChannels,
    OwnedArray<DataStream>* sourceStreams,
    OwnedArray<DeviceInfo>* devices,
    OwnedArray<ConfigurationObject>* configurationObjects)
{

    if (!deviceFound)
        return;

    continuousChannels->clear();
    eventChannels->clear();
    spikeChannels->clear();
    sourceStreams->clear();
    devices->clear();
    configurationObjects->clear();

    channelNames.clear();

    // create device
    // CODE GOES HERE

    DataStream::Settings dataStreamSettings
    {
        "Rhythm_Data",
        "Continuous and event data from a device running Rhythm FPGA firmware",
        "rhythm-fpga-device.data",

        static_cast<float>(device->getSampleRate())

    };

    DataStream* stream = new DataStream(dataStreamSettings);

    sourceStreams->add(stream);

    int hsIndex = -1;

    for (auto headstage : headstages)
    {
        hsIndex++;

        if (headstage->isConnected())
        {
            for (int ch = 0; ch < headstage->getNumChannels(); ch++)
            {

                ContinuousChannel::Settings channelSettings{
                    ContinuousChannel::ELECTRODE,
                    headstage->getChannelName(ch),
                    "Headstage channel from a Rhythm FPGA device",
                    "rhythm-fpga-device.continuous.headstage",

                    0.195,

                    stream
                };

                continuousChannels->add(new ContinuousChannel(channelSettings));
                continuousChannels->getLast()->setUnits("uV");

                if (impedances.valid)
                {
                    continuousChannels->getLast()->impedance.magnitude = headstage->getImpedanceMagnitude(ch);
                    continuousChannels->getLast()->impedance.phase = headstage->getImpedancePhase(ch);
                }

            }

            if (settings.acquireAux)
            {
                for (int ch = 0; ch < 3; ch++)
                {

                    ContinuousChannel::Settings channelSettings{
                        ContinuousChannel::AUX,
                        headstage->getStreamPrefix() + "_AUX" + String(ch + 1),
                        "Aux input channel from a Rhythm FPGA device",
                        "rhythm-fpga-device.continuous.aux",

                        0.0000374,

                        stream
                    };

                    continuousChannels->add(new ContinuousChannel(channelSettings));
                    continuousChannels->getLast()->setUnits("mV");

                }
            }
        }
    }

    if (settings.acquireAdc)
    {
        for (int ch = 0; ch < 8; ch++)
        {

            String name = "ADC" + String(ch + 1);

            ContinuousChannel::Settings channelSettings{
                ContinuousChannel::ADC,
                name,
                "ADC input channel from a Rhythm FPGA device",
                "rhythm-fpga-device.continuous.adc",

                getAdcBitVolts(ch),

                stream
            };

            continuousChannels->add(new ContinuousChannel(channelSettings));
            continuousChannels->getLast()->setUnits("V");

        }
    }

    EventChannel::Settings settings{
            EventChannel::Type::TTL,
            "Rhythm FPGA TTL Input",
            "Events on digital input lines of a Rhythm FPGA device",
            "rhythm-fpga-device.events",
            stream,
            8
    };

    eventChannels->add(new EventChannel(settings));

}

void DeviceThread::impedanceMeasurementFinished()
{

    if (impedances.valid)
    {
        LOGD( "Updating headstage impedance values" );

        for (auto hs : headstages)
        {
            if (hs->isConnected())
            {
                hs->setImpedances(impedances);
            }
        }
    }
}

void DeviceThread::saveImpedances(File& file)
{

    if (impedances.valid)
    {
        std::unique_ptr<XmlElement> xml = std::unique_ptr<XmlElement>(new XmlElement("IMPEDANCES"));

        int globalChannelNumber = -1;

        for (auto hs : headstages)
        {
            XmlElement* headstageXml = new XmlElement("HEADSTAGE");
            headstageXml->setAttribute("name", hs->getStreamPrefix());

            for (int ch = 0; ch < hs->getNumActiveChannels(); ch++)
            {
                globalChannelNumber++;

                XmlElement* channelXml = new XmlElement("CHANNEL");
                channelXml->setAttribute("name", hs->getChannelName(ch));
                channelXml->setAttribute("number", globalChannelNumber);
                channelXml->setAttribute("magnitude", hs->getImpedanceMagnitude(ch));
                channelXml->setAttribute("phase", hs->getImpedancePhase(ch));
                headstageXml->addChildElement(channelXml);
            }

            xml->addChildElement(headstageXml);
        }

        xml->writeTo(file);
    }

}

String DeviceThread::getChannelName(int i) const
{
    return channelNames[i];
}

bool DeviceThread::isAcquisitionActive() const
{
    return isTransmitting;
}

void DeviceThread::setNamingScheme(ChannelNamingScheme scheme)
{

    channelNamingScheme = scheme;

    for (auto hs : headstages)
    {
        hs->setNamingScheme(scheme);
    }
}

ChannelNamingScheme DeviceThread::getNamingScheme()
{
    return channelNamingScheme;
}

void DeviceThread::setNumChannels(int hsNum, int numChannels)
{
    if (headstages[hsNum]->getNumChannels() == 32)
    {
        if (numChannels < headstages[hsNum]->getNumChannels())
            headstages[hsNum]->setHalfChannels(true);
        else
            headstages[hsNum]->setHalfChannels(false);

        numChannelsPerDataStream.set(headstages[hsNum]->getStreamIndex(0), numChannels);
    }

    int channelIndex = 0;

    for (auto hs : headstages)
    {
        if (hs->isConnected())
        {
            hs->setFirstChannel(channelIndex);

            channelIndex += hs->getNumActiveChannels();
        }
    }
}

int DeviceThread::getHeadstageChannels (int hsNum) const
{
    return headstages[hsNum]->getNumChannels();
}


int DeviceThread::getNumChannels()
{
    int totalChannels = getNumDataOutputs(ContinuousChannel::ELECTRODE)
           + getNumDataOutputs(ContinuousChannel::AUX)
           + getNumDataOutputs(ContinuousChannel::ADC);

    return totalChannels;
}

int DeviceThread::getNumDataOutputs(ContinuousChannel::Type type)
{

    if (type == ContinuousChannel::ELECTRODE)
    {
        int totalChannels = 0;

        for (auto headstage : headstages)
        {
            if (headstage->isConnected())
            {
                totalChannels += headstage->getNumActiveChannels();
            }
        }

        return totalChannels;
    }
    if (type == ContinuousChannel::AUX)
    {
        if (settings.acquireAux)
        {
            int numAuxOutputs = 0;

            for (auto headstage : headstages)
            {
                if (headstage->isConnected())
                {
                    numAuxOutputs += 3;
                }
            }
            return numAuxOutputs;
        }
        else
        {
            return 0;
        }
    }
    if (type == ContinuousChannel::ADC)
    {
        if (settings.acquireAdc)
        {
            return 8;
        }
        else
        {
            return 0;
        }
    }

    return 0;
}


float DeviceThread::getAdcBitVolts (int chan) const
{
    if (chan < adcBitVolts.size())
    {
        return adcBitVolts[chan];
    }
    else
    {
        if (boardType == ACQUISITION_BOARD)
        {
            return 0.00015258789; // +/-5V / pow(2,16)
        }
        else if (boardType == INTAN_RHD_USB)
        {
            return 0.0000503540039;  // 3.3V / pow(2,16)
        }
    }
}

double DeviceThread::setUpperBandwidth(double upper)
{
    impedanceThread->stopThreadSafely();

    settings.dsp.upperBandwidth = upper;

    //updateRegisters();
    LOGC("DeviceThread::setUpperBandwidth NOT IMPLEMENTED.");

    return settings.dsp.upperBandwidth;
}


double DeviceThread::setLowerBandwidth(double lower)
{
    impedanceThread->stopThreadSafely();

    settings.dsp.lowerBandwidth = lower;

    LOGC("DeviceThread::setLowerBandwidth NOT IMPLEMENTED.");
//updateRegisters();

    return settings.dsp.lowerBandwidth;
}

double DeviceThread::setDspCutoffFreq(double freq)
{
    impedanceThread->stopThreadSafely();

    settings.dsp.cutoffFreq = freq;

    device->setDacHighpassFilter(freq);

    return settings.dsp.cutoffFreq;
}

double DeviceThread::getDspCutoffFreq() const
{
    return settings.dsp.cutoffFreq;
}

void DeviceThread::setDSPOffset(bool state)
{

    impedanceThread->stopThreadSafely();

    settings.dsp.enabled = state;

    LOGC("DeviceThread::setDSPOffset NOT IMPLEMENTED.");
}

void DeviceThread::setTTLoutputMode(bool state)
{
    settings.ttlMode = state;

    updateSettingsDuringAcquisition = true;
}

void DeviceThread::setDAChpf(float cutoff, bool enabled)
{
    settings.desiredDAChpf = cutoff;

    settings.desiredDAChpfState = enabled;

    updateSettingsDuringAcquisition = true;
}

void DeviceThread::setFastTTLSettle(bool state, int channel)
{
    settings.fastTTLSettleEnabled = state;

    settings.fastSettleTTLChannel = channel;

    updateSettingsDuringAcquisition = true;
}

int DeviceThread::setNoiseSlicerLevel(int level)
{
    settings.noiseSlicerLevel = level;

    LOGC("DeviceThread::setNoiseSlicerLevel NOT IMPLEMENTED.");

    // Level has been checked once before this and then is checked again in setAudioNoiseSuppress.
    // This may be overkill - maybe API should change so that the final function returns the value?

    return settings.noiseSlicerLevel;
}


bool DeviceThread::foundInputSource()
{
    return deviceFound;
}

bool DeviceThread::enableHeadstage(int hsNum, bool enabled, int nStr, int strChans)
{

    if (enabled)
    {

        std::cout << "Enabling headstage " << hsNum << " with " << nStr << " streams." << std::endl;

        headstages[hsNum]->setFirstChannel(getNumDataOutputs(ContinuousChannel::ELECTRODE));
        headstages[hsNum]->setNumStreams(nStr);
        headstages[hsNum]->setChannelsPerStream(strChans);
        headstages[hsNum]->setFirstStreamIndex(enabledStreams.size());
        enabledStreams.add(headstages[hsNum]->getDataStream(0));
        numChannelsPerDataStream.add(strChans);

        if (nStr > 1)
        {
            enabledStreams.add(headstages[hsNum]->getDataStream(1));
            numChannelsPerDataStream.add(strChans);
        }
    }
    else
    {
        int idx = enabledStreams.indexOf(headstages[hsNum]->getDataStream(0));

        if (idx >= 0)
        {
            enabledStreams.remove(idx);
            numChannelsPerDataStream.remove(idx);
        }

        if (headstages[hsNum]->getNumStreams() > 1)
        {
            idx = enabledStreams.indexOf(headstages[hsNum]->getDataStream(1));
            if (idx >= 0)
            {
                enabledStreams.remove(idx);
                numChannelsPerDataStream.remove(idx);
            }
        }

        headstages[hsNum]->setNumStreams(0);
    }

    sourceBuffers[0]->resize(getNumChannels(), 10000);

    return true;
}

bool DeviceThread::isHeadstageEnabled(int hsNum) const
{
    return headstages[hsNum]->isConnected();
}


int DeviceThread::getActiveChannelsInHeadstage (int hsNum) const
{
    return headstages[hsNum]->getNumActiveChannels();
}

int DeviceThread::getChannelsInHeadstage (int hsNum) const
{
    return headstages[hsNum]->getNumChannels();
}

/*void DeviceThread::assignAudioOut(int dacChannel, int dataChannel)
{
    if (deviceFound)
    {
        if (dacChannel == 0)
        {
            audioOutputR = dataChannel;
            dacChannels[0] = dataChannel;
        }
        else if (dacChannel == 1)
        {
            audioOutputL = dataChannel;
            dacChannels[1] = dataChannel;
        }

        updateSettingsDuringAcquisition = true; // set a flag and take care of setting wires
        // during the updateBuffer() method
        // to avoid problems
    }

}*/

void DeviceThread::enableAuxs(bool t)
{
    settings.acquireAux = t;
    sourceBuffers[0]->resize(getNumChannels(), 10000);
}

void DeviceThread::enableAdcs(bool t)
{
    settings.acquireAdc = t;
    sourceBuffers[0]->resize(getNumChannels(), 10000);
}

bool DeviceThread::isAuxEnabled()
{
    return settings.acquireAux;
}

void DeviceThread::setSampleRate(int sampleRateIndex, bool isTemporary)
{
    impedanceThread->stopThreadSafely();
    
    if (!isTemporary)
    {
        settings.savedSampleRateIndex = sampleRateIndex;
    }
    
    device->setSampleRate((AmplifierSampleRate) sampleRateIndex);
}

unsigned int DeviceThread::calculateDataBlockSizeInWords(int numDataStreams, bool usb3, int nSamples)
{
    unsigned int samps = nSamples <= 0 ? SAMPLES_PER_DATA_BLOCK(usb3) : nSamples;
    return samps * (4 + 2 + numDataStreams * 36 + 8 + 2);
}

bool DeviceThread::startAcquisition()
{
    
    impedanceThread->waitSafely();
    
    if (!deviceFound)
        return false;

    dataBlock.reset();

    LOGD("Num streams: ", device->getNumEnabledDataStreams());

    dataBlock = std::make_unique<RHXDataBlock>(device->getType(), device->getNumEnabledDataStreams());

    blockSize = dataBlock->dataBlockSizeInWords();
        
    int ledArray[8] = { 1, 1, 0, 0, 0, 0, 0, 0 };
    device->setLedDisplay(ledArray);

    device->flush();

    LOGD("Starting usb thread with buffer of ", blockSize * 16 , " bytes");
    usbThread->startAcquisition(blockSize * 16);

    device->setContinuousRunMode(true);
    device->run();
    
    // reset TTL output state
    for (int k = 0; k < 16; k++)
    {
        TTL_OUTPUT_STATE[k] = 0;
    }
    
    startThread();

    return true;
}

bool DeviceThread::stopAcquisition()
{

    LOGD("DeviceThread stopping acquisition.");

    usbThread->stopAcquisition();

    if (isThreadRunning())
    {
        signalThreadShouldExit();

    }

    if (waitForThreadToExit(500))
    {
        LOGD("DeviceThread exited.");
    }
    else
    {
        LOGD("DeviceThread failed to exit, continuing anyway...");
    }

    if (deviceFound)
    {
        device->setContinuousRunMode(false);
        device->setMaxTimeStep(0);
        LOGD("Flushing FIFO.");
        device->flush();
    }

    if (deviceFound && boardType == ACQUISITION_BOARD)
    {
        int ledArray[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
        device->setLedDisplay(ledArray);
    }

    sourceBuffers[0]->clear();
    
    isTransmitting = false;
    updateSettingsDuringAcquisition = false;

    // remove timers
    digitalOutputTimers.clear();

    // remove commands
    while (!digitalOutputCommands.empty())
        digitalOutputCommands.pop();

    return true;
}

bool DeviceThread::updateBuffer()
{
            
    uint8_t* usbBuffer;
    oni_frame_t** frames;
    double ts;

    if (usbVersion == USB3 || device->getNumWordsInFifo() >= blockSize)
    {
        
        if (boardType == ONI_USB)
        {
            if (usbThread->getOniFrames(frames) == 0)
                return true;
        }
        else {
            if (usbThread->usbRead(usbBuffer) == 0)
            {
                return true;
            }
        }
        
        int index = 0;// blockSize* BytesPerWord* blockSize;
        int auxIndex, chanIndex;
        int numStreams = enabledStreams.size();
        int nSamps = dataBlock->samplesPerDataBlock();
        int highWord, index1, index2;
        int numAuxChannels = 4;

        dataBlock->fillFromUsbBuffer(usbBuffer,0);

        for (int t = 0; t < nSamps; ++t) {
            if (!dataBlock->checkUsbHeader(usbBuffer, index)) {
                std::cerr << "DeviceThread: Error in RHXDataBlock::fillFromUsbBuffer: Incorrect header.\n";
            }
            
            index += 8;
            int64 timestamp = dataBlock->convertUsbTimeStamp(usbBuffer, index);
            index += 4;

            // Read auxiliary command results 0-2 (for stim/record controller, read auxiliary command results 1-3)
            index1 = t * numStreams * numAuxChannels;
            for (int channel = numAuxChannels - 3; channel < numAuxChannels; ++channel) {
                index2 = channel * numStreams;
                int lfp = numStreams;
                int lfp2 = numAuxChannels;
                for (int stream = 0; stream < numStreams; ++stream) {
                    //auxiliaryDataInternal[index1 + index2 + stream] = dataBlock->convertUsbWord(usbBuffer, index);
                    index += 2;
                    if (device->getType() == ControllerStimRecordUSB2) {
                        if (channel == 2) {
                            //highWord = dataBlock->convertUsbWord(usbBuffer, index); // The top 16 bits will be either all 1's (results of a WRITE command)
                                                                        // or all 0's (results of a READ command)
                            //if (highWord == 0) {  // update compliance limit only if a 'read' command was executed, denoting a read from Register 40
                            //    for (int ch = 0; ch < channelsPerStream(); ++ch) {
                            //        complianceLimitInternal[complianceIndex++] = (auxiliaryDataInternal[index1 + (2 * numStreams) + stream] & (1 << ch)) ? 1 : 0;
                            //    }
                            //}
                            //else {
                            //    for (int ch = 0; ch < channelsPerStream(); ++ch) {
                            //        complianceLimitInternal[complianceIndex++] = 0;  // if Register 40 was not read, assume no compliance limit violations
                            //    }
                            //}
                        }
                        index += 2;
                    }
                }
            }

            //Sumedh Change this now
            // Read amplifier channels. 
            
            //std::cout << " numStreams " << numStreams << std::endl;
            for (int channel = 0; channel < 16; ++channel) {
                for (int stream = 0; stream < 8; ++stream) {
                //int lfp3 = numChannelsPerDataStream[stream];
                    
                    if (device->getType() == ControllerStimRecordUSB2) {
                        //dcAmplifierDataInternal[dcAmpIndex++] = dataBlock->convertUsbWord(usbBuffer, index);
                        index += 2;
                    }
                    thisSample[channel] = float(*(uint16*)(usbBuffer + channel) - 32768) * 0.195f;
                    //amplifierDataInternal[ampIndex++] = dataBlock->convertUsbWord(usbBuffer, index);
                    index += 2;
                }
            }
            //std::cout << "index at the rhsblock :: " << index << std::endl;
            if (device->getType() == ControllerStimRecordUSB2) {
                // Read auxiliary command 0 results (see above for auxiliary command 1-3 results).
                for (int stream = 0; stream < numStreams; ++stream) {
                    //auxiliaryDataInternal[index1 + stream] = dataBlock->convertUsbWord(usbBuffer, index);
                    index += 2;
                    index += 2; // We are skipping the top 16 bits here since they will typically be either all 1's (results of a WRITE command)
                                // or all 0's (results of a READ command).
                }

                // Read stimulation control parameters.
                for (int stream = 0; stream < numStreams; ++stream) {
                    //stimOnInternal[stimOnIndex++] = dataBlock->convertUsbWord(usbBuffer, index);
                    index += 2;
                }

                for (int stream = 0; stream < numStreams; ++stream) {
                    //stimPolInternal[stimPolIndex++] = dataBlock->convertUsbWord(usbBuffer, index);
                    index += 2;
                }

                for (int stream = 0; stream < numStreams; ++stream) {
                    //ampSettleInternal[ampSettleIndex++] = dataBlock->convertUsbWord(usbBuffer, index);
                    index += 2;
                }

                for (int stream = 0; stream < numStreams; ++stream) {
                    //chargeRecovInternal[chargeRecovIndex++] = dataBlock->convertUsbWord(usbBuffer, index);
                    index += 2;
                }

                // Read from DACs.
                for (int i = 0; i < 8; ++i) {
                    //boardDacDataInternal[dacIndex++] = dataBlock->convertUsbWord(usbBuffer, index);
                    index += 2;
                }
            }

            // Skip filler words in each data stream.
            if (device->getType() == ControllerRecordUSB2) {
                index += 2 * numStreams;
            }
            else if (device->getType() == ControllerRecordUSB3) {
                index += 2 * (numStreams % 4);
            }

            // Read from ADCs.
            for (int i = 0; i < 8; ++i) {
               //boardAdcDataInternal[adcIndex++] = dataBlock->convertUsbWord(usbBuffer, index);
                index += 2;
            }

            // Read TTL input and output values.
            //ttlInInternal[t] = dataBlock->convertUsbWord(usbBuffer, index);
            uint64 ttlEventWord = *(uint64*)(usbBuffer + index) & 65535;
            index += 2;

            //ttlOutInternal[t] = dataBlock->convertUsbWord(usbBuffer, index);
            index += 2;

            sourceBuffers[0]->addToBuffer(thisSample,
                &timestamp,
                &ts,
                &ttlEventWord,
                1);

        }

    }

    if (updateSettingsDuringAcquisition)
    {
        for (int k = 0; k < 8; k++)
        {
            if (dacChannelsToUpdate[k])
            {
                dacChannelsToUpdate[k] = false;
                
                if (dacChannels[k] >= 0)
                {
                    device->enableDac(k, true);
                    device->selectDacDataStream(k, dacStream[k]);
                    device->selectDacDataChannel(k, dacChannels[k]);
                    device->setDacThreshold(k, (int)abs((dacThresholds[k]/0.195) + 32768),dacThresholds[k] >= 0);
                   // evalBoard->setDacThresholdVoltage(k, (int) dacThresholds[k]);
                }
                else
                {
                    device->enableDac(k, false);
                }
            }
        }

        device->setTtlMode(settings.ttlMode ? 1 : 0);
        device->enableExternalFastSettle(settings.fastTTLSettleEnabled);
        device->setExternalFastSettleChannel(settings.fastSettleTTLChannel);
        device->setDacHighpassFilter(settings.desiredDAChpf);
        device->enableDacHighpassFilter(settings.desiredDAChpfState);
        device->enableLeds(settings.ledsEnabled);
        device->setClockDivider(settings.clockDivideFactor);

        updateSettingsDuringAcquisition = false;
    }

    if (!digitalOutputCommands.empty())
    {

        while (!digitalOutputCommands.empty())
        {
            DigitalOutputCommand command = digitalOutputCommands.front();
            TTL_OUTPUT_STATE[command.ttlLine] = command.state;
            digitalOutputCommands.pop();

        }

        device->setTtlOut(TTL_OUTPUT_STATE);

        LOGB("TTL OUTPUT STATE: ",
            TTL_OUTPUT_STATE[0],
            TTL_OUTPUT_STATE[1],
            TTL_OUTPUT_STATE[2],
            TTL_OUTPUT_STATE[3],
            TTL_OUTPUT_STATE[4],
            TTL_OUTPUT_STATE[5],
            TTL_OUTPUT_STATE[6],
            TTL_OUTPUT_STATE[7]);

    }

    return true;

}

int DeviceThread::getChannelFromHeadstage (int hs, int ch)
{
    int channelCount = 0;
    int hsCount = 0;
    if (hs < 0 || hs >= headstages.size() + 1)
        return -1;
    if (hs == headstages.size()) //let's consider this the ADC channels
    {
        int adcOutputs = getNumDataOutputs(ContinuousChannel::ADC);

        if (adcOutputs> 0)
        {
            return getNumDataOutputs(ContinuousChannel::ELECTRODE) + getNumDataOutputs(ContinuousChannel::AUX) + ch;
        }
        else
            return -1;
    }
    if (headstages[hs]->isConnected())
    {
        if (ch < 0)
            return -1;
        if (ch < headstages[hs]->getNumActiveChannels())
        {
            for (int i = 0; i < hs; i++)
            {
                channelCount += headstages[i]->getNumActiveChannels();
            }
            return channelCount + ch;
        }
        else if (ch < headstages[hs]->getNumActiveChannels() + 3)
        {
            for (int i = 0; i < headstages.size(); i++)
            {
                if (headstages[i]->isConnected())
                {
                    channelCount += headstages[i]->getNumActiveChannels();
                    if (i < hs)
                        hsCount++;
                }
            }
            return channelCount + hsCount * 3 + ch-headstages[hs]->getNumActiveChannels();
        }
        else
        {
            return -1;
        }

    }
    else
    {
        return -1;
    }
}

Array<const Headstage*> DeviceThread::getConnectedHeadstages()
{
    Array<const Headstage*> headstageArray;

    for (auto hs : headstages)
    {
        if (hs->isConnected())
            headstageArray.add(hs);
    }

    return headstageArray;
}

int DeviceThread::getHeadstageChannel (int& hs, int ch) const
{
    int channelCount = 0;
    int hsCount = 0;

    if (ch < 0)
        return -1;

    for (int i = 0; i < headstages.size(); i++)
    {
        if (headstages[i]->isConnected())
        {
            int chans = headstages[i]->getNumActiveChannels();

            if (ch >= channelCount && ch < channelCount + chans)
            {
                hs = i;
                return ch - channelCount;
            }
            channelCount += chans;
            hsCount++;
        }
    }
    if (ch < (channelCount + hsCount * 3)) //AUX
    {
        hsCount = (ch - channelCount) / 3;

        for (int i = 0; i < headstages.size(); i++)
        {
            if (headstages[i]->isConnected())
            {
                if (hsCount == 0)
                {
                    hs = i;
                    return ch - channelCount;
                }
                hsCount--;
                channelCount++;
            }
        }
    }
    return -1;
}

void DeviceThread::enableBoardLeds(bool enable)
{
    settings.ledsEnabled = enable;

    if (isAcquisitionActive())
        updateSettingsDuringAcquisition = true;
    else
        device->enableLeds(enable);
}

int DeviceThread::setClockDivider(int divide_ratio)
{
    if (!deviceFound)
        return 1;

    // Divide ratio should be 1 or an even number
    if (divide_ratio != 1 && divide_ratio % 2)
        divide_ratio--;

    // Format the divide ratio from its true value to the
    // format required by the firmware
    // Ratio    N
    // 1        0
    // >=2      Ratio/2
    if (divide_ratio == 1)
        settings.clockDivideFactor = 0;
    else
        settings.clockDivideFactor = static_cast<uint16>(divide_ratio/2);

    if (isAcquisitionActive())
        updateSettingsDuringAcquisition = true;
    else
        device->setClockDivider(settings.clockDivideFactor);

    return divide_ratio;
}

void DeviceThread::setAdcRange(int channel, short range)
{
    adcRangeSettings[channel] = range;
}

short DeviceThread::getAdcRange(int channel) const
{
    return adcRangeSettings[channel];
}

void DeviceThread::runImpedanceTest(double frequency)
{

    impedanceThread->stopThreadSafely();

    impedanceThread->setFrequency(frequency);

    impedanceThread->runThread();

}



