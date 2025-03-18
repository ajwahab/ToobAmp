﻿/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:

 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.

 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */
// ToobML.h

#pragma once

#include "std.h"

#include "lv2/core/lv2.h"
#include "lv2/log/logger.h"
#include "lv2/uri-map/uri-map.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/worker/worker.h"
#include "lv2/patch/patch.h"
#include "lv2/parameters/parameters.h"
#include "lv2/units/units.h"
#include "iir/ChebyshevI.h"
#include "FilterResponse.h"
#include <string>

#include <lv2_plugin/Lv2Plugin.hpp>

#include "MidiProcessor.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "ControlDezipper.h"
#include "LsNumerics/BaxandallToneStack.hpp"
#include "SagProcessor.h"

#define TOOB_ML_URI "http://two-play.com/plugins/toob-ml"
#ifndef TOOB_URI
#define TOOB_URI "http://two-play.com/plugins/toob"
#endif

namespace toob
{

	class ToobMlModel
	{
	protected:
		ToobMlModel() {};

	public:
		virtual ~ToobMlModel() {}
		static ToobMlModel *Load(const std::string &fileName);
		virtual void Reset() = 0;
		virtual void Process(int numSamples, const float *input, float *output, float param, float param2) = 0;
		virtual float Process(float input, float param, float param2) = 0;
		virtual bool IsGainEnabled() const = 0;
	};

	class ToobML : public Lv2PluginWithState
	{
	public:
		using super = Lv2PluginWithState;
	private:
		enum class PortId
		{
			TRIM = 0,
			TRIM_OUT,
			AMP_MODEL,
			GAIN,
			MASTER,
			BASS,
			MID,
			TREBLE,

			SAG,
			SAGD,
			SAGF,

			GAIN_ENABLE,

			AUDIO_IN,
			AUDIO_OUT,
			CONTROL_IN,
			NOTIFY_OUT,

		};

		double rate;
		std::string bundle_path;

		const float *input = nullptr;
		const float *trimData = nullptr;
		float *trimOutData = nullptr;
		const float *gainData = nullptr;
		const float *masterData = nullptr;
		const float *modelData = nullptr;
		const float *bassData = nullptr;
		const float *midData = nullptr;
		const float *trebleData = nullptr;
		float *gainEnableData = nullptr;

		float modelValue;
		float gainValue = 0;
		float trimOutValue = -96;
		float trimDb = 0;
		float masterDb = 0;
		float bassValue = 0;
		float midValue = 0;
		float trebleValue = 0;
		float gainEnable = 1;

		float trim = 1;
		float master = 1;
		float gain = 0;

		int trimOutputCount = 0;
		int trimOutputSampleRate = 10000;

		SagProcessor sagProcessor;

		LsNumerics::BaxandallToneStack baxandallToneStack;
		bool bypassToneFilter = false;
		void UpdateFilter();
		ToobMlModel *pCurrentModel = nullptr;
		std::vector<std::string> modelFiles;
		Iir::ChebyshevI::HighPass<3> dcBlocker;

		void LoadModelIndex();
		ToobMlModel *LoadModel(const std::string &fileName);

		float *output = nullptr;

		LV2_Atom_Sequence *controlIn = nullptr;
		LV2_Atom_Sequence *notifyOut = nullptr;
		uint64_t frameTime = 0;

		bool responseChanged = true;
		bool responsePatchGetRequested = false;
		bool modelPatchGetRequested = false;
		int64_t updateSampleDelay;
		uint64_t updateMsDelay;

		int64_t updateSamples = 0;
		uint64_t updateMs = 0;

		int programNumber;

		struct Urids
		{
		public:
			void Map(Lv2Plugin *plugin)
			{
				pluginUri = plugin->MapURI(TOOB_ML_URI);

				atom__Path = plugin->MapURI(LV2_ATOM__Path);
				atom__String = plugin->MapURI(LV2_ATOM__String);
				atom__Float = plugin->MapURI(LV2_ATOM__Float);
				atom_Int = plugin->MapURI(LV2_ATOM__Int);
				atom_Sequence = plugin->MapURI(LV2_ATOM__Sequence);
				atom__URID = plugin->MapURI(LV2_ATOM__URID);
				atom_eventTransfer = plugin->MapURI(LV2_ATOM__eventTransfer);
				patch__Get = plugin->MapURI(LV2_PATCH__Get);
				patch__Set = plugin->MapURI(LV2_PATCH__Set);
				patch_Put = plugin->MapURI(LV2_PATCH__Put);
				patch_body = plugin->MapURI(LV2_PATCH__body);
				patch_subject = plugin->MapURI(LV2_PATCH__subject);
				patch__property = plugin->MapURI(LV2_PATCH__property);
				patch_accept = plugin->MapURI(LV2_PATCH__accept);
				patch__value = plugin->MapURI(LV2_PATCH__value);
				param_gain = plugin->MapURI(LV2_PARAMETERS__gain);
				units__Frame = plugin->MapURI(LV2_UNITS__frame);
				property__frequencyResponseVector = plugin->MapURI(TOOB_URI "#frequencyResponseVector");
				param_uiState = plugin->MapURI(TOOB_ML_URI "#uiState");
				ml__modelFile = plugin->MapURI("http://two-play.com/plugins/toob-ml#modelFile");
				ml__version = plugin->MapURI("http://two-play.com/plugins/toob-ml#version");
			}
			LV2_URID ml__modelFile;
			LV2_URID ml__version;
			LV2_URID patch_accept;

			LV2_URID frequencyResponseUri;
			LV2_URID units__Frame;
			LV2_URID pluginUri;
			LV2_URID atom__Float;
			LV2_URID atom_Int;
			LV2_URID atom__String;
			LV2_URID atom__Path;
			LV2_URID atom_Sequence;
			LV2_URID atom__URID;
			LV2_URID atom_eventTransfer;
			LV2_URID midi_Event;
			LV2_URID patch__Get;
			LV2_URID patch__Set;
			LV2_URID patch_Put;
			LV2_URID patch_body;
			LV2_URID patch_subject;
			LV2_URID patch__property;
			LV2_URID patch__value;
			LV2_URID param_gain;
			LV2_URID property__frequencyResponseVector;
			LV2_URID param_uiState;
		};

		Urids urids;

		FilterResponse filterResponse;

	protected:
		virtual void OnPatchGet(LV2_URID propertyUrid) override;
		virtual void OnPatchSet(LV2_URID propertyUrid, const LV2_Atom *atom) override;

	private:
		int version = 0;
		std::string currentModel;
		bool modelChanged = true;

		void SetModel(const char *model);
		ControlDezipper trimDezipper;
		ControlDezipper gainDezipper;
		ControlDezipper masterDezipper;

		enum class AsyncState
		{
			Idle,
			Loading,
			Loaded,
			Deleting,
		};
		AsyncState asyncState = AsyncState::Idle;

		ToobMlModel *pPendingLoad = nullptr;

		class LoadWorker : public WorkerAction
		{
		private:
			ToobML *pThis = nullptr;
			std::string modelFileName;
			std::string requestFileName;
			ToobMlModel *pModelResult = nullptr;

		public:
			LoadWorker(ToobML *pThis)
				: WorkerAction(pThis),
				  pThis(pThis)
			{
				modelFileName.reserve(1024);
				requestFileName.reserve(1024);
			}
			virtual ~LoadWorker();

			const std::string &GetFileName() const
			{
				return this->modelFileName;
			}
			bool SetFileName(const char *szFileName)
			{
				if (strcmp(szFileName, modelFileName.c_str()) != 0)
				{
					this->modelFileName = szFileName;
					return true;
				}
				return false;
				this->WorkerAction::Request();
			}
			void StartRequest()
			{
				this->requestFileName = modelFileName;
				this->WorkerAction::Request();
			}

		protected:
			void OnWork()
			{
				this->pModelResult = pThis->LoadModel(this->requestFileName);
			}
			void OnResponse()
			{
				pThis->AsyncLoadComplete(requestFileName, pModelResult);
				pModelResult = nullptr;
			}
		};

		LoadWorker loadWorker;

		class DeleteWorker : public WorkerAction
		{
		private:
			ToobML *pThis = nullptr;
			ToobMlModel *pModel = nullptr;

		public:
			DeleteWorker(ToobML *pThis)
				: WorkerAction(pThis),
				  pThis(pThis)
			{
			}
			virtual ~DeleteWorker();

			void Request(ToobMlModel *pModel)
			{
				this->pModel = pModel;
				this->WorkerAction::Request();
			}

		protected:
			void OnWork();

			void OnResponse()
			{
				pThis->AsyncDeleteComplete();
			}
		};

		DeleteWorker deleteWorker;

		void AsyncLoadComplete(const std::string &modelPath, ToobMlModel *pNewModel);
		void HandleAsyncLoad();
		void AsyncDelete(ToobMlModel *pNewModel);
		void AsyncDeleteComplete();

		float CalculateFrequencyResponse(float f);

		void SetProgram(uint8_t programNumber);
		LV2_Atom_Forge_Ref WriteFrequencyResponse();

	protected:
		double getRate() { return rate; }
		std::string getBundlePath() { return bundle_path.c_str(); }

	public:
		void OnMidiCommand(int cmd0, int cmd1, int cmd2);

	public:
		static Lv2Plugin *Create(double rate,
								 const char *bundle_path,
								 const LV2_Feature *const *features)
		{
			return new ToobML(rate, bundle_path, features);
		}

		ToobML(double rate,
			   const char *bundle_path,
			   const LV2_Feature *const *features);
		virtual ~ToobML();

	public:
		static const char *URI;

	protected:
		virtual void ConnectPort(uint32_t port, void *data) override;
		virtual void Activate() override;
		virtual void Run(uint32_t n_samples) override;
		virtual void Deactivate() override;
		virtual LV2_State_Status OnSaveLv2State(
			LV2_State_Store_Function store,
			LV2_State_Handle handle,
			uint32_t flags,
			const LV2_Feature *const *features) override;
		virtual LV2_State_Status OnRestoreLv2State(
			LV2_State_Retrieve_Function retrieve,
			LV2_State_Handle handle,
			uint32_t flags,
			const LV2_Feature *const *features) override;

	private:
		void LegacyLoad(size_t patchNumber);
		std::string MapFilename(
			const LV2_Feature *const *features,
			const std::string &input);

		void SaveLv2Filename(
			LV2_State_Store_Function store,
			LV2_State_Handle handle,
			const LV2_Feature *const *features,
			LV2_URID urid,
			const std::string &filename_);
		std::string UnmapFilename(const LV2_Feature *const *features, const std::string &fileName);
	};
}
