#include "Utilities.h"
#include <fstream>
#include <chrono>

namespace Niagara
{
	std::vector<char> ReadFile(const std::string& fileName)
	{
		// ate - start reading at the end of the file
		// The advance of starting to read at the end of the file is that we can use the read position to determine the size of the file and allocate a buffer
		std::ifstream file(fileName, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		// After that, we can seek back to the beginning of the file and read all of the bytes at once
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	/// Miscs

	double GetSystemTime()
	{
		auto now(std::chrono::system_clock::now());
		auto duration = now.time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
	}
	
}
