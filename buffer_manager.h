#pragma once
#include <vector>
#include "glad/glad.h"

class BufferGroup
{
public:
	void init(std::vector<GLuint> buffers, const std::vector<int>& dataTypeSizes, int bufferLength)
	{
		this->BUFFER_COUNT = static_cast<int>(dataTypeSizes.size());
		this->bufferLength = bufferLength;
		this->buffers = buffers;
		this->dataTypeSizes = dataTypeSizes;
	}

	void create()
	{
		glCreateBuffers(BUFFER_COUNT, buffers.data());
		for (int i{ 0 }; i < BUFFER_COUNT; i++)
		{
			glNamedBufferData(
				buffers[i],
				bufferLength * dataTypeSizes[i],
				nullptr,
				GL_DYNAMIC_DRAW);
		}
	}

	void bindBase(int index)
	{
		for (int i{ 0 }; i < BUFFER_COUNT; i++)
		{
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, buffers[i]);
		}
	}

	void update(int index, int length, const std::vector<void*>& data)
	{
		for (int i = 0; i < BUFFER_COUNT; ++i)
		{
			glNamedBufferSubData(
				buffers[i],
				index * dataTypeSizes[i],
				length * dataTypeSizes[i],
				data[i]
			);
		}
	}

	void cleanup()
	{
		glDeleteBuffers(BUFFER_COUNT, buffers.data());
	}

private:
	int BUFFER_COUNT;
	int bufferLength;
	std::vector<GLuint> buffers;
	std::vector<int> dataTypeSizes;
};