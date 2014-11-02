// GIMP includes
#include <libgimp/gimp.h>
// plugin includes
#include "exr_file.hpp"
// myself
#include "conversion.hpp"


// Converts an EXR layer into Gimp 8-bit color data.
static guchar*
convert_layer_to_8_bit (const exr::Layer &layer)
{
  // 8-bit color data
  guchar *dest = new guchar[layer.get_channels()[0]->get_byte_size()];

  const float *r_channel = (const float*)layer.get_channels()[3]->get_data();
  const float *g_channel = (const float*)layer.get_channels()[2]->get_data();
  const float *b_channel = (const float*)layer.get_channels()[1]->get_data();
  const float *a_channel = (const float*)layer.get_channels()[0]->get_data();

  // copy over the pixels (TODO: vectorize this code)
  for (size_t i = 0; i < layer.get_channels()[0]->get_pixel_count(); ++i)
    {
      dest[4 * i + 0] = r_channel[i] * 255.f;
      dest[4 * i + 1] = g_channel[i] * 255.f;
      dest[4 * i + 2] = b_channel[i] * 255.f;
      dest[4 * i + 3] = a_channel[i] * 255.f;
    }

  return dest;
}


bool convert_to_8_bit (const exr::File &file,
                       gint32          &image_id,
                       std::string     &error_msg)
{
  image_id = -1;
  error_msg.clear();

  // not much we can do if the file isn't in memory yet
  if (!file.is_loaded())
    {
      error_msg = "file not loaded in memory";
      return false;
    }

  // create a new gimp image
  image_id = gimp_image_new (file.get_width(), file.get_height(), GIMP_RGB);
  if (image_id == -1)
    {
      error_msg = "failed to create GIMP image";
      return false;
    }

  // create a GIMP layer for each layer in the file
  const exr::ConstLayerListT &layers = file.get_layers(); 
  for (int i = (int)layers.size() - 1; i >= 0; --i)
  {
    const exr::Layer *layer = layers[i];

    gint32 layer_id = gimp_layer_new (image_id,
        layer->get_name().c_str(),
        file.get_width(),
        file.get_height(),
        GIMP_RGBA_IMAGE, 
        100.0,
        GIMP_NORMAL_MODE);
    if (layer_id == -1)
      {
        error_msg = "failed to create layer";
        return false;
      }

    if (!gimp_image_insert_layer(image_id, layer_id, 0, -1))
      {
        error_msg = "failed to add layer";
        return false;
      }

    // create a drawable for the current layer
    GimpDrawable *drawable = gimp_drawable_get (layer_id);
    if (!drawable)
      {
        error_msg = "failed to get drawable for layer";
        return false;
      }

    // init a pixel region that contains the full layer
    GimpPixelRgn pixel_region;
    gimp_pixel_rgn_init (&pixel_region,
        drawable,
        0, 0,
        file.get_width(), file.get_height(),
        TRUE,
        TRUE);

    // convert floating point to 8-bit
    guchar *data_8_bit = convert_layer_to_8_bit (*layer);
    gimp_pixel_rgn_set_rect (&pixel_region,
        data_8_bit,
        0,
        0,
        pixel_region.w,
        pixel_region.h);
    delete[] data_8_bit;

    // update drawable
    gimp_drawable_flush (drawable);
    gimp_drawable_merge_shadow (drawable->drawable_id, FALSE);
    gimp_drawable_update (drawable->drawable_id,
        0, 0,
        file.get_width(), file.get_height());
    // we're done drawing
    gimp_item_delete (drawable->drawable_id); 
  }

  return true;
}


/* vim: set ts=2 sw=2 : */
