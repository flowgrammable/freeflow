#ifndef FF_UTIL_IO_HPP
#define FF_UTIL_IO_HPP

#include <fstream>

#include <boost/iostreams/filtering_streambuf.hpp>


/// OUTPUT GZ STREAM ///
class gz_ostream {
public:
  gz_ostream(const std::string& filename);
  ~gz_ostream();

  gz_ostream(const gz_ostream&) = delete;
  gz_ostream(gz_ostream&&) = default;
  gz_ostream& operator=(const gz_ostream&) = delete;
  gz_ostream& operator=(gz_ostream&&) = default;

  std::ostream& get_ostream() const;
  void flush();

private:
  std::fstream os_file;
  std::unique_ptr< boost::iostreams::filtering_streambuf<boost::iostreams::output> > gzip_buf;
  std::unique_ptr<std::ostream> os;
};


/// INPUT GZ STREAM ///
class gz_istream {
public:
  gz_istream(const std::string& filename);
  ~gz_istream() = default;

  gz_istream(const gz_istream&) = delete;
  gz_istream(gz_istream&&) = default;
  gz_istream& operator=(const gz_istream&) = delete;
  gz_istream& operator=(gz_istream&&) = default;

  std::istream& get_ostream() const;

private:
  std::fstream os_file;
  std::unique_ptr< boost::iostreams::filtering_streambuf<boost::iostreams::input> > gzip_buf;
  std::unique_ptr<std::istream> os;
};


#endif // FF_UTIL_IO_HPP
