#ifndef GZ_OSTREAM_HPP
#define GZ_OSTREAM_HPP

//#include <iosfwd>
//#include <ostream>
#include <fstream>

#include <boost/iostreams/filtering_streambuf.hpp>


class gz_ostream {
public:
  gz_ostream() = delete;
  gz_ostream(const std::string& filename);
  gz_ostream(gz_ostream& other) = delete;
  gz_ostream(gz_ostream&& other) = default;
  ~gz_ostream();
  std::ostream& get_ostream() const;

private:
  std::ofstream os_file;
  std::unique_ptr< boost::iostreams::filtering_streambuf<boost::iostreams::output> > gzip_buf;
  std::unique_ptr<std::ostream> os;
};


#endif // GZ_OSTREAM_HPP
