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


//-----------------------------------------------------------------------------
// Wraps a data channel from the file in memory.
class Channel
{
public:

  enum data_type
  {
    FLOAT = 1,
    HALF  = 2,
    UINT  = 3,
  };

  // Creates a new channel.
  Channel (const std::string &name,
           const data_type   type,
           const size_t      pixel_width,
           const size_t      pixel_height);
          
  // Destroys this channel.
  ~Channel ();

  // Returns the name of this channel.
  const std::string& get_name() const;

  // Returns the data type of this channel.
  data_type get_type() const;

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

  const std::string name_;
  const data_type   type_;
  const size_t      x_stride_;
  const size_t      y_stride_;
  const size_t      line_count_;
  char              *const buffer_;
};


inline const std::string& Channel::get_name() const
{
  return name_;
}


inline Channel::data_type Channel::get_type() const
{
  return type_;
}


inline const char* Channel::get_data() const
{
  return buffer_;
}
  

inline size_t Channel::get_byte_size() const
{
  return y_stride_ * line_count_;
}


inline size_t Channel::get_x_stride() const
{
  return x_stride_;
}


inline size_t Channel::get_y_stride() const
{
  return y_stride_;
}


inline size_t Channel::get_pixel_count() const
{
  return (y_stride_ / x_stride_) * line_count_;
}


//----------------------------------------------------------------------------
// Groups a set of channels into a layer.
class Layer
{
public:  

  // Creates a new layer.
  Layer(const std::string &name);

  // Destroys this layer.
  ~Layer();

  // Returns the name of this layer.
  const std::string& get_name() const;

  // Adds a channel to this layer.
  void add_channel(const Channel *channel);

  // Returns the list of channels in this layers.
  const ConstChannelListT& get_channels() const;

private:

  const std::string name_;
  ConstChannelListT m_channels;
};


inline Layer::Layer(const std::string &name)
:
  name_(name)
{}


inline Layer::~Layer()
{
  m_channels.clear();
}


inline const std::string& Layer::get_name() const
{
  return name_;
}


inline void Layer::add_channel(const Channel *channel)
{
  m_channels.push_back (channel);
}


inline const ConstChannelListT& Layer::get_channels() const
{
  return m_channels;
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
  bool has_channel(const std::string &name);

private:

  // flag indicating successfull disk load
  bool              m_loaded;
  // path to the file on disk
  const std::string path_;
  // width in pixels
  size_t            width_;
  // height in pixels
  size_t            height_;
  // OpenEXR lib file handle
  void              *handle_;
  // list of all the channels
  ConstChannelListT m_channels;
  // list of all the layers
  ConstLayerListT   layers_;
};


inline bool File::is_loaded() const
{
  return m_loaded;
}


inline const std::string& File::get_path() const
{
  return path_;
}


inline size_t File::get_width() const
{
  return width_;
}


inline size_t File::get_height() const
{
  return height_;
}


inline const ConstLayerListT& File::get_layers() const
{
  return layers_;
}


inline size_t File::get_layer_count() const
{
  return layers_.size();
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
