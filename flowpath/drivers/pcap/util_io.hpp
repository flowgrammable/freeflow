#ifndef FF_UTIL_IO_HPP
#define FF_UTIL_IO_HPP

#include <fstream>
#include <sstream>
#include <variant>

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


/// Convert array into a tuple ///
template<typename Array, std::size_t... I>
auto a2t_impl(const Array& a, std::index_sequence<I...>)
{
    return std::make_tuple(a[I]...);
}

template<typename T, std::size_t N, typename Indices = std::make_index_sequence<N>>
auto a2t(const std::array<T, N>& a)
{
    return a2t_impl(a, Indices{});
}


/// Tuple CSV Printer ///
template<class Tuple, std::size_t N>
struct TuplePrinter {
    template<class Target>
    static void print(Target& out, const Tuple& t)
    {
        TuplePrinter<Tuple, N-1>::print(out, t);
        out << "," << std::get<N-1>(t);
    }
};

template<class Tuple>
struct TuplePrinter<Tuple, 1> {
    template<class Target>
    static void print(Target& out, const Tuple& t)
    {
        out << std::get<0>(t);
    }
};

template<class Target, typename... Args, std::enable_if_t<sizeof...(Args) == 0, int> = 0>
void print_tuple(Target& out, const std::tuple<Args...>& t)
{
    out << '\n';
}

template<class Target, typename... Args, std::enable_if_t<sizeof...(Args) != 0, int> = 0>
void print_tuple(Target& out, const std::tuple<Args...>& t)
{
    TuplePrinter<decltype(t), sizeof...(Args)>::print(out, t);
    out << '\n';
}


/// Templated CSV List Class ///
class CSV {
public:
  CSV() = default;
  CSV(std::string filename);

  void flush();

  template<typename Tuple>
  size_t append(Tuple t) {
    print_tuple(f_, t);
    return ++lines_;
  }

private:
  std::ofstream f_;
  size_t lines_ = 0;
};


/// Templated Rolling Buffer ///
template<typename T, size_t Entries>
class RollingBuffer {
public:
  RollingBuffer() = default;

  T insert(T a) {
    if (head_ > Entries) {
      std::swap(a, ring_[head_++ % Entries]);
      return a;
    }
    else {
      ring_[head_++] = a;
      return T{};
    }
  }

  size_t total() const {
    return head_;
  }

private:
  size_t head_ = 0;   // Index 1 past last inserted entry (if n_ > 0); must mod by Entries
  std::array<T, Entries> ring_;
};


#endif // FF_UTIL_IO_HPP
