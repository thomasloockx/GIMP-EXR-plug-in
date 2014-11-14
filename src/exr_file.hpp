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

  // Returns the number of channels.
  size_t get_channel_count() const;

  // Returns the channel with the specified name.
  const Channel* get_channel (const std::string &name) const;

  // Returns the channel at the specified index.
  const Channel* get_channel_at (const size_t index) const;

  // Checks if we have a channel with the given name.
  bool find_channel (const std::string &name,
                     const Channel     *channel) const;

private:

  friend class File;

  typedef std::map <std::string, size_t> IndexT;
  typedef std::vector<const Channel*>    ConstChannelListT;

  // name of this channel
  const std::string m_name;
  // index from channel name to it's index
  IndexT            m_index;
  // list of the channels
  ConstChannelListT m_channels;

  // Inserts a channel into this layer. The layer parents itself to the channel
  // and also deletes the channel when this layer itself is deleted.
  void insert_channel (Channel *channel);
};


inline Layer::Layer(const std::string &name)
:
  m_name(name)
{}


inline Layer::~Layer()
{
  for (size_t i = 0; i < m_channels.size(); ++i)
    {
      delete m_channels[i];
    }
  m_channels.clear();
}


inline const std::string& Layer::get_name() const
{
  return m_name;
}


inline size_t Layer::get_channel_count() const
{
  return m_channels.size();
}


inline const Channel* Layer::get_channel (const std::string &name) const
{
  IndexT::const_iterator found = m_index.find(name);
  if (found != m_index.end())
    {
      return m_channels[found->second];
    }
  return NULL;
}


inline const Channel* Layer::get_channel_at (const size_t index) const
{
  if (index < m_channels.size())
    {
      return m_channels[index];
    }
  return NULL;
}


inline bool Layer::find_channel (const std::string &name,
                                 const Channel     *channel) const
{
  IndexT::const_iterator found = m_index.find(name);
  if (found == m_index.end())
    {
      channel = NULL;
      return false;
    }
  else
    {
      channel = m_channels[found->second];
      return true;
    }
}


inline void Layer::insert_channel (Channel *channel)
{
  m_index.insert (std::make_pair(channel->get_name(), m_channels.size()));
  m_channels.push_back (channel);
  channel->set_layer (this);
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

  // Returns the number of layers.
  size_t get_layer_count() const;

  // Finds a layer by name, returns false if no such layer can be found.
  bool find_layer (const std::string &name,
                   const Layer       **layer) const;

  // Returns the layer at the specified index.
  const Layer* get_layer_at (size_t index) const;

private:

  typedef std::map <std::string, size_t> IndexT;
  typedef std::vector <const Layer*>     ConstLayerListT;

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
  // layer name index
  IndexT            m_index;
  // list of all the layers in this file.
  ConstLayerListT   m_layers;

  // inserts a layer
  void insert_layer (Layer *layer);

  // splits a full channel name (e.g. AO.G into AO & G)
  static void split_full_channel_name (const std::string &input,
                                       std::string       &layer_name,
                                       std::string       &channel_name);
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


inline size_t File::get_layer_count() const
{
  return m_layers.size();
}


inline bool File::find_layer (const std::string &name,
                              const Layer       **layer) const
{
  IndexT::const_iterator found = m_index.find (name);
  if (found == m_index.end())
    {
      *layer = NULL;
      return false;
    }
  else
    {
      *layer = m_layers[found->second];
      return true;
    }
}


inline const Layer* File::get_layer_at (size_t index) const
{
  if (index < m_layers.size())
    {
      return m_layers[index];
    }
  return NULL;
}


inline void File::insert_layer (Layer *layer)
{
  m_index.insert(std::make_pair(layer->get_name(), m_layers.size()));
  m_layers.push_back(layer);
}


} // namespace exr


#endif // #ifndef _EXR_FILE_HPP_


/* vim: set ts=2 sw=2 : */
