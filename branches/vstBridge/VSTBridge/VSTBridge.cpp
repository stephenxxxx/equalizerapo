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
#include "VSTBridge.h"
#include <cstdlib>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include "helpers/VSTPluginLibrary.h"
#include "helpers/BufferedSocket.h"
#include "helpers/VSTBridgeManager.h"

using namespace std;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR lpCmdLine,
	_In_ int nCmdShow)
{
	int port = wcstol(lpCmdLine, NULL, 10);
	if (port <= 0)
		return 1;

	VSTBridge bridge;
	bridge.run(port);

	return 0;
}

void VSTBridge::run(int port)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock != INVALID_SOCKET)
	{
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = htons(port);

		int res = connect(sock, (const sockaddr*)&addr, sizeof(addr));
		if (res != SOCKET_ERROR)
		{
			BufferedSocket conn(sock);

			char c;
			while (conn.read(c))
			{
				switch (c)
				{
				case VSTBridgeManager::LOAD_LIBRARY:
					{
						wstring libPath;
						conn.read(libPath);
						shared_ptr<VSTPluginLibrary> library = VSTPluginLibrary::getInstance(libPath);
						res = library->initialize();
						if (res >= 0)
						{
							bool found = false;
							for (unsigned i = 0; i < libraries.size(); i++)
							{
								if (!libraries[i])
								{
									libraries[i] = library;
									res = i;
									found = true;
									break;
								}
							}

							if (!found)
							{
								libraries.push_back(library);
								res = libraries.size() - 1;
							}
							libraryCount++;
						}
						conn.write(res);
						conn.flush();
					}
					break;
				case VSTBridgeManager::UNLOAD_LIBRARY:
					{
						int id;
						conn.read(id);
						if (id >= 0 && id < (int)libraries.size())
						{
							if (libraries[id])
							{
								libraries[id].reset();
								libraryCount--;
							}
						}
						conn.write(libraryCount);
						conn.flush();
					}
					break;
				case VSTBridgeManager::CREATE_PLUGIN_INSTANCE:
					{
						int libraryId;
						conn.read(libraryId);
						int processLevel;
						conn.read(processLevel);

						int pluginId = -1;
						int numInputs = 0;
						int numOutputs = 0;
						bool canReplacing = true;
						int uniqueId = 0;
						if (libraryId >= 0 && libraryId < (int)libraries.size())
						{
							VSTBridgePluginInstance* effect = new VSTBridgePluginInstance(libraries[libraryId], processLevel);
							if (!effect->initialize())
							{
								delete effect;
							}
							else
							{
								bool found = false;
								for (unsigned i = 0; i < effects.size(); i++)
								{
									if (!effects[i])
									{
										effects[i] = effect;
										pluginId = i;
										found = true;
										break;
									}
								}

								if (!found)
								{
									effects.push_back(effect);
									pluginId = effects.size() - 1;
								}

								numInputs = effect->numInputs();
								numOutputs = effect->numOutputs();
								canReplacing = effect->canReplacing();
								uniqueId = effect->uniqueID();
							}
						}

						conn.write(pluginId);
						conn.write(numInputs);
						conn.write(numOutputs);
						conn.write(canReplacing);
						conn.write(uniqueId);
						conn.flush();
					}
					break;
				case VSTBridgeManager::DELETE_PLUGIN_INSTANCE:
					{
						int pluginId;
						conn.read(pluginId);

						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTBridgePluginInstance* effect = effects[pluginId];
							if (effect != NULL)
							{
								delete effect;
								effects[pluginId] = NULL;
							}
						}
					}
					break;
				case VSTBridgeManager::SET_USED_CHANNEL_COUNT:
					{
						int pluginId;
						conn.read(pluginId);
						int count;
						conn.read(count);

						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->setUsedChannelCount(count);
						}
					}
					break;
				case VSTBridgeManager::PREPARE_FOR_PROCESSING:
					{
						int pluginId;
						conn.read(pluginId);
						float sampleRate;
						conn.read(sampleRate);
						int blockSize;
						conn.read(blockSize);

						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->prepareForProcessing(sampleRate, blockSize);
						}
					}
					break;
				case VSTBridgeManager::WRITE_TO_EFFECT:
					{
						int pluginId;
						conn.read(pluginId);
						wstring chunkData;
						conn.read(chunkData);
						int paramCount;
						conn.read(paramCount);
						unordered_map<wstring, float> paramMap;
						for (int i = 0; i < paramCount; i++)
						{
							wstring name;
							conn.read(name);
							float value;
							conn.read(value);
							paramMap[name] = value;
						}

						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->writeToEffect(chunkData, paramMap);
						}
					}
					break;
				case VSTBridgeManager::READ_FROM_EFFECT:
					{
						int pluginId;
						conn.read(pluginId);
						wstring chunkData;
						unordered_map<wstring, float> paramMap;
						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->readFromEffect(chunkData, paramMap);
						}
						conn.write(chunkData);
						conn.write((int)paramMap.size());
						for (auto it : paramMap)
						{
							conn.write(it.first);
							conn.write(it.second);
						}
						conn.flush();
					}
					break;
				case VSTBridgeManager::START_PROCESSING:
					{
						int pluginId;
						conn.read(pluginId);

						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->startProcessing();
						}
					}
					break;
				case VSTBridgeManager::PROCESS_REPLACING:
					{
						int pluginId;
						conn.read(pluginId);

						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTBridgePluginInstance* effect = effects[pluginId];
							effect->processReplacing(conn);
						}
					}
					break;
				case VSTBridgeManager::PROCESS:
					{
						int pluginId;
						conn.read(pluginId);

						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTBridgePluginInstance* effect = effects[pluginId];
							effect->process(conn);
						}
					}
					break;
				case VSTBridgeManager::STOP_PROCESSING:
					{
						int pluginId;
						conn.read(pluginId);

						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->stopProcessing();
						}
					}
					break;
				case VSTBridgeManager::GET_NAME:
					{
						int pluginId;
						conn.read(pluginId);

						wstring name;
						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							name = effect->getName();
						}

						conn.write(name);
						conn.flush();
					}
					break;
				case VSTBridgeManager::START_EDITING:
					{
						int pluginId;
						conn.read(pluginId);
						int hWnd;
						conn.read(hWnd);
						short width;
						short height;
						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->startEditing((HWND)hWnd, &width, &height);
						}

						conn.write(width);
						conn.write(height);
						conn.flush();
					}
					break;
				case VSTBridgeManager::DO_IDLE:
					{
						int pluginId;
						conn.read(pluginId);
						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->doIdle();
						}
					}
					break;
				case VSTBridgeManager::STOP_EDITING:
					{
						int pluginId;
						conn.read(pluginId);
						if (pluginId >= 0 && pluginId < (int)effects.size())
						{
							VSTPluginInstance* effect = effects[pluginId];
							effect->stopEditing();
						}
					}
					break;
				}
			}
		}
		else
		{
			closesocket(sock);
		}
	}

	WSACleanup();
}
