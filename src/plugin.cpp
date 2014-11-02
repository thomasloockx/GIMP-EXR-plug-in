// C includes
#include <stdlib.h>
#include <string.h>
// gimp includes
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
// plugin includes
#include "exr_file.hpp"

// list of comma seperated file extensions that work for OpenEXR
static const char *FILE_EXTENSIONS = "exr,EXR";
// name of the load procedure in the PDB
static const char *LOAD_PROCEDURE = "file-exr-load";


// Converts an EXR layer into Gimp 8-bit color data.
static guchar*
convert_layer_to_8_bit (const exr::Layer &layer)
{
  const exr::ConstChannelListT &channels = layer.get_channels();
  if (channels.size() != 4)
    {
    return 0;
    }

  // 8-bit color data
  guchar *dest = new guchar[channels[0]->get_byte_size()];

  const float *r_channel = (const float*)channels[3]->get_data();
  const float *g_channel = (const float*)channels[2]->get_data();
  const float *b_channel = (const float*)channels[1]->get_data();
  const float *a_channel = (const float*)channels[0]->get_data();

  // copy over the pixels (TODO: vectorize this code)
  for (size_t i = 0; i < channels[0]->get_pixel_count(); ++i)
  {
    dest[4 * i + 0] = r_channel[i] * 255.f;
    dest[4 * i + 1] = g_channel[i] * 255.f;
    dest[4 * i + 2] = b_channel[i] * 255.f;
    dest[4 * i + 3] = a_channel[i] * 255.f;
  }

  return dest;
}


/* Needs to return plugin info to the GIMP.
 */
static void
query (void)
{
  static GimpParamDef load_args[] =
  {
    {
      GIMP_PDB_INT32,
      "run-mode",
      "Run mode"
    },
    {
      GIMP_PDB_STRING,
      "filename",
      "The name of the file to load"
    },
    {
      GIMP_PDB_STRING,
      "raw-filename",
      "The name of the file to load",
    }
  };

  static GimpParamDef load_return_vals[] =
  {
    {
      GIMP_PDB_IMAGE,
      "image",
      "Output image",
    }
  };

  gimp_install_procedure ("file-exr-load",
                          "OpenEXR Import",
                          "Imports OpenEXR files into the GIMP.",
                          "Thomas Loockx",
                          "Thomas Loockx",
                          "2014",
                          "<Load>/EXR",
                          0,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS(load_args),
                          G_N_ELEMENTS(load_return_vals),
                          load_args,
                          load_return_vals);
  gimp_register_file_handler_mime (LOAD_PROCEDURE, "image/x-exr");
  gimp_register_load_handler (LOAD_PROCEDURE, FILE_EXTENSIONS, "");
}


static void
run(const gchar      *name,
    gint              nparams,
    const GimpParam  *param,
    gint             *nreturn_vals,
    GimpParam       **return_vals)
{
  static GimpParam  return_values[2];
  GimpRunMode       run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  // get our run mode
  run_mode        = (GimpRunMode)param[0].data.d_int32;
  gchar *filename = param[1].data.d_string;

  exr::File   file (filename);
  std::string error_msg = "";
  gint32      image_id  = -1;

  // read the exr file and load each layer as a gimp layer
  if (file.load(error_msg))
    {
    // set up a new image
    image_id = gimp_image_new (file.get_width(), file.get_height(), GIMP_RGB);
    if (image_id != -1)
      {
      // create a Gimp layer for each EXR layer
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
          g_message("failed to create layer for \"%s\"", layer->get_name().c_str());
          status = GIMP_PDB_EXECUTION_ERROR;
          break;
          }

        if (!gimp_image_insert_layer(image_id, layer_id, 0, -1))
          {
          g_message("failed to add layer for \"%s\"", layer->get_name().c_str());
          status = GIMP_PDB_EXECUTION_ERROR;
          break;
          }

        // create a drawable for the current layer
        GimpDrawable *drawable = gimp_drawable_get (layer_id);
        if (!drawable)
          {
          g_message("failed to get drawable for layer \"%s\"",
                    layer->get_name().c_str());
          status = GIMP_PDB_EXECUTION_ERROR;
          break;
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
      }
    else
      {
        g_message("failed to create image\n");
        status = GIMP_PDB_EXECUTION_ERROR;
      }
    }

  // init return values
  return_values[0].type          = GIMP_PDB_STATUS;
  return_values[0].data.d_status = status;
  return_values[1].type          = GIMP_PDB_IMAGE;
  return_values[1].data.d_image  = image_id;
  *nreturn_vals                  = 2;
  *return_vals                   = return_values;
}


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,
  NULL,
  query,
  run
};


MAIN()

/* vim: set ts=2 sw=2 : */
