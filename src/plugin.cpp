// C includes
#include <stdlib.h>
#include <string.h>
// GIMP includes
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
// plugin includes
#include "conversion.hpp"
#include "exr_file.hpp"

// list of comma seperated file extensions that work for OpenEXR
static const char *FILE_EXTENSIONS = "exr,EXR";
// name of the load procedure in the PDB
static const char *LOAD_PROCEDURE = "file-exr-load";


// Returns plugin info to the GIMP.
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


// Runs the plugin.
static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam  return_values[2];
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  // get our run mode
  GimpRunMode run_mode  = (GimpRunMode)param[0].data.d_int32;
  gchar       *filename = param[1].data.d_string;

  exr::File   file (filename);
  std::string error_msg = "";
  gint32      image_id  = -1;

  // read the exr file
  if (file.load(error_msg))
    {
      // TODO: configurable settings
      ConversionSettings settings;
      settings.m_gamma = 2.2f;
      // create converter and do the conversion
      Converter converter (file, settings);
      if (!converter.convert (image_id, error_msg))
        {
          g_message("%s\n", error_msg.c_str());
          status = GIMP_PDB_EXECUTION_ERROR;
        }
    }
  else
    {
      status = GIMP_PDB_EXECUTION_ERROR;
    }

  // fill in the return values (status & image id)
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
