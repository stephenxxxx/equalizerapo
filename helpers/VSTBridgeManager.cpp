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
#include "VSTBridgeManager.h"
#include <shellapi.h>
#include <sstream>
#include "helpers/LogHelper.h"
#include "helpers/StringHelper.h"

using namespace std;

static const wchar_t* bridgeFilename = L"VSTBridge.exe";

SOCKET VSTBridgeManager::sock = INVALID_SOCKET;
BufferedSocket* VSTBridgeManager::conn = NULL;

void VSTBridgeManager::start()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock != INVALID_SOCKET)
	{
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = htons(18226);

		int ret = bind(sock, (const sockaddr*)&addr, sizeof(addr));
		if (ret == SOCKET_ERROR)
		{
			LogFStatic(L"%s", StringHelper::getSystemErrorString(WSAGetLastError()).c_str());
			return;
		}

		listen(sock, 1);

		int addrSize = sizeof(addr);
		getsockname(sock, (sockaddr*)&addr, &addrSize);

		wstringstream str;
		str << ntohs(addr.sin_port);
		LogFStatic(L"%s", str.str().c_str());

		wstring bridgePath = getBridgePath();
		ShellExecuteW(NULL, NULL, bridgePath.c_str(), str.str().c_str(), NULL, SW_SHOWDEFAULT);

		SOCKET connSocket = accept(sock, NULL, NULL);
		if (connSocket != INVALID_SOCKET)
			conn = new BufferedSocket(connSocket);
	}
}

void VSTBridgeManager::stop()
{
	if (conn != NULL)
	{
		delete conn;
		conn = NULL;
	}

	if (sock != INVALID_SOCKET)
	{
		closesocket(sock);
		sock = INVALID_SOCKET;
	}

	WSACleanup();
}

int VSTBridgeManager::loadLibrary(const std::wstring& libPath)
{
	if (conn == NULL)
		start();

	if (conn == NULL)
		return BRIDGE_INIT_FAILED;

	conn->write(LOAD_LIBRARY);
	conn->write(libPath);
	conn->flush();
	int result;
	conn->read(result);

	return result;
}

void VSTBridgeManager::unloadLibrary(int id)
{
	if (conn == NULL)
		return;

	conn->write(UNLOAD_LIBRARY);
	conn->write(id);
	conn->flush();

	int libraryCount;
	conn->read(libraryCount);
	if (libraryCount == 0)
		stop();
}

int VSTBridgeManager::createPluginInstance(int libraryId, int processLevel, int& numInputs, int& numOutputs, bool& canReplacing, int& uniqueId)
{
	if (conn == NULL)
		return -1;

	conn->write(CREATE_PLUGIN_INSTANCE);
	conn->write(libraryId);
	conn->write(processLevel);
	conn->flush();

	int pluginId;
	conn->read(pluginId);
	conn->read(numInputs);
	conn->read(numOutputs);
	conn->read(canReplacing);
	conn->read(uniqueId);

	return pluginId;
}

void VSTBridgeManager::deletePluginInstance(int pluginId)
{
	conn->write(DELETE_PLUGIN_INSTANCE);
	conn->write(pluginId);
	conn->flush();
}

void VSTBridgeManager::setUsedChannelCount(int pluginId, int count)
{
	conn->write(SET_USED_CHANNEL_COUNT);
	conn->write(pluginId);
	conn->write(count);
	conn->flush();
}

void VSTBridgeManager::prepareForProcessing(int pluginId, float sampleRate, int blockSize)
{
	conn->write(PREPARE_FOR_PROCESSING);
	conn->write(pluginId);
	conn->write(sampleRate);
	conn->write(blockSize);
	conn->flush();
}

void VSTBridgeManager::writeToEffect(int pluginId, const std::wstring& chunkData, const std::unordered_map<std::wstring, float>& paramMap)
{
	conn->write(WRITE_TO_EFFECT);
	conn->write(pluginId);
	conn->write(chunkData);
	conn->write((int)paramMap.size());
	for (auto it : paramMap)
	{
		conn->write(it.first);
		conn->write(it.second);
	}
	conn->flush();
}

void VSTBridgeManager::readFromEffect(int pluginId, std::wstring& chunkData, std::unordered_map<std::wstring, float>& paramMap)
{
	conn->write(READ_FROM_EFFECT);
	conn->write(pluginId);
	conn->flush();

	conn->read(chunkData);
	int paramCount;
	conn->read(paramCount);
	paramMap.clear();
	for (int i = 0; i < paramCount; i++)
	{
		wstring name;
		conn->read(name);
		float value;
		conn->read(value);
		paramMap[name] = value;
	}
}

void VSTBridgeManager::startProcessing(int pluginId)
{
	conn->write(START_PROCESSING);
	conn->write(pluginId);
	conn->flush();
}

void VSTBridgeManager::processReplacing(int pluginId, float** inputArray, float** outputArray, int frameCount, int numInputs, int numOutputs)
{
	conn->write(PROCESS_REPLACING);
	conn->write(pluginId);
	conn->write(frameCount);
	for (int i = 0; i < numInputs; i++)
		conn->write(inputArray[i], frameCount);
	conn->flush();
	for (int i = 0; i < numOutputs; i++)
		conn->read(outputArray[i], frameCount);
}

void VSTBridgeManager::process(int pluginId, float** inputArray, float** outputArray, int frameCount, int numInputs, int numOutputs)
{
	conn->write(PROCESS);
	conn->write(pluginId);
	conn->write(frameCount);
	for (int i = 0; i < numInputs; i++)
		for (int j = 0; j < frameCount; j++)
			conn->write(inputArray[i][j]);
	conn->flush();
	for (int i = 0; i < numOutputs; i++)
		for (int j = 0; j < frameCount; j++)
			conn->read(outputArray[i][j]);
}

void VSTBridgeManager::stopProcessing(int pluginId)
{
	conn->write(STOP_PROCESSING);
	conn->write(pluginId);
	conn->flush();
}

std::wstring VSTBridgeManager::getName(int pluginId)
{
	conn->write(GET_NAME);
	conn->write(pluginId);
	conn->flush();

	wstring name;
	conn->read(name);

	return name;
}

void VSTBridgeManager::startEditing(int pluginId, HWND hWnd, short* width, short* height)
{
	conn->write(START_EDITING);
	conn->write(pluginId);
	conn->write((int)hWnd);
	conn->flush();

	*width = 300;
	*height = 300;
	// conn->read(*width);
	// conn->read(*height);
}

void VSTBridgeManager::doIdle(int pluginId)
{
	conn->write(DO_IDLE);
	conn->write(pluginId);
	conn->flush();
}

void VSTBridgeManager::stopEditing(int pluginId)
{
	conn->write(STOP_EDITING);
	conn->write(pluginId);
	conn->flush();
}

std::wstring VSTBridgeManager::getBridgePath()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(NULL, filename, ARRAYSIZE(filename));
	PathRemoveFileSpecW(filename);
	wstring bridgePath = filename;
#ifdef _DEBUG
	bridgePath = StringHelper::replaceSuffix(bridgePath, L"\\x64\\Debug", L"\\Debug");
	bridgePath = StringHelper::replaceSuffix(bridgePath, L"64bit-Debug\\debug", L"64bit-Debug\\..\\Debug");
#else
	bridgePath = StringHelper::replaceSuffix(bridgePath, L"\\x64\\Release", L"\\Release");
	bridgePath = StringHelper::replaceSuffix(bridgePath, L"64bit-Release\\release", L"64bit-Release\\..\\Release");
#endif
	bridgePath = bridgePath + L"\\" + bridgeFilename;

	return bridgePath;
}
