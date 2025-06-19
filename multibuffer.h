#pragma once
#include <vector>
#include "glad/glad.h"

struct Multibuffer
{
    int numberOfBuffers;
    std::vector<GLuint> buffers;
    int rotation{ 0 };

    Multibuffer(int count, GLsizeiptr size, GLbitfield flags)
        : numberOfBuffers{count}, buffers(count, 0)
    {
        createBuffers(size, flags);
    }

    ~Multibuffer()
    {
        glDeleteBuffers(numberOfBuffers, buffers.data());
    }

    void createBuffers(GLsizeiptr size, GLbitfield flags)
    {
        glCreateBuffers(numberOfBuffers, buffers.data());

        for (int i = 0; i < numberOfBuffers; ++i)
        {
            glNamedBufferStorage(buffers[i], size, nullptr, flags);
        }
    }

    void rotate() {
        rotation = getRotatedIndex(rotation + 1);
    }

    GLuint getRead() const { return buffers[rotation]; }
    GLuint getWrite() const { return buffers[getRotatedIndex(rotation + 1)]; }
    GLuint getStandby() const { return buffers[getRotatedIndex(rotation + 2)]; }
    GLuint getBufferAtIndex(int index) const { return buffers[index]; }

private:
    int getRotatedIndex(int index) const {
        return index % numberOfBuffers;
    }
};