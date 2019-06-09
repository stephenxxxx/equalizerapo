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
#include "VSTBridgePluginInstance.h"
#include <thread>
#include <future>

using namespace std;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

const wchar_t* registerWindowClass()
{
	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandleW(NULL);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"editingWindowClass";
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassExW(&wc))
		return NULL;

	return wc.lpszClassName;
}

const wchar_t* g_className = registerWindowClass();

VSTBridgePluginInstance::VSTBridgePluginInstance(const std::shared_ptr<VSTPluginLibrary>& library, int processLevel)
	: VSTPluginInstance(library, processLevel)
{
}

VSTBridgePluginInstance::~VSTBridgePluginInstance()
{
	cleanup();
}

void VSTBridgePluginInstance::prepareForProcessing(float sampleRate, int blockSize)
{
	cleanup();

	inputArray = new float*[numInputs()];
	for (int i = 0; i < numInputs(); i++)
		inputArray[i] = new float[blockSize];
	outputArray = new float*[numOutputs()];
	for (int i = 0; i < numOutputs(); i++)
		outputArray[i] = new float[blockSize];
}

void VSTBridgePluginInstance::processReplacing(BufferedSocket& conn)
{
	int frameCount;
	conn.read(frameCount);

	for (int i = 0; i < numInputs(); i++)
		conn.read(inputArray[i], frameCount);

	VSTPluginInstance::processReplacing(inputArray, outputArray, frameCount);

	for (int i = 0; i < numOutputs(); i++)
		conn.write(outputArray[i], frameCount);

	conn.flush();
}

void VSTBridgePluginInstance::process(BufferedSocket& conn)
{
	int frameCount;
	conn.read(frameCount);

	for (int i = 0; i < numInputs(); i++)
		for (int j = 0; j < frameCount; j++)
			conn.read(inputArray[i][j]);

	VSTPluginInstance::process(inputArray, outputArray, frameCount);

	for (int i = 0; i < numOutputs(); i++)
		for (int j = 0; j < frameCount; j++)
			conn.write(outputArray[i][j]);

	conn.flush();
}

void VSTBridgePluginInstance::startEditing(HWND hWnd, short* width, short* height)
{
	promise<void> promise;
	future<void> future = promise.get_future();
	thread(&VSTBridgePluginInstance::editWindowThread, this, hWnd, width, height, std::move(promise)).detach();
	future.wait();
}

void VSTBridgePluginInstance::stopEditing()
{
	PostMessageW(childHWnd, WM_CLOSE, 0, 0);
	childHWnd = NULL;
}

void VSTBridgePluginInstance::cleanup()
{
	if (inputArray != NULL)
	{
		for (int i = 0; i < numInputs(); i++)
			delete[] inputArray[i];
		delete[] inputArray;
		inputArray = NULL;
	}

	if (outputArray != NULL)
	{
		for (int i = 0; i < numInputs(); i++)
			delete[] outputArray[i];
		delete[] outputArray;
		outputArray = NULL;
	}
}

void VSTBridgePluginInstance::editWindowThread(HWND parentHWnd, short* width, short* height, promise<void>& promise)
{
	HINSTANCE hInstance = GetModuleHandleW(NULL);

	childHWnd = CreateWindowExW(
		0,
		g_className,
		NULL,
		WS_POPUP,
		-300, 0, 240, 120,
		NULL, NULL, hInstance, NULL);

	if (childHWnd == NULL)
	{
		MessageBox(NULL, L"Window Creation Failed!", L"Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	ShowWindow(childHWnd, SW_SHOW);
	UpdateWindow(childHWnd);

	VSTPluginInstance::startEditing(childHWnd, width, height);
	SetParent(childHWnd, parentHWnd);
	SetWindowPos(childHWnd, 0, 0, 0, *width, *height, SWP_NOZORDER);

	promise.set_value();

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
