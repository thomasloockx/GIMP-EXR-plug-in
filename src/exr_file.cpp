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

// Searches for a channel in the list O(N)
static bool contains_channel (const ChannelListT &channels,
                              const std::string  &name)
{
  for (size_t i = 0; i < channels.size(); ++i)
    {
      if (channels[i]->get_name() == name)
        {
          return true;
        }
    }
  return false;
}


// Given a channel list, determines the image type.
static image_type channels_to_image_type 
  (const ConstChannelListT &channels)
{
  if (channels.empty() || channels.size() > 4) { return IMAGE_TYPE_UNDEFINED; }

  // create a string with the channel names, sort it and compare it with the
  // possible channel combinations
  std::string letters;
  for (size_t i = 0; i < channels.size(); ++i)
    {
      letters += channels[i]->get_name();
    }
  std::sort(letters.begin(), letters.end());

  if (letters == "Y"    ) { return IMAGE_TYPE_Y;      }
  if (letters == "BCRRY") { return IMAGE_TYPE_CHROMA; }
  if (letters == "BGR"  ) { return IMAGE_TYPE_RGB;    }
  if (letters == "ABGR" ) { return IMAGE_TYPE_RGBA;   }
  if (letters == "CY"   ) { return IMAGE_TYPE_YC;     }
  if (letters == "AY"   ) { return IMAGE_TYPE_YA;     }
  if (letters == "ACY"  ) { return IMAGE_TYPE_YC;     }

  return IMAGE_TYPE_UNDEFINED;
}



//-----------------------------------------------------------------------------
// Implementation of Channel


Channel::Channel(const std::string &name,
                 const data_type   type,
                 const size_t      pixel_width,
                 const size_t      pixel_height)
:
  m_name(name),
  m_type(type),
  m_x_stride(type == DATA_TYPE_HALF ? 2 : 4),
  m_y_stride(m_x_stride * pixel_width),
  m_line_count(pixel_height),
  m_buffer(new char[m_y_stride * m_line_count])
{}
 

Channel::~Channel()
{
  delete[] m_buffer;
}



//-----------------------------------------------------------------------------
// Implementation of Layer


const image_type Layer::get_image_type() const
{
  return channels_to_image_type (m_channels);
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
                                     DATA_TYPE_UINT,
                                     m_width,
                                     m_height);
              break;
              }
            case Imf::FLOAT:
              {
              channel = new Channel (it.name(), 
                                     DATA_TYPE_FLOAT,
                                     m_width,
                                     m_height);
              break;
              }
            case Imf::HALF:
              {
              channel = new Channel (it.name(), 
                                     DATA_TYPE_HALF,
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
                      layer->add_channel(m_channels[i]); 
                      break;
                    }
                }
          }
          m_layers.push_back (layer);
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


const image_type File::get_image_type() const
{
  return channels_to_image_type (m_channels);
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
