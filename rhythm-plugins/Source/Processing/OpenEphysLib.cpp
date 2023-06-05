/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2013 Open Ephys

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

#include <PluginInfo.h>

#include "../Processing/InputSource/AcquisitionBoard.h"
#include "../Processing/InputSource/IntanUSBInterface.h"
#include "../Processing/OutputSink/AcqBoardOutput.h"
#include "../Processing/InputSource/IntanRecordController.h"
#include "../Processing/InputSource/IntanStimRecordController.h"

#include <string>
#ifdef WIN32
#include <Windows.h>
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

using namespace Plugin;
#define NUM_PLUGINS 5

extern "C" EXPORT void getLibInfo(Plugin::LibraryInfo* info)
{
	info->apiVersion = PLUGIN_API_VER;
	info->name = "Rhythm Plugins";
	info->libVersion = "0.3.0";
	info->numPlugins = NUM_PLUGINS;
}

extern "C" EXPORT int getPluginInfo(int index, Plugin::PluginInfo* info)
{
	switch (index)
	{
	case 0:
		info->type = Plugin::Type::DATA_THREAD;
		info->dataThread.name = "Acquisition Board";
		info->dataThread.creator = &createDataThread<RhythmNode::AcquisitionBoard>;
		break;
	case 1:
		info->type = Plugin::Type::DATA_THREAD;
		info->dataThread.name = "Intan RHD USB";
		info->dataThread.creator = &createDataThread<RhythmNode::IntanUSBInterface>;
		break;
	case 2:
		info->type = Plugin::Type::PROCESSOR;
		info->processor.name = "Acq Board Output";
		info->processor.type = Plugin::Processor::SINK;
		info->processor.creator = &createProcessor<AcqBoardOutputNamespace::AcqBoardOutput>;
		break;
	case 3:
		info->type = Plugin::Type::DATA_THREAD;
		info->dataThread.name = "RHD Rec Controller";
		info->dataThread.creator = &createDataThread<RhythmNode::IntanRecordController>;
		break;
    case 4:
        info->type = Plugin::Type::DATA_THREAD;
        info->dataThread.name = "RHS";
        info->dataThread.creator = &createDataThread<RhythmNode::IntanStimRecordController>;
        break;
	default:
		return -1;
		break;
	}
	return 0;
}

#ifdef WIN32
BOOL WINAPI DllMain(IN HINSTANCE hDllHandle,
	IN DWORD     nReason,
	IN LPVOID    Reserved)
{
	return TRUE;
}

#endif
