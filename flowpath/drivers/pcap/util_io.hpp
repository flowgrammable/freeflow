#ifndef GZ_OSTREAM_HPP
#define GZ_OSTREAM_HPP

#include <fstream>

#include <boost/iostreams/filtering_streambuf.hpp>


/// OUTPUT GZ STREAM ///
class gz_ostream {
public:
  gz_ostream() = delete;
  gz_ostream(const std::string& filename);
  gz_ostream(gz_ostream& other) = delete;
  gz_ostream(gz_ostream&& other) = default;
  ~gz_ostream();
  std::ostream& get_ostream() const;

private:
  std::fstream os_file;
  std::unique_ptr< boost::iostreams::filtering_streambuf<boost::iostreams::output> > gzip_buf;
  std::unique_ptr<std::ostream> os;
};


/// INPUT GZ STREAM ///
class gz_istream {
public:
  gz_istream() = delete;
  gz_istream(const std::string& filename);
  gz_istream(gz_istream& other) = delete;
  gz_istream(gz_istream&& other) = default;
  ~gz_istream() = default;
  std::istream& get_ostream() const;

private:
  std::fstream os_file;
  std::unique_ptr< boost::iostreams::filtering_streambuf<boost::iostreams::input> > gzip_buf;
  std::unique_ptr<std::istream> os;
};


#endif // GZ_OSTREAM_HPP
