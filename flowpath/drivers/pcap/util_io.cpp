#include "util_io.hpp"

#include <ostream>
#include <fstream>
#include <memory>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>

using namespace std;


gz_ostream::gz_ostream(const std::string& filename) {
  os_file.open(filename, ios::out | ios::binary);
  gzip_buf = make_unique< boost::iostreams::filtering_streambuf<boost::iostreams::output> >();
  gzip_buf->push(boost::iostreams::gzip_compressor());
  gzip_buf->push(os_file);
  os = make_unique<ostream>(gzip_buf.get());
}

gz_ostream::~gz_ostream() {
  os->flush();
  os_file.close();
}

ostream& gz_ostream::get_ostream() const {
  return *os;
}
