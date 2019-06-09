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

class BufferedSocket
{
public:
	BufferedSocket(SOCKET conn, unsigned capacity = 1024);
	~BufferedSocket();

	void write(char c);
	void write(const char* buf, unsigned count);
	void write(bool b);
	void write(short s);
	void write(int i);
	void write(float f);
	void write(const float* f, unsigned count);
	void write(const std::wstring& s);
	bool read(char& c);
	bool read(char* buf, unsigned count);
	bool read(bool& b);
	bool read(short& i);
	bool read(int& i);
	bool read(float& f);
	bool read(float* f, unsigned count);
	bool read(std::wstring& s);
	void flush();

private:
	SOCKET conn;
	char* writeBuf;
	unsigned written = 0;
	char* readBuf;
	int readBytes = 0;
	int readOffset = 0;
	unsigned capacity;
};
