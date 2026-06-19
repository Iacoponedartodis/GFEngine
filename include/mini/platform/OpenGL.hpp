#pragma once

#ifdef _WIN32
  #include <windows.h>
#endif
#include <GL/gl.h>
#include <cstddef>

typedef char          GLchar;
typedef ptrdiff_t     GLsizeiptr;
typedef ptrdiff_t     GLintptr;

#ifndef GL_ARRAY_BUFFER
  #define GL_ARRAY_BUFFER         0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
  #define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
  #define GL_STATIC_DRAW          0x88B4
#endif
#ifndef GL_DYNAMIC_DRAW
  #define GL_DYNAMIC_DRAW         0x88B8
#endif
#ifndef GL_STREAM_DRAW
  #define GL_STREAM_DRAW          0x88B0
#endif
#ifndef GL_FRAGMENT_SHADER
  #define GL_FRAGMENT_SHADER      0x8B30
#endif
#ifndef GL_VERTEX_SHADER
  #define GL_VERTEX_SHADER        0x8B31
#endif
#ifndef GL_COMPILE_STATUS
  #define GL_COMPILE_STATUS       0x8B81
#endif
#ifndef GL_LINK_STATUS
  #define GL_LINK_STATUS          0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
  #define GL_INFO_LOG_LENGTH      0x8B84
#endif

typedef void   (APIENTRY* PFNGLGENBUFFERSPROC)              (GLsizei n, GLuint* buffers);
typedef void   (APIENTRY* PFNGLBINDBUFFERPROC)              (GLenum target, GLuint buffer);
typedef void   (APIENTRY* PFNGLBUFFERDATAPROC)              (GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void   (APIENTRY* PFNGLBUFFERSUBDATAPROC)           (GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void   (APIENTRY* PFNGLDELETEBUFFERSPROC)           (GLsizei n, const GLuint* buffers);

typedef void   (APIENTRY* PFNGLGENVERTEXARRAYSPROC)         (GLsizei n, GLuint* arrays);
typedef void   (APIENTRY* PFNGLBINDVERTEXARRAYPROC)         (GLuint array);
typedef void   (APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)      (GLsizei n, const GLuint* arrays);

typedef void   (APIENTRY* PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void   (APIENTRY* PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void   (APIENTRY* PFNGLVERTEXATTRIBPOINTERPROC)     (GLuint index, GLint size, GLenum type,
                                                              GLboolean normalized, GLsizei stride, const void* pointer);

typedef GLuint (APIENTRY* PFNGLCREATESHADERPROC)            (GLenum type);
typedef void   (APIENTRY* PFNGLSHADERSOURCEPROC)            (GLuint shader, GLsizei count,
                                                              const GLchar* const* string, const GLint* length);
typedef void   (APIENTRY* PFNGLCOMPILESHADERPROC)           (GLuint shader);
typedef void   (APIENTRY* PFNGLGETSHADERIVPROC)             (GLuint shader, GLenum pname, GLint* params);
typedef void   (APIENTRY* PFNGLGETSHADERINFOLOGPROC)        (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void   (APIENTRY* PFNGLDELETESHADERPROC)            (GLuint shader);

typedef GLuint (APIENTRY* PFNGLCREATEPROGRAMPROC)           (void);
typedef void   (APIENTRY* PFNGLATTACHSHADERPROC)            (GLuint program, GLuint shader);
typedef void   (APIENTRY* PFNGLLINKPROGRAMPROC)             (GLuint program);
typedef void   (APIENTRY* PFNGLGETPROGRAMIVPROC)            (GLuint program, GLenum pname, GLint* params);
typedef void   (APIENTRY* PFNGLGETPROGRAMINFOLOGPROC)       (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void   (APIENTRY* PFNGLUSEPROGRAMPROC)              (GLuint program);
typedef void   (APIENTRY* PFNGLDELETEPROGRAMPROC)           (GLuint program);

typedef GLint  (APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)      (GLuint program, const GLchar* name);
typedef void   (APIENTRY* PFNGLUNIFORM1FPROC)               (GLint location, GLfloat v0);
typedef void   (APIENTRY* PFNGLUNIFORM2FPROC)               (GLint location, GLfloat v0, GLfloat v1);
typedef void   (APIENTRY* PFNGLUNIFORM3FPROC)               (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void   (APIENTRY* PFNGLUNIFORM4FPROC)               (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void   (APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)        (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

extern PFNGLGENBUFFERSPROC               glGenBuffers;
extern PFNGLBINDBUFFERPROC               glBindBuffer;
extern PFNGLBUFFERDATAPROC               glBufferData;
extern PFNGLBUFFERSUBDATAPROC            glBufferSubData;
extern PFNGLDELETEBUFFERSPROC            glDeleteBuffers;

extern PFNGLGENVERTEXARRAYSPROC          glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC          glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC       glDeleteVertexArrays;

extern PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer;

extern PFNGLCREATESHADERPROC             glCreateShader;
extern PFNGLSHADERSOURCEPROC             glShaderSource;
extern PFNGLCOMPILESHADERPROC            glCompileShader;
extern PFNGLGETSHADERIVPROC              glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog;
extern PFNGLDELETESHADERPROC             glDeleteShader;

extern PFNGLCREATEPROGRAMPROC            glCreateProgram;
extern PFNGLATTACHSHADERPROC             glAttachShader;
extern PFNGLLINKPROGRAMPROC              glLinkProgram;
extern PFNGLGETPROGRAMIVPROC             glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC               glUseProgram;
extern PFNGLDELETEPROGRAMPROC            glDeleteProgram;

extern PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation;
extern PFNGLUNIFORM1FPROC                glUniform1f;
extern PFNGLUNIFORM2FPROC                glUniform2f;
extern PFNGLUNIFORM3FPROC                glUniform3f;
extern PFNGLUNIFORM4FPROC                glUniform4f;
extern PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv;

bool miniGLLoad();