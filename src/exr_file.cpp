// OpenEXR includes
#include "ImfChannelList.h"
#include "ImfHeader.h"
#include "ImfInputFile.h"
#include "ImfTestFile.h"
// myself
#include "exr_file.hpp"

using namespace exr;

Channel::Channel(const std::string &name,
                 const data_type   type,
                 const size_t      pixel_width,
                 const size_t      pixel_height)
:
  name_(name),
  type_(type),
  x_stride_(type == HALF ? 2 : 4),
  y_stride_(x_stride_ * pixel_width),
  line_count_(pixel_height),
  buffer_(new char[y_stride_ * line_count_])
{}
 

Channel::~Channel()
{
  delete[] buffer_;
}


File::File(const std::string &path)
:
  m_loaded(false),
  path_(path),
  width_(0),
  height_(0),
  handle_(NULL)
{}


File::~File()
{
  // delete layers
  for (size_t i = 0; i < layers_.size(); ++i)
    {
      delete layers_[i];
    }
  layers_.clear();
  // delete channels 
  for (size_t i = 0; i < m_channels.size(); ++i)
    {
    delete m_channels[i];
    }
  m_channels.clear();
  // cleanup OpenEXR file handle
  delete (Imf::InputFile*)handle_;
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
      Imf::InputFile *file = new Imf::InputFile(path_.c_str());
      handle_              = (void*)file;

      // extract the header info
      const Imf::Header &header = file->header();
      Imath::Box2i data_window  = header.dataWindow();
      width_                    = data_window.max.x - data_window.min.x + 1;
      height_                   = data_window.max.y - data_window.min.y + 1;

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
                                     Channel::UINT,
                                     width_,
                                     height_);
              break;
              }
            case Imf::FLOAT:
              {
              channel = new Channel (it.name(), 
                                     Channel::FLOAT,
                                     width_,
                                     height_);
              break;
              }
            case Imf::HALF:
              {
              channel = new Channel (it.name(), 
                                     Channel::HALF,
                                     width_,
                                     height_);
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
          layers_.push_back (layer);
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


bool File::has_channel(const std::string &name)
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


/* vim: set ts=2 sw=2 : */
