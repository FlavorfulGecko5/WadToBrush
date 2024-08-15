#include "BinaryReader.h"
#include <fstream>
#include <vector>

BinaryReader::~BinaryReader()
{
	if(ownsBuffer)
		delete[] buffer;
}

BinaryReader::BinaryReader(const std::string& path) : ownsBuffer(true)
{
	std::vector<char> tempVector;

	std::ifstream file(path, std::ios_base::binary);
	if (file.fail())
		return;

	while (!file.eof())
	{
		char temp[2048];
		file.read(temp, 2048);
		tempVector.insert(tempVector.end(), temp, temp + file.gcount());
	}
	file.close();

	buffer = new char[tempVector.size()];
	memcpy(buffer, tempVector.data(), tempVector.size());
	length = tempVector.size();
}

BinaryReader::BinaryReader(char* p_buffer, size_t p_length) : ownsBuffer(false),
	buffer(p_buffer), length(p_length) {}

bool BinaryReader::InitSuccessful()
{
	return buffer != nullptr;
}

char* BinaryReader::GetBuffer()
{
	return buffer;
}

size_t BinaryReader::GetLength()
{
	return length;
}

size_t BinaryReader::Position() const
{
	return pos;
}

bool BinaryReader::ReachedEOF() const
{
	return pos == length;
}

void BinaryReader::Goto(const size_t newPos)
{
	if(newPos > length)
		throw IndexOOBException();
	pos = newPos;
}

void BinaryReader::GoRight(const size_t shiftAmount)
{
	if(pos + shiftAmount > length)
		throw IndexOOBException();
	pos += shiftAmount;
}

void BinaryReader::ReadBytes(char* writeTo, const size_t numBytes)
{
	if(pos + numBytes > length)
		throw IndexOOBException();

	for(size_t i = 0; i < numBytes; i++)
		writeTo[i] = buffer[pos++];
}

char* BinaryReader::ReadCString()
{
	size_t inc = pos;
	bool foundNull = false;
	
	// Determine if there's a valid c string we can read
	while (inc < length)
	{
		if (buffer[inc++] == '\0') {
			foundNull = true;
			break;
		}
	}
	if(!foundNull)
		throw IndexOOBException();

	// Copy the string into a new buffer
	size_t length = inc - pos;
	char* ptr = new char[length];
	memcpy(ptr, buffer + pos, length);
	pos = inc;

	return ptr;
}

wchar_t* BinaryReader::ReadWCStringLE()
{
	size_t inc = pos;
	bool foundNull = false;

	// Determine if there's a valid wide C string we can read
	while (inc < length)
	{
		if (buffer[inc++] == '\0') {
			if (inc < length && buffer[inc++] == '\0') {
				foundNull = true;
				break;
			}
		} else inc++;
	}
	if(!foundNull)
		throw IndexOOBException();

	// Copy the string into a new buffer
	size_t length = (inc - pos) / 2;
	wchar_t* ptr = new wchar_t[length];
	for (size_t i = 0; i < length; i++) {
		ptr[i] = buffer[pos++];
		ptr[i] += static_cast<unsigned char>(buffer[pos++]) << 8;
	}

	return ptr;
}