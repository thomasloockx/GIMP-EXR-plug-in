#ifndef _CONVERSION_HPP_
#define _CONVERSION_HPP_ 1

// system includes
#include <string>

namespace exr
{
    class File;
}


//-----------------------------------------------------------------------------
// Tracks the user-defined settings for doing the conversion.
struct ConversionSettings
{
    // gamma correction factor
    float m_gamma;
};



//-----------------------------------------------------------------------------
// Converts an EXR file into a GIMP image. We try to map the structure of the
// EXR file to something that makes sense in the GIMP.
class Converter
{
public:

    // Creates a new converter.
    Converter (const exr::File          &file,
               const ConversionSettings &settings);

    // Converts an EXR file into an 8-bit GIMP image.
    //
    // @param[out]  image_id
    //  Id of the freshly created image. Only valid when we return true.
    // @param[out]  error_message
    //  Error message, only set when this function fails.
    // @return
    //  True on success, false on failure.
    bool convert (gint32      &image_id,
                  std::string &error_msg);

protected:

    // file to convert
    const exr::File          &m_file;
    // conversion settings
    const ConversionSettings m_settings;
};



#endif // #ifndef _CONVERSION_HPP_
