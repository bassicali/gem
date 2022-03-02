
#include <iostream>
#include <fstream>
#include <cfloat>
#include <windows.h>

#include <glad/glad.h>

#include "OpenGL/Common.h"

using namespace std;

void* __GetOpenGLAddr(const char* function_name)
{
    static HMODULE libgl = LoadLibraryW(L"opengl32.dll");

    void* p = (void*)wglGetProcAddress(function_name);
    if (p == 0 || (p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) || (p == (void*)-1))
    {
        p = (void*)GetProcAddress(libgl, function_name);
    }

    return p;
}

void __CheckForGLErrors(const char* file, int line, const char* function)
{
    std::string message;
    GLenum code;

    int max_iterations = 5;

    while ((code = glGetError()) != GL_NO_ERROR && --max_iterations)
    {
        switch (code)
        {
        case GL_INVALID_ENUM:
            message = "INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            message = "INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            message = "INVALID_OPERATION";
            break;
        case GL_STACK_OVERFLOW:
            message = "STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            message = "STACK_UNDERFLOW";
            break;
        case GL_OUT_OF_MEMORY:
            message = "OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            message = "INVALID_FRAMEBUFFER_OPERATION";
            break;
        }

        LOG_ERROR("GL error in %s: %08X (%s:%d): %s", function, code, file, line, message.c_str());
    }
}
