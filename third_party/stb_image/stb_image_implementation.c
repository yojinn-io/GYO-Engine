/* The one translation unit that holds stb_image's code. Its configuration
 * comes from the gyo_stb_image target (CMakeLists.txt next to this file), so
 * the adapter that includes the header sees the same one. */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
