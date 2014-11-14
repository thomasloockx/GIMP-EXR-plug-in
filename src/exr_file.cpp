// C++ includes
#include <algorithm>
// OpenEXR includes
#include "ImfChannelList.h"
#include "ImfHeader.h"
#include "ImfInputFile.h"
#include "ImfTestFile.h"
// myself
#include "exr_file.hpp"

using namespace exr;


//-----------------------------------------------------------------------------
// Implementation of Channel


Channel::Channel(const std::string   &name,
                 const PixelDataType type,
                 const size_t        pixel_width,
                 const size_t        pixel_height)
:
  m_name(name),
  m_layer(NULL),
  m_pixel_data_type(type),
  m_x_stride(type == PIXEL_DATA_TYPE_HALF ? 2 : 4),
  m_y_stride(m_x_stride * pixel_width),
  m_line_count(pixel_height),
  m_buffer(new char[m_y_stride * m_line_count])
{}
 

Channel::~Channel()
{
  delete[] m_buffer;
}



//-----------------------------------------------------------------------------
// Implementation of File


File::File(const std::string &path)
:
  m_loaded(false),
  m_path(path),
  m_width(0),
  m_height(0),
  m_handle(NULL)
{}


File::~File()
{
  // delete layers (layers will delete their channels)
  for (size_t i = 0; i < m_layers.size(); ++i)
    {
      delete m_layers[i];
    }
  m_layers.clear();
  // cleanup OpenEXR file handle
  delete (Imf::InputFile*)m_handle;
}


bool File::load(std::string &error_msg)
{
  // don't bother if it's not an OpenEXR file
  if (!Imf::isOpenExrFile(get_path().c_str()))
    {
      error_msg = "file is not an OpenEXR file";
      return false;
    }

  try
    {
      // open file and keep track of it
      Imf::InputFile *file = new Imf::InputFile(m_path.c_str());
      m_handle             = (void*)file;

      // extract the header info
      const Imf::Header &header = file->header();
      Imath::Box2i data_window  = header.dataWindow();
      m_width                   = data_window.max.x - data_window.min.x + 1;
      m_height                  = data_window.max.y - data_window.min.y + 1;

      // stores pointers to the data to read out of the file
      Imf::FrameBuffer frame_buffer;

      // list of all the channels in the file
      const Imf::ChannelList &channel_list = header.channels();
      // collect all the channels
      for (Imf::ChannelList::ConstIterator it = channel_list.begin();
           it != channel_list.end();
           ++it)
        {
          std::string channel_name;
          std::string layer_name;
          split_full_channel_name (it.name(), layer_name, channel_name);

          // create the new channel
          Channel *channel = NULL; 
          switch (it.channel().type)
            {
            case Imf::UINT:
              {
              channel = new Channel (channel_name,
                                     PIXEL_DATA_TYPE_UINT,
                                     m_width,
                                     m_height);
              break;
              }
            case Imf::FLOAT:
              {
              channel = new Channel (channel_name,
                                     PIXEL_DATA_TYPE_FLOAT,
                                     m_width,
                                     m_height);
              break;
              }
            case Imf::HALF:
              {
              channel = new Channel (channel_name,
                                     PIXEL_DATA_TYPE_HALF,
                                     m_width,
                                     m_height);
              break;
              }
            }

          // find or create the layer
          Layer *layer = NULL;
          if (!find_layer (layer_name, (const Layer**)&layer))
            {
              layer = new Layer (layer_name);
              insert_layer (layer);
            }
          // track the channel
          layer->insert_channel (channel);

          // register channels' buffer with fame buffer
          frame_buffer.insert (it.name(),
                               Imf::Slice (it.channel().type,
                                           (char*)channel->get_data(),
                                           channel->get_x_stride(),
                                           channel->get_y_stride(),
                                           it.channel().xSampling,
                                           it.channel().ySampling,
                                           0.f));
        }

      // read out all the data in one sweep 
      file->setFrameBuffer(frame_buffer);
      file->readPixels(data_window.min.y, data_window.max.y);
    }
  catch (std::exception &e)
    {
      error_msg = e.what();
      return false;
    }

  m_loaded = true;
  return true;
}


void File::split_full_channel_name (const std::string &input,
                                    std::string       &layer_name,
                                    std::string       &channel_name)
{
  const size_t last_dot_ix = std::string(input).find_last_of ('.');
  if (last_dot_ix == std::string::npos)
    {
      layer_name   = "";
      channel_name = input;
    }
  else
    {
      layer_name   = input.substr(0, last_dot_ix);
      channel_name = input.substr(last_dot_ix+1);
    }
}



/* vim: set ts=2 sw=2 : */
