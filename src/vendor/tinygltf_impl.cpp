// Compilazione unica dell'implementazione tinygltf.
// Includiamo stb_image come dichiarazioni (implementazione in stb_image_impl.cpp),
// ma compiliamo stb_image_write qui perché non è inclusa altrove.
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include "tiny_gltf.h"
