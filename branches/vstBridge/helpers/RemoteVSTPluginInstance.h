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

#pragma once

#include "VSTPluginInstance.h"

class RemoteVSTPluginInstance : public VSTPluginInstance
{
public:
	RemoteVSTPluginInstance(const std::shared_ptr<VSTPluginLibrary>& library, int processLevel);
	~RemoteVSTPluginInstance() override;

	bool initialize() override;

	int numInputs() const override;
	int numOutputs() const override;
	bool canReplacing() const override;
	int uniqueID() const override;
	std::wstring getName() const override;
	void setUsedChannelCount(int count) override;

	void prepareForProcessing(float sampleRate, int blockSize) override;
	void writeToEffect(const std::wstring& chunkData, const std::unordered_map<std::wstring, float>& paramMap) override;
	void readFromEffect(std::wstring& chunkData, std::unordered_map<std::wstring, float>& paramMap) const override;

	void startProcessing() override;
	void processReplacing(float** inputArray, float** outputArray, int frameCount) override;
	void process(float** inputArray, float** outputArray, int frameCount) override;
	void stopProcessing() override;

	void startEditing(HWND hWnd, short* width, short* height) override;
	void doIdle() override;
	void stopEditing() override;

private:
	int pluginId = -1;
	int bridgeNumInputs = 0;
	int bridgeNumOutputs = 0;
	bool bridgeCanReplacing = true;
	int bridgeUniqueId = 0;
};
