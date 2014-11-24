// system includes
#include <algorithm>
// GIMP includes
#include <libgimp/gimp.h>
// OpenEXR includes
#include <half.h>
// plugin includes
#include "exr_file.hpp"
// myself
#include "conversion.hpp"

using namespace exr;

//-----------------------------------------------------------------------------
// Helpers


// Creates a new GIMP image.
//
// @param[in]   type
//  GIMP image type
// @param[in]   width
//  width of the image in pixels
// @param[in]   height
//  height of the image in pixels
// @param[out]  image_id
//  id of the fresh image, only valid when this function returns true
// @param[out]  error_msg
//  error message, only contains a meaningful message when this function
//  fails
// @return
//  true on success, false on failure
static bool create_gimp_image (const GimpImageBaseType type,
                               size_t                  width,
                               size_t                  height,
                               gint32                  &image_id,
                               std::string             &error_msg)
{
  image_id = gimp_image_new (width, height, type);
  if (image_id == -1)
    {
      error_msg = "failed to create GIMP image";
      return false;
    }
  return true;
}


// Adds a layer to an existing GIMP image and copies in the provided pixel
// data.
//
// @param[in]   type
//  type of layer
// @param[in]   width
//  width of the layer in pixels
// @param[in]   height
//  height of the layer in pixels
// @param[in]   image_id
//  id of the image to which we add this layer
// @param[in]   data
//  pixel data in the format as expected by the layer type
// @param[out]  error_msg
//  error message, only filled in when something went wrong
// @return
//  true on success, false on failure
static bool add_layer (const GimpImageType type,
                       const std::string   &layer_name,
                       const size_t        width,
                       const size_t        height,
                       const gint32        image_id,
                       const guchar        *data,
                       std::string         &error_msg)
{
  const gint32 layer_id = gimp_layer_new (image_id,
                                          layer_name.c_str(),
                                          width,
                                          height,
                                          type,
                                          100.0,
                                          GIMP_NORMAL_MODE);
  if (layer_id == -1)
    {
      error_msg = "failed to create layer";
      return false;
    }

  if (!gimp_image_insert_layer(image_id, layer_id, 0, -1))
    {
      gimp_item_delete (layer_id); 
      error_msg = "failed to add layer";
      return false;
    }

  GimpDrawable *drawable = gimp_drawable_get (layer_id);
  if (!drawable)
    {
      gimp_item_delete (layer_id); 
      error_msg = "failed to get drawable for layer";
      return false;
    }

  GimpPixelRgn pixel_region;
  gimp_pixel_rgn_init (&pixel_region,
                       drawable,
                       0, 0,
                       width, height, 
                       TRUE,
                       TRUE);
  gimp_pixel_rgn_set_rect (&pixel_region,
                           data,
                           0,
                           0,
                           pixel_region.w,
                           pixel_region.h);

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, FALSE);
  gimp_drawable_update (drawable->drawable_id,
                        0, 0,
                        width, height);

  gimp_item_delete (drawable->drawable_id); 
  return true;
}


enum LayerType
{
  // random channels
  LAYER_TYPE_UNDEFINED = 0,
  // grayscale channel
  LAYER_TYPE_Y         = 1,
  // grayscale & chroma channels
  LAYER_TYPE_YC        = 2,
  // grayscale & alpha channels
  LAYER_TYPE_YA        = 3,
  // grayscale, chroma & alpha channels
  LAYER_TYPE_YCA       = 4,
  // red, green, blue & alpha channels
  LAYER_TYPE_RGBA      = 5,
  // red, green and blue channel
  LAYER_TYPE_RGB       = 6,
};


static LayerType determine_layer_type (const exr::Layer &layer)
{
  if (layer.get_channel_count() > 4)
    {
      return LAYER_TYPE_UNDEFINED;
    }

  // append and sort layer names
  std::string names = "";
  for (size_t i = 0; i < layer.get_channel_count(); ++i)
    {
      names += layer.get_channel_at(i)->get_name();
    }
  std::sort(names.begin(), names.end());

  // figure out the type from the names
  if (names == "Y"     ) { return LAYER_TYPE_Y;    }
  if (names == "BRYYY" ) { return LAYER_TYPE_YC;   }
  if (names == "AY"    ) { return LAYER_TYPE_YA;   }
  if (names == "ABRYYY") { return LAYER_TYPE_YCA;  }
  if (names == "ABGR"  ) { return LAYER_TYPE_RGBA; }
  if (names == "BGR"   ) { return LAYER_TYPE_RGB;  }

  return LAYER_TYPE_UNDEFINED;
}


static const char* layer_type_to_string (const LayerType type)
{
  switch (type)
  {
    LAYER_TYPE_UNDEFINED: { return "undefined"; }
    LAYER_TYPE_Y        : { return "Y";         }
    LAYER_TYPE_YC       : { return "YC";        }
    LAYER_TYPE_YA       : { return "YA";        }
    LAYER_TYPE_YCA      : { return "YCA";       }
    LAYER_TYPE_RGBA     : { return "RGBA";      }
    LAYER_TYPE_RGB      : { return "RGB";       }
    default             : { return "unknown";   }
  }
}


template<typename T>
static inline T clamp (const T x,
                       const T lo,
                       const T hi)
{
  if (x < lo) { return lo; }
  if (x > hi) { return hi; }
  return x;
}


// Converts EXR HDR channels data to GIMP 8-bit LDR.
//
// @param[in]   settings
//    user-configured conversion settings
// @param[in]   pixel_count
//    number of pixels for the image - must be also the lenght of each channel
//    data array
// @param[in]   data_type
//    data type the channels
// @param[in]   input
//    list with the raw data for each channel
// @param[out]  output
//    8-bit LDR image, it's up to the caller to delete[] this image afterwards
static void convert_to_ldr(const ConversionSettings &settings,
                           const size_t             pixel_count,
                           const exr::PixelDataType data_type,
                           std::vector<const char*> &input,
                           guchar                   **output)
{
  const float  inv_gamma     = 1.0f / settings.m_gamma;
  const float  exposure      = powf(2.f, settings.m_exposure + 2.47393f);
  const size_t channel_count = input.size();
  *output                    = new guchar[pixel_count * channel_count];

  // copy over the pixels 
  // TODO: do a proper HDR to LDR conversion
  // TODO: vectorize this code
  switch (data_type)
    {
    case exr::PIXEL_DATA_TYPE_FLOAT: 
      {
        for (size_t i = 0; i < pixel_count; ++i)
          {
            for (size_t j = 0; j < channel_count; ++j)
              {
                float val = ((float*)input[j])[i];
                (*output)[channel_count * i + j] = clamp(val * 255.f, 0.f, 255.f);
              }
          }
        break;
      }
    case exr::PIXEL_DATA_TYPE_HALF: 
      {
         for (size_t i = 0; i < pixel_count; ++i)
            {
              for (size_t j = 0; j < channel_count; ++j)
                {
                  float val = ((half*)input[j])[i] * 1.f;
                  (*output)[channel_count * i + j] = clamp(val * 255.f, 0.f, 255.f);
                }
            }
         break;
      }
    case exr::PIXEL_DATA_TYPE_UINT: 
      {
         for (size_t i = 0; i < pixel_count; ++i)
            {
              for (size_t j = 0; j < channel_count; ++j)
                {
                  float val = ((unsigned int*)input[j])[i] * 1u;
                  (*output)[channel_count * i + j] = clamp(val * 255.f, 0.f, 255.f);
                }
            }
        break;
      }
    }
}


// Converts EXR luminance/chroma channels data to GIMP 8-bit LDR.
//
// @param[in]   settings
//    user-configured conversion settings
// @param[in]   width
//    width of the image in pixels
// @param[in]   height
//    height of the image in pixels
// @param[in]   data_type
//    data type the channels
// @param[in]   input
//    list with the raw data for each channel
// @param[out]  output
//    8-bit LDR image, it's up to the caller to delete[] this image afterwards
static void chroma_to_ldr (const ConversionSettings &settings,
                           const size_t             width,
                           const size_t             height,
                           const exr::PixelDataType data_type,
                           std::vector<const char*> &input,
                           guchar                   **output)
{
  const float  inv_gamma     = 1.0f / settings.m_gamma;
  const float  exposure      = powf(2.f, settings.m_exposure + 2.47393f);
  const size_t channel_count = input.size();
  *output                    = new guchar[width * height * channel_count];

  // copy over the pixels 
  // TODO: do a proper HDR to LDR conversion
  // TODO: vectorize this code
  switch (data_type)
    {
    case exr::PIXEL_DATA_TYPE_FLOAT: 
    case exr::PIXEL_DATA_TYPE_UINT: 
      {
        const float *lum = (const float*)input[0];
        const float *ry  = (const float*)input[1];
        const float *rb  = (const float*)input[2];
        const float *a   = channel_count > 3 ? (const float*)input[3] : NULL;
        for (size_t y = 0; y < height; ++y)
          {
            for (size_t x = 0; x < width; ++x)
              {
                const size_t ix   = y * width + x;
                const size_t six  = (y / 2) * width + (x / 2);
                // get luminance/chroma and convert them to rgb color space
                (*output)[channel_count * ix + 0] = clamp(lum[ix] * 255.f, 0.f, 255.f);
                (*output)[channel_count * ix + 1] = clamp(ry[six] * 255.f, 0.f, 255.f);
                (*output)[channel_count * ix + 2] = clamp(rb[six] * 255.f, 0.f, 255.f);
                if (a)
                  {
                    (*output)[channel_count * ix + 3] = clamp(a[ix] * 255.f, 0.f, 255.f);
                  }
              }
            }
        break;
      }
    case exr::PIXEL_DATA_TYPE_HALF: 
      {
        const half *lum = (const half*)input[0];
        const half *ry  = (const half*)input[1];
        const half *rb  = (const half*)input[2];
        const half *a   = channel_count > 3 ? (const half*)input[3] : NULL;
        for (size_t y = 0; y < height; ++y)
          {
            for (size_t x = 0; x < width; ++x)
              {
                const size_t ix   = y * width + x;
                const size_t six  = (y / 2) * width + (x / 2);
                // get luminance/chroma and convert them to rgb color space
                (*output)[channel_count * ix + 0] = clamp(lum[ix] * 255.f, 0.f, 255.f);
                (*output)[channel_count * ix + 1] = clamp(ry[six] * 255.f, 0.f, 255.f);
                (*output)[channel_count * ix + 2] = clamp(rb[six] * 255.f, 0.f, 255.f);
                if (a)
                  {
                    (*output)[channel_count * ix + 3] = clamp(a[ix] * 255.f, 0.f, 255.f);
                  }
              }
            }
         break;
      }
    }
}


//-----------------------------------------------------------------------------
// Implementation of Converter


Converter::Converter (const exr::File          &file,
                      const ConversionSettings &settings)
:
  m_file (file),
  m_settings (settings)
{}


bool Converter::convert (gint32      &image_id,
                         std::string &error_msg)
{
  error_msg.clear();

  // not much we can do if the file isn't in memory yet
  if (!m_file.is_loaded())
    {
      error_msg = "file not loaded in memory";
      return false;
    }

  // check if all layers are grayscale, only then we can create
  // a graycale image in the GIMP
  bool grayscale = true;
  for (size_t i = 0; i < m_file.get_layer_count(); ++i)
    {
      const LayerType layer_type = determine_layer_type (*m_file.get_layer_at(i));
      if (layer_type != LAYER_TYPE_Y && layer_type != LAYER_TYPE_YA)
        {
          grayscale = false;
          break;
        }
    }

  // create GIMP image
  if (!create_gimp_image (grayscale ? GIMP_GRAY : GIMP_RGB,
                          m_file.get_width(),
                          m_file.get_height(),
                          image_id,
                          error_msg))
    {
      return false;
    }

  // convert each layer individually 
  for (size_t i = 0; i < m_file.get_layer_count(); ++i)
    {
      const Layer     *layer = m_file.get_layer_at(i);
      const LayerType type   = determine_layer_type (*layer);
      switch (type)
        {
          case LAYER_TYPE_RGBA:
            {
              std::vector<const char*> input;
              input.push_back(layer->get_channel("R")->get_data());
              input.push_back(layer->get_channel("G")->get_data());
              input.push_back(layer->get_channel("B")->get_data());
              input.push_back(layer->get_channel("A")->get_data());
              
              guchar *output = NULL;
              convert_to_ldr (m_settings,
                              m_file.get_width() * m_file.get_height(),
                              layer->get_channel("R")->get_pixel_data_type(),
                              input,
                              &output);

              if (!add_layer (GIMP_RGBA_IMAGE,
                              layer->get_name(),
                              m_file.get_width(),
                              m_file.get_height(),
                              image_id,
                              output,
                              error_msg))
                {
                  delete[] output;
                  return false;
                }

              delete[] output;
              break;
            }
          case LAYER_TYPE_RGB:
            {
              std::vector<const char*> input;
              input.push_back(layer->get_channel("R")->get_data());
              input.push_back(layer->get_channel("G")->get_data());
              input.push_back(layer->get_channel("B")->get_data());
              
              guchar *output = NULL;
              convert_to_ldr (m_settings,
                              m_file.get_width() * m_file.get_height(),
                              layer->get_channel("R")->get_pixel_data_type(),
                              input,
                              &output);

              if (!add_layer (GIMP_RGB_IMAGE,
                              layer->get_name(),
                              m_file.get_width(),
                              m_file.get_height(),
                              image_id,
                              output,
                              error_msg))
                {
                  delete[] output;
                  return false;
                }

              delete[] output;
              break;

            }
          case LAYER_TYPE_Y:
            {
              guchar *output = NULL;
              if (grayscale)
                {
                  std::vector<const char*> input;
                  input.push_back(layer->get_channel("Y")->get_data());

                  convert_to_ldr (m_settings,
                                  m_file.get_width() * m_file.get_height(),
                                  layer->get_channel("Y")->get_pixel_data_type(),
                                  input,
                                  &output);
                }
              else
                {
                  std::vector<const char*> input;
                  input.push_back(layer->get_channel("Y")->get_data());
                  input.push_back(layer->get_channel("Y")->get_data());
                  input.push_back(layer->get_channel("Y")->get_data());

                  convert_to_ldr (m_settings,
                                  m_file.get_width() * m_file.get_height(),
                                  layer->get_channel("Y")->get_pixel_data_type(),
                                  input,
                                  &output);
                }

              if (!add_layer (GIMP_GRAY_IMAGE,
                              layer->get_name(),
                              m_file.get_width(),
                              m_file.get_height(),
                              image_id,
                              output,
                              error_msg))
                {
                  delete[] output;
                  return false;
                }

              delete[] output;
              break;
            }
          case LAYER_TYPE_YA:
            {
              guchar *output = NULL;
              if (grayscale)
                {
                  std::vector<const char*> input;
                  input.push_back(layer->get_channel("Y")->get_data());
                  input.push_back(layer->get_channel("A")->get_data());

                  convert_to_ldr (m_settings,
                                  m_file.get_width() * m_file.get_height(),
                                  layer->get_channel("Y")->get_pixel_data_type(),
                                  input,
                                  &output);
                }
              else
                {
                  std::vector<const char*> input;
                  input.push_back(layer->get_channel("Y")->get_data());
                  input.push_back(layer->get_channel("Y")->get_data());
                  input.push_back(layer->get_channel("Y")->get_data());
                  input.push_back(layer->get_channel("A")->get_data());

                  convert_to_ldr (m_settings,
                                  m_file.get_width() * m_file.get_height(),
                                  layer->get_channel("Y")->get_pixel_data_type(),
                                  input,
                                  &output);
                }

              if (!add_layer (GIMP_GRAYA_IMAGE,
                              layer->get_name(),
                              m_file.get_width(),
                              m_file.get_height(),
                              image_id,
                              output,
                              error_msg))
                {
                  delete[] output;
                  return false;
                }

              delete[] output;
              break;
            }
          case LAYER_TYPE_YC:
          case LAYER_TYPE_YCA:
            {
              std::vector<const char*> input;
              input.push_back(layer->get_channel("Y")->get_data());
              input.push_back(layer->get_channel("RY")->get_data());
              input.push_back(layer->get_channel("BY")->get_data());
              Channel *alpha_channel = NULL;
              if (layer->find_channel("A", alpha_channel))
              {
                input.push_back(alpha_channel->get_data());
              }

              guchar *output = NULL;
              chroma_to_ldr (m_settings,
                             m_file.get_width(),
                             m_file.get_height(),
                             layer->get_channel("Y")->get_pixel_data_type(),
                             input,
                             &output);

              if (!add_layer (GIMP_RGB_IMAGE,
                              layer->get_name(),
                              m_file.get_width(),
                              m_file.get_height(),
                              image_id,
                              output,
                              error_msg))
                {
                  delete[] output;
                  return false;
                }
              break;
            }
          case LAYER_TYPE_UNDEFINED:
            {
              error_msg = "not implemented: " 
                          + std::string(layer_type_to_string (type));
              return false;
            }
        }
    }

  return true;
}


/* vim: set ts=2 sw=2 : */
