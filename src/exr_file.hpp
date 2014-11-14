#ifndef _EXR_FILE_HPP_
#define _EXR_FILE_HPP_ 1

// system includes
#include <map>
#include <string>
#include <vector>


namespace exr
{

class Channel;
class File;
class Layer;

typedef std::map<std::string, size_t> ChannelMapT;
typedef std::vector<Channel*>         ChannelListT;
typedef std::vector<const Channel*>   ConstChannelListT;
typedef std::vector<Layer*>           LayerListT;
typedef std::vector<const Layer*>     ConstLayerListT;;


enum PixelDataType
{
  PIXEL_DATA_TYPE_FLOAT = 1,
  PIXEL_DATA_TYPE_HALF  = 2,
  PIXEL_DATA_TYPE_UINT  = 3,
};


//-----------------------------------------------------------------------------
// Wraps a data channel from the file in memory.
class Channel
{
public:

  // Creates a new channel.
  Channel (const std::string   &name,
           const PixelDataType type,
           const size_t        pixel_width,
           const size_t        pixel_height);
          
  // Destroys this channel.
  ~Channel ();

  // Returns the name of this channel.
  const std::string& get_name() const;

  // Returns the layer that contains this channel.
  const Layer* get_layer() const;

  // Returns the pixel data type of this channel.
  PixelDataType get_pixel_data_type() const;

  // Fetches the raw data pointer. Pixel (x, y)'s index is calculated with:
  // index = x * x_stride + y * y_stride
  const char* get_data() const;
  
  // Returns the size of the data in bytes.
  size_t get_byte_size() const;

  // Returns x-stride in bytes, this is also the size of a singe data element.
  size_t get_x_stride() const;

  // Returns the y-stride in bytes.
  size_t get_y_stride() const;

  // Returns the number of pixels in this channel.
  size_t get_pixel_count() const;

private:

  friend class Layer;

  const std::string   m_name;
  const Layer*        m_layer;
  const PixelDataType m_pixel_data_type;
  const size_t        m_x_stride;
  const size_t        m_y_stride;
  const size_t        m_line_count;
  char                *const m_buffer;

  // internal function to set a layer
  void set_layer(const Layer *layer);
};


inline const std::string& Channel::get_name() const
{
  return m_name;
}


inline const Layer* Channel::get_layer() const
{
  return m_layer;
}


inline void Channel::set_layer(const Layer *layer)
{
  m_layer = layer;
}


inline PixelDataType Channel::get_pixel_data_type() const
{
  return m_pixel_data_type;
}


inline const char* Channel::get_data() const
{
  return m_buffer;
}
  

inline size_t Channel::get_byte_size() const
{
  return m_y_stride * m_line_count;
}


inline size_t Channel::get_x_stride() const
{
  return m_x_stride;
}


inline size_t Channel::get_y_stride() const
{
  return m_y_stride;
}


inline size_t Channel::get_pixel_count() const
{
  return (m_y_stride / m_x_stride) * m_line_count;
}



//----------------------------------------------------------------------------
// Groups a set of channels into a layer. Each channel is always on a layer.
class Layer
{
public:  

  // Creates a new layer.
  Layer(const std::string &name);

  // Destroys this layer.
  ~Layer();

  // Returns the name of this layer.
  const std::string& get_name() const;

  // Adds a channel to this layer. The layer will also set itself
  // as the "parent" of the channel.
  void add_channel(Channel *channel);

  // Returns the list of channels in this layer.
  const ConstChannelListT& get_channels() const;

  // Returns the number of channels.
  size_t get_channel_count() const;

  // Checks if we have a channel with the given name.
  bool has_channel(const std::string &name) const;

private:

  const std::string m_name;
  ConstChannelListT m_channels;
};


inline Layer::Layer(const std::string &name)
:
  m_name(name)
{}


inline Layer::~Layer()
{
  m_channels.clear();
}


inline const std::string& Layer::get_name() const
{
  return m_name;
}


inline void Layer::add_channel(Channel *channel)
{
  m_channels.push_back (channel);
  channel->set_layer (this);
}


inline const ConstChannelListT& Layer::get_channels() const
{
  return m_channels;
}


inline size_t Layer::get_channel_count() const
{
  return m_channels.size();
}


//-----------------------------------------------------------------------------
// Wraps the data in an OpenEXR file. Once the file is loaded, all the data
// is loaded into memory.
class File 
{
public:

  // Creates an file, this will not read the file.
  File(const std::string &path); 

  // Destroys this file.
  ~File();

  // Loads the exr file into memory. Returns true on success, false on failure.
  // On failure the error message should contain something meaningfull.
  bool load(std::string &error_msg);

  // Checks if the file was successfully loaded in memory.
  bool is_loaded() const;

  // Returns the load path of this file.
  const std::string& get_path() const;

  // Returns the width of the file in pixels.
  size_t get_width() const;

  // Returns the height of the file in pixels.
  size_t get_height() const;

  // Returns the list of all layers.
  const ConstLayerListT& get_layers() const;

  // Returns the number of layers.
  size_t get_layer_count() const;

  // Returns a list of all the channels.
  const ConstChannelListT& get_channels() const;

  // Returns the total number of channels.
  size_t get_channel_count() const;

  // Checks if we have a channel with the given name.
  bool has_channel(const std::string &name) const;

  // Returns a particular channel by name. Returns NULL if no channel was
  // found in this image.
  const Channel* find_channel (const std::string &name) const;

private:

  // flag indicating successfull disk load
  bool              m_loaded;
  // path to the file on disk
  const std::string m_path;
  // width in pixels
  size_t            m_width;
  // height in pixels
  size_t            m_height;
  // OpenEXR lib file handle
  void              *m_handle;
  // list of all the channels in this file (nested in layers as well)
  ConstChannelListT m_channels;
  // list of all the layers in this file
  ConstLayerListT   m_layers;
};


inline bool File::is_loaded() const
{
  return m_loaded;
}


inline const std::string& File::get_path() const
{
  return m_path;
}


inline size_t File::get_width() const
{
  return m_width;
}


inline size_t File::get_height() const
{
  return m_height;
}


inline const ConstLayerListT& File::get_layers() const
{
  return m_layers;
}


inline size_t File::get_layer_count() const
{
  return m_layers.size();
}


inline const ConstChannelListT& File::get_channels() const
{
  return m_channels;
}


inline size_t File::get_channel_count() const
{
  return m_channels.size();
}

 
} // namespace exr


#endif // #ifndef _EXR_FILE_HPP_


/* vim: set ts=2 sw=2 : */
