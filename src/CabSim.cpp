﻿/*
Copyright (c) 2022 Robin E. R. Davies

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// CabSim.cpp : Defines the entry point for the application.
//

#include "CabSim.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;
using namespace toob;

#ifndef _MSC_VER
#include <unistd.h>
#include <signal.h>
#include <csignal>
#endif


const int MAX_UPDATES_PER_SECOND = 10;

const char* CabSim::URI= CAB_SIM_URI;


FilterCoefficients2 CabSim::LOWPASS_PROTOTYPE = FilterCoefficients2(
	0.8291449788086549, 0, 0,
	0.8484582463996709, 1.156251050939778,1);

// Chebyshev HP I, 0.2db ripple, -3db at 1
FilterCoefficients2 CabSim::HIPASS_PROTOTYPE = FilterCoefficients2(
	0, 0, 0.982613364180136,
	1.102510328053848,1.097734328563927,1);




uint64_t timeMs();

#pragma GCC diagnostic ignored "-Wreorder" 

CabSim::CabSim(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2Plugin(_rate, _bundle_path,features),
	rate(_rate),
	filterResponse(236),
	bundle_path(_bundle_path),
	programNumber(0)
{
	LogTrace("CabSim: Loading");
#ifdef JUNK

	for (int i = 0; i < 200;++i) // repeat for 20 seconds.
	{
		usleep(100000);  // sleep for 0.1 seconds
	}

#endif
	uris.Map(this);
	lv2_atom_forge_init(&forge, map);
	LogTrace("CabSim: Loadedx");
	this->highCutFilter.SetSampleRate((float)_rate);
	this->loCutFilter.SetSampleRate((float)_rate);
	this->brightFilter.SetSampleRate((float)_rate);
	this->combFilter.SetSampleRate(_rate);

	this->updateSampleDelay = (int)(_rate/MAX_UPDATES_PER_SECOND);
	this->updateMsDelay = (1000/MAX_UPDATES_PER_SECOND);
}

CabSim::~CabSim()
{

}

void CabSim::ConnectPort(uint32_t port, void* data)
{
	switch ((PortId)port) {

	case PortId::TRIM:
		this->trim.SetData(data);
		break;
	case PortId::LOCUT:
		this->loCutFilter.Frequency.SetData(data);
		break;
	case PortId::BRIGHT:
		bright.SetData(data);
		break;
	case PortId::BRIGHTF:
		brightf.SetData(data);
		break;
	case PortId::HICUT:
		highCutFilter.Frequency.SetData(data);
		break;
	case PortId::AUDIO_IN:
		this->inputL = (const float*)data;
		break;
	case PortId::AUDIO_OUT:
		this->outputL = (float*)data;
		break;
	case PortId::CONTROL_IN:
		this->controlIn = (LV2_Atom_Sequence*)data;
		break;
	case PortId::NOTIFY_OUT:
		this->notifyOut = (LV2_Atom_Sequence*)data;
		break;
	case PortId::COMB:
		this->combFilter.Comb.SetData(data);
		break;
	case PortId::COMBF:
		this->combFilter.CombF.SetData(data);
		break;

	}
}

void CabSim::Activate()
{
	LogTrace("CabSim activated.");
	
	responseChanged = true;
	frameTime = 0;
	this->loCutFilter.Reset();
	this->highCutFilter.Reset();
	this->brightFilter.Reset();
	this->combFilter.Reset();
	this->peakValueL = 0;
}
void CabSim::Deactivate()
{
	LogTrace("CabSim deactivated.");
}

void CabSim::Run(uint32_t n_samples)
{
	// prepare forge to write to notify output port.
	// Set up forge to write directly to notify output port.
	const uint32_t notify_capacity = this->notifyOut->atom.size;
	lv2_atom_forge_set_buffer(
		&(this->forge), (uint8_t*)(this->notifyOut), notify_capacity);

	// Start a sequence in the notify output port.
	LV2_Atom_Forge_Frame out_frame;

	lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.units__Frame);

	this->HandleEvents(this->controlIn);


	if (this->trim.HasChanged())
	{
		responseChanged = true;
	}

	float trim = this->trim.GetAf();

	if (highCutFilter.UpdateControls())
	{
		responseChanged = true;
	}
	if (loCutFilter.UpdateControls())
	{
		responseChanged = true;
	}
	if (combFilter.UpdateControls())
	{
		responseChanged = true;
	}

	if (this->bright.HasChanged())
	{
		brightFilter.SetLowCutDb(this->bright.GetDb());
		responseChanged = true;
	}
	if (this->brightf.HasChanged())
	{
		brightFilter.SetCutoffFrequency(brightf.GetValue());
		responseChanged = true;
	}


	for (uint32_t i = 0; i < n_samples; ++i)
	{

		float xL = Undenormalize((float)
			this->combFilter.Tick(
				this->brightFilter.Tick(
					this->highCutFilter.Tick(
						this->loCutFilter.Tick(
							trim*inputL[i]
						)))));
		float absXL = std::abs(xL);
		if (absXL > this->peakValueL)
		{
			this->peakValueL = absXL;
		}
		outputL[i] = xL;
	}
	frameTime += n_samples;

	if (responseChanged)
	{
		responseChanged = false;
		// delay by samples or ms, depending on whether we're connected.
		if (n_samples == 0)
		{
			updateMs = timeMs() + this->updateMsDelay;
		} else {
			this->updateSamples = this->updateSampleDelay;
		}
	}
    if (patchGet) {
        WriteFrequencyResponse();
        patchGet = false;
        updateSamples = 0;
        updateMs = 0;
    } else if (this->updateSamples != 0)
	{
		this->updateSamples -= n_samples;
		if (this->updateSamples <= 0 || n_samples == 0)
		{
			this->updateSamples = 0;
			WriteFrequencyResponse();
		}
	} else if (this->updateMs != 0)
	{
		uint64_t ctime = timeMs();
		if (ctime > this->updateMs || n_samples != 0)
		{
			this->updateMs = 0;
			WriteFrequencyResponse();
		}
	}
	// this->peakDelay -= n_samples;
	// if (this->peakDelay < 0)
	// {
	// 	this->peakDelay = this->updateSampleDelay;
	// 	WriteUiState();
	// 	this->peakValueL = 0;
	// }
	lv2_atom_forge_pop(&forge, &out_frame);
}


float CabSim::CalculateFrequencyResponse(float f)
{
	return highCutFilter.GetFrequencyResponse(f) 
		* brightFilter.GetFrequencyResponse(f)
		* loCutFilter.GetFrequencyResponse(f)
		* combFilter.GetFrequencyResponse(f);
}





void CabSim::WriteFrequencyResponse()
{

	for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
	{
		filterResponse.SetResponse(
			i,
			this->CalculateFrequencyResponse(
				filterResponse.GetFrequency(i)
				));
	}


	lv2_atom_forge_frame_time(&forge, frameTime);

	LV2_Atom_Forge_Frame objectFrame;
	lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);

	lv2_atom_forge_key(&forge, uris.patch__property);		
	lv2_atom_forge_urid(&forge, uris.patchProp__frequencyResponseVector);
	lv2_atom_forge_key(&forge, uris.patch__value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__float);

	lv2_atom_forge_float(&forge,30.0f);
	lv2_atom_forge_float(&forge,20000.0f);
	lv2_atom_forge_float(&forge,5.0f);
	lv2_atom_forge_float(&forge,-35.0f);


	float trimAf = this->trim.GetAf();
	for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
	{
		// lv2_atom_forge_float(&forge,filterResponse.GetFrequency(i));
		lv2_atom_forge_float(&forge,filterResponse.GetResponse(i)*trimAf);
	}
	lv2_atom_forge_pop(&forge, &vectorFrame);

	lv2_atom_forge_pop(&forge, &objectFrame);
}


void CabSim::SetProgram(uint8_t programNumber)
{
	this->programNumber = programNumber;
}


void CabSim::OnMidiCommand(int cmd0, int cmd1, int cmd2)
{
	UNUSED(cmd2);
	if (cmd0 == LV2_MIDI_MSG_PGM_CHANGE)
	{
		SetProgram(cmd1);
	}
}

void CabSim::OnPatchGet(LV2_URID propertyUrid)
{
	if (propertyUrid == uris.patchProp__frequencyResponseVector)
	{
		patchGet = true;

	}
}

void CabSim::OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *value) 
{
}
