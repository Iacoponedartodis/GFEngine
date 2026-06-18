#pragma once
 
// Loader OpenGL minimalista per GFEngine.
// Include solo le funzioni che usiamo — si aggiungono qui man mano.
// Usa SDL_GL_GetProcAddress come backend: nessuna dipendenza aggiuntiva.
 
#ifdef _WIN32
  #include <windows.h>
#endif
#include <GL/gl.h>   // OpenGL 1.x: tipi base + glClear, glViewport, glDrawArrays, ecc.
 
#include <cstddef>   // ptrdiff_t
 
// ============================================================
// Tipi aggiuntivi (GL 2.0+, non in GL/gl.h)
// ============================================================
 
typedef char          GLchar;
typedef ptrdiff_t     GLsizeiptr;
typedef ptrdiff_t     GLintptr;
 
// ============================================================
// Costanti (GL 1.5 - 3.3)
// ============================================================
 
// Buffer objects (GL 1.5)
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW          0x88B4
#define GL_DYNAMIC_DRAW         0x88B8
#define GL_STREAM_DRAW          0x88B0
 
// Shaders (GL 2.0)
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_VERTEX_SHADER        0x8B31
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_INFO_LOG_LENGTH      0x8B84
 
// ============================================================
// Tipi dei function pointer
// ============================================================
 
// --- Buffer objects (GL 1.5) ---
typedef void   (APIENTRY* PFNGLGENBUFFERSPROC)    (GLsizei n, GLuint* buffers);
typedef void   (APIENTRY* PFNGLBINDBUFFERPROC)    (GLenum target, GLuint buffer);
typedef void   (APIENTRY* PFNGLBUFFERDATAPROC)    (GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void   (APIENTRY* PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint* buffers);
 
// --- Vertex Array Objects (GL 3.0) ---
typedef void   (APIENTRY* PFNGLGENVERTEXARRAYSPROC)    (GLsizei n, GLuint* arrays);
typedef void   (APIENTRY* PFNGLBINDVERTEXARRAYPROC)    (GLuint array);
typedef void   (APIENTRY* PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint* arrays);
 
// --- Vertex attributes (GL 2.0) ---
typedef void   (APIENTRY* PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void   (APIENTRY* PFNGLVERTEXATTRIBPOINTERPROC)     (GLuint index, GLint size, GLenum type,
                                                              GLboolean normalized, GLsizei stride, const void* pointer);
 
// --- Shader compilation (GL 2.0) ---
typedef GLuint (APIENTRY* PFNGLCREATESHADERPROC)     (GLenum type);
typedef void   (APIENTRY* PFNGLSHADERSOURCEPROC)     (GLuint shader, GLsizei count,
                                                       const GLchar* const* string, const GLint* length);
typedef void   (APIENTRY* PFNGLCOMPILESHADERPROC)    (GLuint shader);
typedef void   (APIENTRY* PFNGLGETSHADERIVPROC)      (GLuint shader, GLenum pname, GLint* params);
typedef void   (APIENTRY* PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize,
                                                       GLsizei* length, GLchar* infoLog);
typedef void   (APIENTRY* PFNGLDELETESHADERPROC)     (GLuint shader);
 
// --- Shader program (GL 2.0) ---
typedef GLuint (APIENTRY* PFNGLCREATEPROGRAMPROC)     (void);
typedef void   (APIENTRY* PFNGLATTACHSHADERPROC)      (GLuint program, GLuint shader);
typedef void   (APIENTRY* PFNGLLINKPROGRAMPROC)       (GLuint program);
typedef void   (APIENTRY* PFNGLGETPROGRAMIVPROC)      (GLuint program, GLenum pname, GLint* params);
typedef void   (APIENTRY* PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize,
                                                        GLsizei* length, GLchar* infoLog);
typedef void   (APIENTRY* PFNGLUSEPROGRAMPROC)        (GLuint program);
typedef void   (APIENTRY* PFNGLDELETEPROGRAMPROC)     (GLuint program);
 
// --- Uniforms (GL 2.0) ---
typedef GLint  (APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)  (GLuint program, const GLchar* name);
typedef void   (APIENTRY* PFNGLUNIFORM1FPROC)            (GLint location, GLfloat v0);
typedef void   (APIENTRY* PFNGLUNIFORM2FPROC)            (GLint location, GLfloat v0, GLfloat v1);
typedef void   (APIENTRY* PFNGLUNIFORM3FPROC)            (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void   (APIENTRY* PFNGLUNIFORM4FPROC)            (GLint location, GLfloat v0, GLfloat v1,
                                                           GLfloat v2, GLfloat v3);
typedef void   (APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)     (GLint location, GLsizei count,
                                                           GLboolean transpose, const GLfloat* value);
 
// ============================================================
// Dichiarazioni extern (definite in OpenGL.cpp)
// ============================================================
 
extern PFNGLGENBUFFERSPROC              glGenBuffers;
extern PFNGLBINDBUFFERPROC              glBindBuffer;
extern PFNGLBUFFERDATAPROC              glBufferData;
extern PFNGLDELETEBUFFERSPROC           glDeleteBuffers;
 
extern PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC         glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays;
 
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
 
extern PFNGLCREATESHADERPROC            glCreateShader;
extern PFNGLSHADERSOURCEPROC            glShaderSource;
extern PFNGLCOMPILESHADERPROC           glCompileShader;
extern PFNGLGETSHADERIVPROC             glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog;
extern PFNGLDELETESHADERPROC            glDeleteShader;
 
extern PFNGLCREATEPROGRAMPROC           glCreateProgram;
extern PFNGLATTACHSHADERPROC            glAttachShader;
extern PFNGLLINKPROGRAMPROC             glLinkProgram;
extern PFNGLGETPROGRAMIVPROC            glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC              glUseProgram;
extern PFNGLDELETEPROGRAMPROC           glDeleteProgram;
 
extern PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
extern PFNGLUNIFORM1FPROC               glUniform1f;
extern PFNGLUNIFORM2FPROC               glUniform2f;
extern PFNGLUNIFORM3FPROC               glUniform3f;
extern PFNGLUNIFORM4FPROC               glUniform4f;
extern PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
 
// ============================================================
// Loader — chiama subito dopo SDL_GL_CreateContext()
// ============================================================
 
bool miniGLLoad();