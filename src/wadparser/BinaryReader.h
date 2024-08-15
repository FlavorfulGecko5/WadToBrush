#include <string>

class IndexOOBException : public std::exception {};

class BinaryReader
{
	private:
	bool ownsBuffer;
	char* buffer = nullptr;
	size_t length = 0;
	size_t pos = 0;

	public:
	~BinaryReader();
	BinaryReader(const std::string& path);
	BinaryReader(char* p_buffer, size_t p_length);
	bool InitSuccessful();
	char* GetBuffer();
	size_t GetLength();

	// =====
	// Reading
	// =====
	size_t Position() const;
	bool ReachedEOF() const;
	void Goto(const size_t newPos);
	void GoRight(const size_t shiftAmount);
	void ReadBytes(char* writeTo, const size_t numBytes);

	char* ReadCString();
	wchar_t* ReadWCStringLE();

	template<typename T>
	void ReadLE(T& readTo)
	{
		// Determine number of bytes to read
		constexpr size_t numBytes = sizeof(T);
		if (pos + numBytes > length)
			throw IndexOOBException();

		// Read the bytes into the value
		T value = 0;
		for (size_t i = 0, max = numBytes * 8; i < max; i += 8) {
			T temp = static_cast<unsigned char>(buffer[pos++]);

			// Literal expression overflows for > 32 bits if we don't shift on a temp variable
			// This ensures it works for 64 bits. TODO: research and find a better solution
			temp = temp << i; 
			value += temp;
		}

		readTo = value;
	}
};