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


#ifndef __STIMRECORDCONTROLLER_H_2C4CBD67__
#define __STIMRECORDCONTROLLER_H_2C4CBD67__

#include "../Threads/DeviceThread.h"

namespace RhythmNode
{

    /**
        Communicates with the Intan RHS Stim/Recording Controller

        https://intantech.com/stim_record_controller.html

        @see DataThread, SourceNode
        */

    class IntanStimRecordController : public DeviceThread
    {
    public:

        /** Constructor */
        IntanStimRecordController(SourceNode* sn) : DeviceThread(sn, RHS_STIM_RECORDING_CONTROLLER) { }

        /** Destructor */
        ~IntanStimRecordController() { }
    };
}


#endif

