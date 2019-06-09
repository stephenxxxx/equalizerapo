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
#include "BufferedSocket.h"
#include "StringHelper.h"

using namespace std;

BufferedSocket::BufferedSocket(SOCKET conn, unsigned capacity)
	: conn(conn), capacity(capacity)
{
	writeBuf = new char[capacity];
	readBuf = new char[capacity];
}

BufferedSocket::~BufferedSocket()
{
	delete[] readBuf;
	delete[] writeBuf;
	if (conn != INVALID_SOCKET)
		closesocket(conn);
}

void BufferedSocket::write(char c)
{
	if (written >= capacity)
		flush();
	writeBuf[written++] = c;
}

void BufferedSocket::write(const char* buf, unsigned count)
{
	flush();
	send(conn, buf, count, 0);
}

void BufferedSocket::write(bool b)
{
	write(b ? '\1' : '\0');
}

void BufferedSocket::write(short s)
{
	char* array = (char*)&s;
	for (int n = 0; n < 2; n++)
		write(array[n]);
}

void BufferedSocket::write(int i)
{
	char* array = (char*)&i;
	for (int n = 0; n < 4; n++)
		write(array[n]);
}

void BufferedSocket::write(float f)
{
	char* array = (char*)&f;
	for (int n = 0; n < 4; n++)
		write(array[n]);
}

void BufferedSocket::write(const float* f, unsigned count)
{
	write((const char*)f, count * sizeof(float));
}

void BufferedSocket::write(const std::wstring& s)
{
	string str = StringHelper::toString(s, CP_UTF8);
	for (unsigned i = 0; i < str.length(); i++)
		write(str.at(i));
	write('\0');
}

bool BufferedSocket::read(char& c)
{
	if (readOffset >= readBytes)
	{
		readBytes = recv(conn, readBuf, capacity, 0);
		readOffset = 0;

		if (readBytes <= 0)
		{
			c = '\0';
			return false;
		}
	}

	c = readBuf[readOffset++];
	return true;
}

bool BufferedSocket::read(char* buf, unsigned count)
{
	int bytesRemaining = readBytes - readOffset;
	if (bytesRemaining > 0)
	{
		int bytesToCopy = max(0, min((int)count, bytesRemaining));
		memcpy(buf, readBuf + readOffset, bytesToCopy);
		readOffset += bytesToCopy;
		buf += bytesToCopy;
		count -= bytesToCopy;
	}

	// avoid copying for large read
	while (count > 100)
	{
		int bytes = recv(conn, buf, count, 0);
		if (bytes <= 0)
		{
			memset(buf, 0, count);
			return false;
		}
		buf += bytes;
		count -= bytes;
	}

	while (count > 0)
	{
		if (readBytes >= readOffset)
		{
			readBytes = recv(conn, readBuf, capacity, 0);
			readOffset = 0;

			if (readBytes <= 0)
			{
				memset(buf, 0, count);
				return false;
			}
		}
		bytesRemaining = readBytes - readOffset;
		if (bytesRemaining > 0)
		{
			int bytesToCopy = max(0, min((int)count, bytesRemaining));
			memcpy(buf, readBuf + readOffset, bytesToCopy);
			readOffset += bytesToCopy;
			buf += bytesToCopy;
			count -= bytesToCopy;
		}
	}

	return true;
}

bool BufferedSocket::read(bool& b)
{
	char c;
	bool result = read(c);
	b = (c != '\0');

	return result;
}

bool BufferedSocket::read(short& i)
{
	char* array = (char*)&i;
	for (int n = 0; n < 2; n++)
	{
		bool result = read(array[n]);
		if (!result)
		{
			i = 0;
			return false;
		}
	}

	return true;
}

bool BufferedSocket::read(int& i)
{
	char* array = (char*)&i;
	for (int n = 0; n < 4; n++)
	{
		bool result = read(array[n]);
		if (!result)
		{
			i = 0;
			return false;
		}
	}

	return true;
}

bool BufferedSocket::read(float& f)
{
	char* array = (char*)&f;
	for (int n = 0; n < 4; n++)
	{
		bool result = read(array[n]);
		if (!result)
		{
			f = 0;
			return false;
		}
	}

	return true;
}

bool BufferedSocket::read(float* f, unsigned count)
{
	return read((char*)f, count * sizeof(float));
}

bool BufferedSocket::read(std::wstring& s)
{
	bool result = true;

	string str;
	char c;
	while (result = read(c))
	{
		if (c == '\0')
			break;
		str.push_back(c);
	}

	s = StringHelper::toWString(str, CP_UTF8);

	return result;
}

void BufferedSocket::flush()
{
	if (written > 0)
	{
		send(conn, writeBuf, written, 0);
		written = 0;
	}
}
