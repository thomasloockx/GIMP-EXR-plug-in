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
  // delete layers
  for (size_t i = 0; i < m_layers.size(); ++i)
    {
      delete m_layers[i];
    }
  m_layers.clear();
  // delete channels 
  for (size_t i = 0; i < m_channels.size(); ++i)
    {
    delete m_channels[i];
    }
  m_channels.clear();
  // cleanup OpenEXR file handle
  delete (Imf::InputFile*)m_handle;
}


bool File::load(std::string &error_msg)
{
  // don't bother if it's not an OpenEXR file
  if (!Imf::isOpenExrFile(get_path().c_str()))
    {
      error_msg = "file is not a valid OpenEXR file";
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

      Imf::FrameBuffer frame_buffer;

      // list of all the channels in the file
      const Imf::ChannelList &channel_list = header.channels();

      // collect all the channels
      for (Imf::ChannelList::ConstIterator it = channel_list.begin();
           it != channel_list.end();
           ++it)
        {
          // create the new channel
          Channel *channel = NULL; 
          switch (it.channel().type)
            {
            case Imf::UINT:
              {
              channel = new Channel (it.name(), 
                                     PIXEL_DATA_TYPE_UINT,
                                     m_width,
                                     m_height);
              break;
              }
            case Imf::FLOAT:
              {
              channel = new Channel (it.name(), 
                                     PIXEL_DATA_TYPE_FLOAT,
                                     m_width,
                                     m_height);
              break;
              }
            case Imf::HALF:
              {
              channel = new Channel (it.name(), 
                                     PIXEL_DATA_TYPE_HALF,
                                     m_width,
                                     m_height);
              break;
              }
            }

          // track the channel
          m_channels.push_back(channel);

          // register channels' buffer with fame buffer
          frame_buffer.insert (channel->get_name().c_str(),
                               Imf::Slice (it.channel().type,
                                           (char*)channel->get_data(),
                                           channel->get_x_stride(),
                                           channel->get_y_stride(),
                                           it.channel().xSampling,
                                           it.channel().ySampling,
                                           0.f));
        }

      // get the layer names
      std::set<std::string> layer_names;
      channel_list.layers(layer_names);

      // assign the channels to layers
      for (std::set<std::string>::const_iterator it = layer_names.begin();
           it != layer_names.end();
           ++it)
        {
          Layer *layer = new Layer(*it);

          // add the channels for this layer
          Imf::ChannelList::ConstIterator first, last;
          channel_list.channelsInLayer(*it, first, last);
          for (Imf::ChannelList::ConstIterator cIt = first; cIt != last; ++cIt)
            {
              const std::string channel_name = cIt.name();
              // find this layer
              for (size_t i = 0; i < m_channels.size(); ++i)
                {
                  if (m_channels[i]->get_name() == channel_name)
                    {
                      layer->add_channel((Channel*)m_channels[i]); 
                      break;
                    }
                }
          }
          m_layers.push_back (layer);
        }

      // put all orphaned channels on a layer
      Layer *main_layer = NULL;
      for (size_t i = 0; i < m_channels.size(); ++i)
        {
          if (!m_channels[i]->get_layer())
            {
              if (!main_layer)
              {
                main_layer = new Layer("");
                m_layers.push_back(main_layer);
              }
              main_layer->add_channel((Channel*)m_channels[i]);
            }
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


bool File::has_channel(const std::string &name) const
{
  for (size_t i = 0; i < m_channels.size(); ++i)
    {
      if (m_channels[i]->get_name() == name)
        {
          return true;
        }
    }

    return false;
}


const Channel* File::find_channel (const std::string &name) const
{
  for (size_t i = 0; i < m_channels.size(); ++i)
    {
      if (m_channels[i]->get_name() == name)
        {
          return m_channels[i];
        }
    }
  return NULL;
}


/* vim: set ts=2 sw=2 : */
