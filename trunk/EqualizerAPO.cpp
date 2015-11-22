/*
	This file is part of EqualizerAPO, a system-wide equalizer.
	Copyright (C) 2012  Jonas Thedering

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "stdafx.h"
#include <string>
#include <Unknwn.h>
#define INITGUID
#include <mmdeviceapi.h>

#include "helpers/LogHelper.h"
#include "helpers/RegistryHelper.h"
#include "DeviceAPOInfo.h"
#include "EqualizerAPO.h"

using namespace std;

long EqualizerAPO::instCount = 0;
const CRegAPOProperties<1> EqualizerAPO::regPostMixProperties(
	EQUALIZERAPO_POST_MIX_GUID, L"EqualizerAPO", L"Copyright (C) 2015", 1, 0, __uuidof(IAudioProcessingObject),
	(APO_FLAG)( APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH | APO_FLAG_INPLACE));
const CRegAPOProperties<1> EqualizerAPO::regPreMixProperties(
	EQUALIZERAPO_PRE_MIX_GUID, L"EqualizerAPO", L"Copyright (C) 2015", 1, 0, __uuidof(IAudioProcessingObject),
	(APO_FLAG)( APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH | APO_FLAG_INPLACE));

EqualizerAPO::EqualizerAPO(IUnknown* pUnkOuter)
	:CBaseAudioProcessingObject(regPostMixProperties)
{
	refCount = 1;
	if(pUnkOuter != NULL)
		this->pUnkOuter = pUnkOuter;
	else
		this->pUnkOuter = reinterpret_cast<IUnknown*>(static_cast<INonDelegatingUnknown*>(this));

	childAPO = NULL;
	childRT = NULL;
	childCfg = NULL;

	InterlockedIncrement(&instCount);
}

EqualizerAPO::~EqualizerAPO()
{
	InterlockedDecrement(&instCount);

	resetChild();
}

HRESULT EqualizerAPO::QueryInterface(const IID& iid, void** ppv)
{
	return pUnkOuter->QueryInterface(iid, ppv);
}

ULONG EqualizerAPO::AddRef()
{
	return pUnkOuter->AddRef();
}

ULONG EqualizerAPO::Release()
{
	return pUnkOuter->Release();
}

HRESULT EqualizerAPO::GetLatency(HNSTIME* pTime)
{
    if(!pTime)
        return E_POINTER;

    if(!m_bIsLocked)
        return APOERR_ALREADY_UNLOCKED;

    *pTime = 0;
    if(childAPO)
		childAPO->GetLatency(pTime);

    return S_OK;
}

HRESULT EqualizerAPO::Initialize(UINT32 cbDataSize, BYTE* pbyData)
{
	LogHelper::reset();

    TraceF(L"Initialize");

	if((NULL == pbyData) && (0 != cbDataSize))
		return E_INVALIDARG;
	if((NULL != pbyData) && (0 == cbDataSize))
		return E_POINTER;
	if(cbDataSize != sizeof(APOInitSystemEffects) )
		return E_INVALIDARG;

	resetChild();

    APOInitSystemEffects* initStruct = (APOInitSystemEffects*)pbyData;
	GUID apoGuid = initStruct->APOInit.clsid;
	try
	{
		TraceF(L"APO GUID: %s", RegistryHelper::getGuidString(apoGuid).c_str());
	}
	catch(RegistryException e)
	{
		LogF(L"Could not convert apo guid to guid string");
	}
	engine.setPreMix((apoGuid == EQUALIZERAPO_PRE_MIX_GUID) != 0);

    PROPVARIANT var;
    PropVariantInit(&var);
	HRESULT hr = initStruct->pAPOEndpointProperties->GetValue(PKEY_AudioEndpoint_GUID, &var);
	if(FAILED(hr))
	{
		LogF(L"Can't read endpoint guid");
		return hr;
	}
	wstring deviceGuid = var.pwszVal;
	TraceF(L"Endpoint GUID: %s", deviceGuid.c_str());

	wstring childApoGuid;

	try
	{
		DeviceAPOInfo apoInfo;
		if(apoInfo.load(deviceGuid))
		{
			engine.setDeviceInfo(apoInfo.isInput, apoInfo.currentInstallState.installPostMix, apoInfo.deviceName, apoInfo.connectionName, apoInfo.deviceGuid);

			if(apoGuid == EQUALIZERAPO_PRE_MIX_GUID)
				childApoGuid = apoInfo.preMixChildGuid;
			else
				childApoGuid = apoInfo.postMixChildGuid;
		}
	}
	catch(RegistryException e)
	{
		LogF(L"Could not read endpoint device info because of: %s", e.getMessage().c_str());
	}

	TraceF(L"Child APO GUID: %s", childApoGuid.c_str());

	if(childApoGuid != L"" && childApoGuid != APOGUID_NULL && childApoGuid != APOGUID_NOKEY && childApoGuid != APOGUID_NOVALUE)
	{
		GUID childGuid;
		hr = CLSIDFromString(childApoGuid.c_str(), &childGuid);
		if(FAILED(hr))
		{
			LogF(L"Can't convert child apo guid string to guid");
			return S_OK;
		}

		hr = CoCreateInstance(childGuid, NULL, CLSCTX_INPROC_SERVER, __uuidof(IAudioProcessingObject), (void**)&childAPO);
		if(FAILED(hr))
		{
			LogF(L"Error in CoCreateInstance for child apo");
			resetChild();
			return S_OK;
		}

		hr = childAPO->QueryInterface(__uuidof(IAudioProcessingObjectRT), (void**)&childRT);
		if(FAILED(hr))
		{
			LogF(L"Error in QueryInterface for child apo RT");
			resetChild();
			return S_OK;
		}

		hr = childAPO->QueryInterface(__uuidof(IAudioProcessingObjectConfiguration), (void**)&childCfg);
		if(FAILED(hr))
		{
			LogF(L"Error in QueryInterface for child apo configuration");
			resetChild();
			return S_OK;
		}

		hr = childAPO->Initialize(cbDataSize, pbyData);
		if(FAILED(hr))
		{
			LogF(L"Error in Initialize of child apo");
			resetChild();
			return S_OK;
		}

		TraceF(L"Successfully created and initialized child APO");
	}

	return S_OK;
}

HRESULT EqualizerAPO::IsInputFormatSupported(IAudioMediaType* pOutputFormat,
		IAudioMediaType* pRequestedInputFormat, IAudioMediaType** ppSupportedInputFormat)
{
    if(!pRequestedInputFormat)
		return E_POINTER;

    UNCOMPRESSEDAUDIOFORMAT inFormat;
    HRESULT hr = pRequestedInputFormat->GetUncompressedAudioFormat(&inFormat);
	if(FAILED(hr))
	{
		LogF(L"Error in GetUncompressedAudioFormat");
		return hr;
	}

    TraceF(L"RequestedInputFormat = { %08X, %u, %u, %u, %f, %08X }",
		inFormat.guidFormatType.Data1, inFormat.dwSamplesPerFrame, inFormat.dwBytesPerSampleContainer,
		inFormat.dwValidBitsPerSample, inFormat.fFramesPerSecond, inFormat.dwChannelMask);

    UNCOMPRESSEDAUDIOFORMAT outFormat;
    hr = pOutputFormat->GetUncompressedAudioFormat(&outFormat);
	if(FAILED(hr))
	{
		LogF(L"Error in second GetUncompressedAudioFormat");
		return hr;
	}

    TraceF(L"Output format = { %08X, %u, %u, %u, %f, %08X }",
		outFormat.guidFormatType.Data1, outFormat.dwSamplesPerFrame, outFormat.dwBytesPerSampleContainer,
		outFormat.dwValidBitsPerSample, outFormat.fFramesPerSecond, outFormat.dwChannelMask);

    if(childAPO)
    {
        hr = childAPO->IsInputFormatSupported(pOutputFormat, pRequestedInputFormat, ppSupportedInputFormat);
        if(SUCCEEDED(hr))
        {
            TraceF(L"Success in IsInputFormatSupported of child apo");
        }
		else
		{
			LogF(L"Failure in IsInputFormatSupported of child apo");
			resetChild();
		}
    }

	if(!childAPO || !SUCCEEDED(hr))
	{
		hr = CBaseAudioProcessingObject::IsInputFormatSupported(pOutputFormat, pRequestedInputFormat, ppSupportedInputFormat);

		// we do not support downmixing currently
		if(hr == S_OK && inFormat.dwSamplesPerFrame > 2 && inFormat.dwSamplesPerFrame > outFormat.dwSamplesPerFrame)
		{
			CreateAudioMediaTypeFromUncompressedAudioFormat(&outFormat, ppSupportedInputFormat);
			hr = S_FALSE;
		}
	}

	if(hr == S_FALSE)
	{
		UNCOMPRESSEDAUDIOFORMAT supportedFormat;
		HRESULT hr2 = (*ppSupportedInputFormat)->GetUncompressedAudioFormat(&supportedFormat);
		if(FAILED(hr2))
		{
			LogF(L"Error in third GetUncompressedAudioFormat");
			return hr2;
		}

		TraceF(L"InputFormat not accepted, SupportedInputFormat = { %08X, %u, %u, %u, %f, %08X }",
			supportedFormat.guidFormatType.Data1, supportedFormat.dwSamplesPerFrame, supportedFormat.dwBytesPerSampleContainer,
			supportedFormat.dwValidBitsPerSample, supportedFormat.fFramesPerSecond, supportedFormat.dwChannelMask);
	}
	else if(hr == S_OK)
	{
		TraceF(L"InputFormat accepted");
	}

	return hr;
}

HRESULT EqualizerAPO::LockForProcess(UINT32 u32NumInputConnections,
		APO_CONNECTION_DESCRIPTOR** ppInputConnections, UINT32 u32NumOutputConnections,
		APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
{
	HRESULT hr;

    UNCOMPRESSEDAUDIOFORMAT inFormat;
    hr = ppInputConnections[0]->pFormat->GetUncompressedAudioFormat(&inFormat);
	if(FAILED(hr))
	{
		LogF(L"Error in GetUncompressedAudioFormat in LockForProcess");
		return hr;
	}

	unsigned maxInputFrameCount = ppInputConnections[0]->u32MaxFrameCount;

    TraceF(L"Input format in LockForProcess = { %08X, %u, %u, %u, %f, %08X, %u }",
		inFormat.guidFormatType.Data1, inFormat.dwSamplesPerFrame, inFormat.dwBytesPerSampleContainer,
		inFormat.dwValidBitsPerSample, inFormat.fFramesPerSecond, inFormat.dwChannelMask, maxInputFrameCount);

    UNCOMPRESSEDAUDIOFORMAT outFormat;
    hr = ppOutputConnections[0]->pFormat->GetUncompressedAudioFormat(&outFormat);
	if(FAILED(hr))
	{
		LogF(L"Error in second GetUncompressedAudioFormat in LockForProcess");
		return hr;
	}

	unsigned maxOutputFrameCount = ppOutputConnections[0]->u32MaxFrameCount;

    TraceF(L"Output format in LockForProcess = { %08X, %u, %u, %u, %f, %08X, %u }",
		outFormat.guidFormatType.Data1, outFormat.dwSamplesPerFrame, outFormat.dwBytesPerSampleContainer,
		outFormat.dwValidBitsPerSample, outFormat.fFramesPerSecond, outFormat.dwChannelMask, maxOutputFrameCount);

	if(childCfg != NULL)
	{
		hr = childCfg->LockForProcess(u32NumInputConnections, ppInputConnections, u32NumOutputConnections,
			ppOutputConnections);
		if(SUCCEEDED(hr))
			TraceF(L"Success in LockForProcess of child apo");
	}

    hr = CBaseAudioProcessingObject::LockForProcess(u32NumInputConnections, ppInputConnections,
		u32NumOutputConnections, ppOutputConnections);
	if(FAILED(hr))
	{
		LogF(L"Error in CBaseAudioProcessingObject::LockForProcess");
		return hr;
	}
	else if(hr == S_OK)
	{
		TraceF(L"LockForProcess successful");
	}

	unsigned maxFrameCount = maxInputFrameCount;
	if(maxFrameCount == 0)
		maxFrameCount = maxOutputFrameCount;

	unsigned realChannelCount;
	if(childCfg != NULL)
		realChannelCount = outFormat.dwSamplesPerFrame;
	else
		realChannelCount = inFormat.dwSamplesPerFrame;

	unsigned channelMask;
	if(engine.isCapture())
	{
		channelMask = inFormat.dwChannelMask;
		if(channelMask == 0 && inFormat.dwSamplesPerFrame == outFormat.dwSamplesPerFrame)
			channelMask = outFormat.dwChannelMask;
	}
	else
	{
		channelMask = outFormat.dwChannelMask;
		if(channelMask == 0 && inFormat.dwSamplesPerFrame == outFormat.dwSamplesPerFrame)
			channelMask = inFormat.dwChannelMask;
	}

	engine.initialize(outFormat.fFramesPerSecond, inFormat.dwSamplesPerFrame, realChannelCount, outFormat.dwSamplesPerFrame, channelMask, maxFrameCount);

    return hr;
}

HRESULT EqualizerAPO::UnlockForProcess()
{
	if(childCfg)
	{
		HRESULT hr = childCfg->UnlockForProcess();
		if(FAILED(hr))
		{
			LogF(L"Error in UnlockForProcess");
			return hr;
		}
	}

    return CBaseAudioProcessingObject::UnlockForProcess();
}

void EqualizerAPO::resetChild()
{
	if(childAPO != NULL)
	{
		childAPO->Release();
		childAPO = NULL;
	}

	if(childRT != NULL)
	{
		childRT->Release();
		childRT = NULL;
	}

	if(childCfg != NULL)
	{
		childCfg->Release();
		childCfg = NULL;
	}
}

#pragma AVRT_CODE_BEGIN
void EqualizerAPO::APOProcess(UINT32 u32NumInputConnections,
	APO_CONNECTION_PROPERTY** ppInputConnections, UINT32 u32NumOutputConnections,
	APO_CONNECTION_PROPERTY** ppOutputConnections)
{
    switch( ppInputConnections[0]->u32BufferFlags )
    {
        case BUFFER_VALID:
		case BUFFER_SILENT:
        {
            float* inputFrames = reinterpret_cast<float*>(ppInputConnections[0]->pBuffer);
            float* outputFrames = reinterpret_cast<float*>(ppOutputConnections[0]->pBuffer);

			if(ppInputConnections[0]->u32BufferFlags == BUFFER_SILENT)
				memset(inputFrames, 0, ppInputConnections[0]->u32ValidFrameCount * engine.getInputChannelCount() * sizeof(float));

            if(childRT)
            {
                childRT->APOProcess(u32NumInputConnections, ppInputConnections, u32NumOutputConnections, ppOutputConnections);

				engine.process(outputFrames, outputFrames, ppInputConnections[0]->u32ValidFrameCount);
            }
            else
                engine.process(outputFrames, inputFrames, ppInputConnections[0]->u32ValidFrameCount);

            ppOutputConnections[0]->u32ValidFrameCount = ppInputConnections[0]->u32ValidFrameCount;
			ppOutputConnections[0]->u32BufferFlags = BUFFER_VALID;

            break;
        }
    }
}
#pragma AVRT_CODE_END

HRESULT EqualizerAPO::NonDelegatingQueryInterface(const IID& iid, void** ppv)
{
	if(iid == __uuidof(IUnknown))
		*ppv = static_cast<INonDelegatingUnknown*>(this);
	else if(iid == __uuidof(IAudioProcessingObject))
		*ppv = static_cast<IAudioProcessingObject*>(this);
	else if(iid == __uuidof(IAudioProcessingObjectRT))
		*ppv = static_cast<IAudioProcessingObjectRT*>(this);
	else if(iid == __uuidof(IAudioProcessingObjectConfiguration))
		*ppv = static_cast<IAudioProcessingObjectConfiguration*>(this);
	else if(iid == __uuidof(IAudioSystemEffects))
		*ppv = static_cast<IAudioSystemEffects*>(this);
	else
	{
		*ppv = NULL;
		return E_NOINTERFACE;
	}

	reinterpret_cast<IUnknown*>(*ppv)->AddRef();
	return S_OK;
}

ULONG EqualizerAPO::NonDelegatingAddRef()
{
	return InterlockedIncrement(&refCount);
}

ULONG EqualizerAPO::NonDelegatingRelease()
{
	if(InterlockedDecrement(&refCount) == 0)
	{
		delete this;
		return 0;
	}

	return refCount;
}