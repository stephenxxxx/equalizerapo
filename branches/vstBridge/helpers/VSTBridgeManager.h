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

#include <string>
#include <winsock2.h>
#include "BufferedSocket.h"

class VSTBridgeManager
{
public:
	static const int BRIDGE_INIT_FAILED = -1000;

	static const char LOAD_LIBRARY = 'L';
	static const char UNLOAD_LIBRARY = 'U';
	static const char CREATE_PLUGIN_INSTANCE = 'C';
	static const char DELETE_PLUGIN_INSTANCE = 'D';
	static const char SET_USED_CHANNEL_COUNT = 'H';
	static const char PREPARE_FOR_PROCESSING = 'P';
	static const char WRITE_TO_EFFECT = 'W';
	static const char READ_FROM_EFFECT = 'A';
	static const char START_PROCESSING = 'S';
	static const char PROCESS_REPLACING = 'O';
	static const char PROCESS = 'R';
	static const char STOP_PROCESSING = 'T';
	static const char GET_NAME = 'G';
	static const char START_EDITING = 'E';
	static const char DO_IDLE = 'I';
	static const char STOP_EDITING = 'e';

	static void start();
	static void stop();
	static int loadLibrary(const std::wstring& libPath);
	static void unloadLibrary(int id);
	static int createPluginInstance(int libraryId, int processLevel, int& numInputs, int& numOutputs, bool& canReplacing, int& uniqueId);
	static void deletePluginInstance(int pluginId);
	static void setUsedChannelCount(int pluginId, int count);
	static void prepareForProcessing(int pluginId, float sampleRate, int blockSize);
	static void writeToEffect(int pluginId, const std::wstring& chunkData, const std::unordered_map<std::wstring, float>& paramMap);
	static void readFromEffect(int pluginId, std::wstring& chunkData, std::unordered_map<std::wstring, float>& paramMap);
	static void startProcessing(int pluginId);
	static void processReplacing(int pluginId, float** inputArray, float** outputArray, int frameCount, int numInputs, int numOutputs);
	static void process(int pluginId, float** inputArray, float** outputArray, int frameCount, int numInputs, int numOutputs);
	static void stopProcessing(int pluginId);
	static std::wstring getName(int pluginId);
	static void startEditing(int pluginId, HWND hWnd, short* width, short* height);
	static void doIdle(int pluginId);
	static void stopEditing(int pluginId);

private:
	static std::wstring getBridgePath();
	static SOCKET sock;
	static BufferedSocket* conn;
};
