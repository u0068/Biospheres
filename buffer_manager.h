#pragma once
#include <cassert>
#include <vector>
#include "glad/glad.h"

struct BufferGroup
{
public:
	int BUFFER_COUNT;
	int bufferLength;
	std::vector<std::reference_wrapper<GLuint>> buffers;
	std::vector<int> dataTypeSizes;
	bool createAndDestroy;

	BufferGroup() = default;
	~BufferGroup()
	{
		if (createAndDestroy)
		{
			cleanup();
		}
	}

	void init(std::vector<std::reference_wrapper<GLuint>> buffers, const std::vector<int>& dataTypeSizes, int bufferLength, bool createAndDestroy = false)
	{
		this->BUFFER_COUNT = static_cast<int>(dataTypeSizes.size());
		this->bufferLength = bufferLength;
		this->buffers = buffers;
		this->dataTypeSizes = dataTypeSizes;
		this->createAndDestroy = createAndDestroy;

		if (createAndDestroy)
		{
			create();
		}
	}

	void create()
	{
		std::vector<GLuint> bufferIds(BUFFER_COUNT);
		glCreateBuffers(BUFFER_COUNT, bufferIds.data());
		for (int i = 0; i < BUFFER_COUNT; ++i)
		{
			buffers[i].get() = bufferIds[i]; // update original GLuint variables
			glNamedBufferData(
				bufferIds[i],
				bufferLength * dataTypeSizes[i],
				nullptr,
				GL_DYNAMIC_DRAW);
		}
	}

	void bindBase(const std::vector<int>& indices)
	{
		assert(indices.size() == BUFFER_COUNT);
		for (int i{ 0 }; i < BUFFER_COUNT; i++)
		{
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, indices[i], buffers[i]);
		}
	}

	void update(int index, int length, const std::vector<void*>& data)
	{
		assert(data.size() == BUFFER_COUNT);
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
		std::vector<GLuint> bufferIds(BUFFER_COUNT);
		glDeleteBuffers(BUFFER_COUNT, bufferIds.data());
	}

};