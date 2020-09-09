#include "util_io.hpp"

#include <ostream>
#include <fstream>
#include <memory>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>

using namespace std;

NULL_OFS& operator <<(NULL_OFS& os, std::ostream&(*)(std::ostream&)) {return os;}

/// OUTPUT GZ STREAM ///
gz_ostream::gz_ostream(const std::string& filename) {
  os_file.open(filename, ios::out | ios::binary);
  gzip_buf = make_unique< boost::iostreams::filtering_streambuf<boost::iostreams::output> >();
  gzip_buf->push(boost::iostreams::gzip_compressor());
  gzip_buf->push(os_file);
  os = make_unique<ostream>(gzip_buf.get());
}

gz_ostream::~gz_ostream() {
  if (os) {
    os->flush();
  }
}

ostream& gz_ostream::get_ostream() const {
  return *os;
}

void gz_ostream::flush() {
  os->flush();
  os_file.flush();
}


/// INPUT GZ STREAM ///
gz_istream::gz_istream(const std::string& filename) {
  os_file.open(filename, ios::out | ios::binary);
  gzip_buf = make_unique< boost::iostreams::filtering_streambuf<boost::iostreams::input> >();
  gzip_buf->push(boost::iostreams::gzip_compressor());
  gzip_buf->push(os_file);
  os = make_unique<istream>(gzip_buf.get());
}

istream& gz_istream::get_ostream() const {
  return *os;
}


/// CSV List Class ///
CSV::CSV(std::string filename) : f_(filename, std::ios::out) {}

void CSV::flush() {
  f_.flush();
}

