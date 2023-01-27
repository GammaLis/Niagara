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

	/// Math

	static glm::vec4 NormalizePlane(const glm::vec4& plane)
	{
		float len = length(glm::vec3(plane));
		return plane / (len < 1e-3 ? 1 : len);
	}

	void GetFrustumPlanes(glm::vec4 frustumPlanes[], const glm::mat4& matrix, bool reversedZ)
	{
		glm::mat4 mt = glm::transpose(matrix);

		frustumPlanes[0] = NormalizePlane(-mt[0] - mt[3]);
		frustumPlanes[1] = NormalizePlane(+mt[0] - mt[3]);
		frustumPlanes[2] = NormalizePlane(-mt[1] - mt[3]);
		frustumPlanes[3] = NormalizePlane(+mt[1] - mt[3]);

		if (reversedZ)
		{
			frustumPlanes[4] = NormalizePlane(+ mt[2] - mt[3]);
			frustumPlanes[5] = NormalizePlane(- mt[2]);
		}
		else
		{
			frustumPlanes[4] = NormalizePlane(-mt[2]);
			frustumPlanes[5] = NormalizePlane(+mt[2] - mt[3]);
		}		
	}

	/// Miscs

	double GetSystemTime()
	{
		auto now(std::chrono::system_clock::now());
		auto duration = now.time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
	}
	
}
