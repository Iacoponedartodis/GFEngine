#include "mini/platform/OpenGL.hpp"

#include <cstring>
#include <iostream>

PFNGLGENBUFFERSPROC              glGenBuffers              = nullptr;
PFNGLBINDBUFFERPROC              glBindBuffer              = nullptr;
PFNGLBUFFERDATAPROC              glBufferData              = nullptr;
PFNGLBUFFERSUBDATAPROC           glBufferSubData           = nullptr;
PFNGLDELETEBUFFERSPROC           glDeleteBuffers           = nullptr;

PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays         = nullptr;
PFNGLBINDVERTEXARRAYPROC         glBindVertexArray         = nullptr;
PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays      = nullptr;

PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray  = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer      = nullptr;

PFNGLCREATESHADERPROC            glCreateShader            = nullptr;
PFNGLSHADERSOURCEPROC            glShaderSource            = nullptr;
PFNGLCOMPILESHADERPROC           glCompileShader           = nullptr;
PFNGLGETSHADERIVPROC             glGetShaderiv             = nullptr;
PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog        = nullptr;
PFNGLDELETESHADERPROC            glDeleteShader            = nullptr;

PFNGLCREATEPROGRAMPROC           glCreateProgram           = nullptr;
PFNGLATTACHSHADERPROC            glAttachShader            = nullptr;
PFNGLLINKPROGRAMPROC             glLinkProgram             = nullptr;
PFNGLGETPROGRAMIVPROC            glGetProgramiv            = nullptr;
PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog       = nullptr;
PFNGLUSEPROGRAMPROC              glUseProgram              = nullptr;
PFNGLDELETEPROGRAMPROC           glDeleteProgram           = nullptr;

PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation      = nullptr;
PFNGLUNIFORM1FPROC               glUniform1f               = nullptr;
PFNGLUNIFORM2FPROC               glUniform2f               = nullptr;
PFNGLUNIFORM3FPROC               glUniform3f               = nullptr;
PFNGLUNIFORM4FPROC               glUniform4f               = nullptr;
PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv        = nullptr;

namespace
{
    template<typename T>
    bool loadFunc(T& outFunc, const char* name)
    {
        PROC proc = wglGetProcAddress(name);
        if (!proc)
        {
            std::cerr << "[GL] Non trovato: " << name << std::endl;
            return false;
        }
        static_assert(sizeof(proc) == sizeof(T), "function pointer size mismatch");
        std::memcpy(&outFunc, &proc, sizeof(proc));
        return true;
    }
}

bool miniGLLoad()
{
    bool ok = true;

    ok &= loadFunc(glGenBuffers,               "glGenBuffers");
    ok &= loadFunc(glBindBuffer,               "glBindBuffer");
    ok &= loadFunc(glBufferData,               "glBufferData");
    ok &= loadFunc(glBufferSubData,            "glBufferSubData");
    ok &= loadFunc(glDeleteBuffers,            "glDeleteBuffers");

    ok &= loadFunc(glGenVertexArrays,          "glGenVertexArrays");
    ok &= loadFunc(glBindVertexArray,          "glBindVertexArray");
    ok &= loadFunc(glDeleteVertexArrays,       "glDeleteVertexArrays");

    ok &= loadFunc(glEnableVertexAttribArray,  "glEnableVertexAttribArray");
    ok &= loadFunc(glDisableVertexAttribArray, "glDisableVertexAttribArray");
    ok &= loadFunc(glVertexAttribPointer,      "glVertexAttribPointer");

    ok &= loadFunc(glCreateShader,             "glCreateShader");
    ok &= loadFunc(glShaderSource,             "glShaderSource");
    ok &= loadFunc(glCompileShader,            "glCompileShader");
    ok &= loadFunc(glGetShaderiv,              "glGetShaderiv");
    ok &= loadFunc(glGetShaderInfoLog,         "glGetShaderInfoLog");
    ok &= loadFunc(glDeleteShader,             "glDeleteShader");

    ok &= loadFunc(glCreateProgram,            "glCreateProgram");
    ok &= loadFunc(glAttachShader,             "glAttachShader");
    ok &= loadFunc(glLinkProgram,              "glLinkProgram");
    ok &= loadFunc(glGetProgramiv,             "glGetProgramiv");
    ok &= loadFunc(glGetProgramInfoLog,        "glGetProgramInfoLog");
    ok &= loadFunc(glUseProgram,               "glUseProgram");
    ok &= loadFunc(glDeleteProgram,            "glDeleteProgram");

    ok &= loadFunc(glGetUniformLocation,       "glGetUniformLocation");
    ok &= loadFunc(glUniform1f,                "glUniform1f");
    ok &= loadFunc(glUniform2f,                "glUniform2f");
    ok &= loadFunc(glUniform3f,                "glUniform3f");
    ok &= loadFunc(glUniform4f,                "glUniform4f");
    ok &= loadFunc(glUniformMatrix4fv,         "glUniformMatrix4fv");

    if (ok)
        std::cout << "[GL] Funzioni OpenGL 3.3 caricate." << std::endl;
    else
        std::cerr << "[GL] ERRORE: alcune funzioni non disponibili." << std::endl;

    return ok;
}