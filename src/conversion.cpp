// system includes
#include <algorithm>
// GIMP includes
#include <libgimp/gimp.h>
// plugin includes
#include "exr_file.hpp"
// myself
#include "conversion.hpp"

using namespace exr;

//-----------------------------------------------------------------------------
// Helpers


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


LayerType determine_layer_type (const exr::Layer &layer)
{
  if (layer.get_channel_count() > 4)
    {
      return LAYER_TYPE_UNDEFINED;
    }

  // append and sort layer names
  std::string names = "";
  for (size_t i = 0; i < layer.get_channel_count(); ++i)
    {
      names += layer.get_channels()[i]->get_name();
    }
  std::sort(names.begin(), names.end());

  // figure out the type from the names
  if (names == "Y"   ) { return LAYER_TYPE_Y;    }
  if (names == "CY"  ) { return LAYER_TYPE_YC;   }
  if (names == "AY"  ) { return LAYER_TYPE_YA;   }
  if (names == "ACY" ) { return LAYER_TYPE_YCA;  }
  if (names == "ABGR") { return LAYER_TYPE_RGBA; }
  if (names == "BGR" ) { return LAYER_TYPE_RGB;  }

  return LAYER_TYPE_UNDEFINED;
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

  // create GIMP image (TODO: distinguish between grayscale and rgba)
  if (!create_gimp_image (GIMP_RGB,
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
      const Layer     *layer = m_file.get_layers()[i];
      const LayerType type   = determine_layer_type (*layer);
      switch (type)
        {
          case LAYER_TYPE_RGBA:
            {
              float *r = (float*)m_file.find_channel("R")->get_data();
              float *g = (float*)m_file.find_channel("G")->get_data();
              float *b = (float*)m_file.find_channel("B")->get_data();
              float *a = (float*)m_file.find_channel("A")->get_data();

              const size_t pixel_count = m_file.get_width() 
                                         * m_file.get_height();
              guchar *data = new guchar[pixel_count * 4];
              // copy over the pixels 
              // TODO: do a proper HDR to LDR conversion
              // TODO: vectorize this code
              for (size_t i = 0; i < pixel_count; ++i)
                {
                  data[4 * i + 0] = r[i] * 255.f;
                  data[4 * i + 1] = g[i] * 255.f;
                  data[4 * i + 2] = b[i] * 255.f;
                  data[4 * i + 3] = a[i] * 255.f;
                }

              if (!add_layer (GIMP_RGBA_IMAGE,
                              layer->get_name(),
                              m_file.get_width(),
                              m_file.get_height(),
                              image_id,
                              data,
                              error_msg))
                {
                  delete[] data;
                  return false;
                }

              delete[] data;
              break;
            }
          case LAYER_TYPE_RGB:
          case LAYER_TYPE_UNDEFINED:
          case LAYER_TYPE_Y:
          case LAYER_TYPE_YC:
          case LAYER_TYPE_YA:
          case LAYER_TYPE_YCA:
            {
              error_msg = "not implemented";
              return false;
            }
        }
    }

  return true;
}


/* vim: set ts=2 sw=2 : */
