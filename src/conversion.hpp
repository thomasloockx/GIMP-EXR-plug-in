#ifndef _CONVERSION_HPP_
#define _CONVERSION_HPP_ 1

// C++ includes
#include <string>

namespace exr
{
    class File;
}


// Converts an EXR file into an 8-bit GIMP image. We try to map the structure
// of the EXR file as best as possible to GIMP layers.
// 
// @param[in]   file
//  EXR file, this file must already be loaded in memory.
// @param[out]  image_id
//  Id of the freshly created image. Only valid when we return true.
// @param[out]  error_message
//  Error message, only valid when we return false.
// @return
//  True on success, false on failure.
bool convert_to_8_bit (const exr::File &file,
                       gint32          &image_id,
                       std::string     &error_msg);


#endif // #ifndef _CONVERSION_HPP_
