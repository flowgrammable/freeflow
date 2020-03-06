#ifndef FF_UTIL_IO_HPP
#define FF_UTIL_IO_HPP

#include <fstream>
#include <sstream>

#include <boost/iostreams/filtering_streambuf.hpp>
#include "types.hpp"


// Compile-time Output Disable:
static struct NULL_OFS {} NULL_OFS_i;
template<typename T> NULL_OFS& operator <<(NULL_OFS& os, const T& x) {return os;}
NULL_OFS& operator <<(NULL_OFS& os, std::ostream&(*)(std::ostream&));


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


/// Templated CSV List Class ///
class CSV {
public:
  CSV() = default;
  CSV(std::string filename) : f_(filename, std::ios::out) {
//    f_ << '{' << std::endl;
  }
  ~CSV() {
//    f_ << "\n}" << std::endl;
  }

  template<typename Tuple>
  size_t print(Tuple t) {
//    if (lines_ > 0)
//      f_ << ",\n";

    std::string s;
    for_each(t, [&](int x) {
      s.append(std::to_string(x) + ',');
    });
//    s.pop_back();   // remove last ','
    s.back() = '\n';

//    f_ << '{' << s << '}';
    f_ << s;
    return ++lines_;
  }

private:
  std::ofstream f_;
  size_t lines_ = 0;
};


#endif // FF_UTIL_IO_HPP
