/*
    This file is part of Equalizer APO, a system-wide equalizer.
    Copyright (C) 2017  Jonas Thedering

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
#include "VSTPluginLibrary.h"
#include "VSTBridgeManager.h"
#include "RemoteVSTPluginInstance.h"

RemoteVSTPluginInstance::RemoteVSTPluginInstance(const std::shared_ptr<VSTPluginLibrary>& library, int processLevel)
	: VSTPluginInstance(library, processLevel)
{
}

RemoteVSTPluginInstance::~RemoteVSTPluginInstance()
{
	if (pluginId != -1)
		VSTBridgeManager::deletePluginInstance(pluginId);
}

bool RemoteVSTPluginInstance::initialize()
{
	int libraryId = library->getLibraryId();
	pluginId = VSTBridgeManager::createPluginInstance(libraryId, getProcessLevel(), bridgeNumInputs, bridgeNumOutputs, bridgeCanReplacing, bridgeUniqueId);
	if (pluginId != -1)
		VSTPluginInstance::setUsedChannelCount(max(numInputs(), numOutputs()));

	return pluginId != -1;
}

int RemoteVSTPluginInstance::numInputs() const
{
	return bridgeNumInputs;
}

int RemoteVSTPluginInstance::numOutputs() const
{
	return bridgeNumOutputs;
}

bool RemoteVSTPluginInstance::canReplacing() const
{
	return bridgeCanReplacing;
}

int RemoteVSTPluginInstance::uniqueID() const
{
	return bridgeUniqueId;
}

std::wstring RemoteVSTPluginInstance::getName() const
{
	return VSTBridgeManager::getName(pluginId);
}

void RemoteVSTPluginInstance::setUsedChannelCount(int count)
{
	VSTBridgeManager::setUsedChannelCount(pluginId, count);

	VSTPluginInstance::setUsedChannelCount(count);
}

void RemoteVSTPluginInstance::prepareForProcessing(float sampleRate, int blockSize)
{
	VSTBridgeManager::prepareForProcessing(pluginId, sampleRate, blockSize);

	VSTPluginInstance::prepareForProcessing(sampleRate, blockSize);
}

void RemoteVSTPluginInstance::writeToEffect(const std::wstring& chunkData, const std::unordered_map<std::wstring, float>& paramMap)
{
	VSTBridgeManager::writeToEffect(pluginId, chunkData, paramMap);
}

void RemoteVSTPluginInstance::readFromEffect(std::wstring& chunkData, std::unordered_map<std::wstring, float>& paramMap) const
{
	VSTBridgeManager::readFromEffect(pluginId, chunkData, paramMap);
}

void RemoteVSTPluginInstance::startProcessing()
{
	VSTBridgeManager::startProcessing(pluginId);
}

void RemoteVSTPluginInstance::processReplacing(float** inputArray, float** outputArray, int frameCount)
{
	VSTBridgeManager::processReplacing(pluginId, inputArray, outputArray, frameCount, numInputs(), numOutputs());
}

void RemoteVSTPluginInstance::process(float** inputArray, float** outputArray, int frameCount)
{
	VSTBridgeManager::process(pluginId, inputArray, outputArray, frameCount, numInputs(), numOutputs());
}

void RemoteVSTPluginInstance::stopProcessing()
{
	VSTBridgeManager::stopProcessing(pluginId);
}

void RemoteVSTPluginInstance::startEditing(HWND hWnd, short* width, short* height)
{
	VSTBridgeManager::startEditing(pluginId, hWnd, width, height);
}

void RemoteVSTPluginInstance::doIdle()
{
	VSTBridgeManager::doIdle(pluginId);
}

void RemoteVSTPluginInstance::stopEditing()
{
	VSTBridgeManager::stopEditing(pluginId);
}
