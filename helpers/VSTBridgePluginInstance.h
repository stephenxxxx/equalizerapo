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

#include <memory>
#include "VSTPluginInstance.h"
#include "helpers/BufferedSocket.h"
#include <future>

class VSTBridgePluginInstance : public VSTPluginInstance
{
public:
	VSTBridgePluginInstance(const std::shared_ptr<VSTPluginLibrary>& library, int processLevel);
	~VSTBridgePluginInstance() override;
	void prepareForProcessing(float sampleRate, int blockSize) override;
	void processReplacing(BufferedSocket& conn);
	void process(BufferedSocket& conn);
	void startEditing(HWND hWnd, short* width, short* height) override;
	void stopEditing() override;

private:
	void cleanup();
	void editWindowThread(::HWND__* parentHWnd, short* width, short* height, std::promise<void>& promise);

	float** inputArray = NULL;
	float** outputArray = NULL;
	HWND parentHWnd = NULL;
	HWND childHWnd = NULL;
	short width = 0;
	short height = 0;
};
