#include "util_caida.hpp"

#include <string>
#include <iostream>
#include <map>
#include <queue>

using namespace std;


bool timespec_less(const timespec& lhs, const timespec& rhs) {
  return (lhs.tv_sec < rhs.tv_sec) &&
         (lhs.tv_nsec < rhs.tv_nsec);
}

bool timespec_greater(const timespec& lhs, const timespec& rhs) {
  return (lhs.tv_sec > rhs.tv_sec) &&
         (lhs.tv_nsec > rhs.tv_nsec);
}

bool timespec_equal(const timespec& lhs, const timespec& rhs) {
  return (lhs.tv_sec == rhs.tv_sec) &&
         (lhs.tv_nsec == rhs.tv_nsec);
}


int caidaHandler::advance(int id) {
  fp::Context cxt;
  if (pcap_.at(id).recv(&cxt) > 0) {
    next_cxt_.push( std::move(cxt) );
    pkt_pos_.at(id)++;
    return 1; // 1 packet read
  }
  else {
    if (pcap_files_.at(id).size() > 0) {
      // Queue next pcap file and prepare for first read:
      auto& list = pcap_files_.at(id);
      string file = list.front();
      list.pop();

      cerr << "Opening PCAP file: " << file
           << "\n - Transition at packet " << pkt_pos_.at(id) << endl;
      pcap_.erase(id);
      pcap_.emplace(std::piecewise_construct,
                    std::forward_as_tuple(id),
                    std::forward_as_tuple(id, file.c_str()) );

      return advance(id); // recursive call to handle 1st read
    }
  }
  cerr << "No packets to read from Port: " << id << endl;
  return 0; // no packets to read
}

void caidaHandler::open_list(int id, string listFile) {
  // Open text list file:
  ifstream lf;
  lf.open(listFile);
  if (!lf.is_open()) {
    cerr << "Failed to open_list(" << id << ", " << listFile << endl;
    return;
  }

  // Find directory component of listFile:
  size_t path = listFile.find_last_of('/');

  // Queue all files up in list (in read-in order)
  auto& fileQueue = pcap_files_[id];
  string fileName;
  while (getline(lf, fileName)) {
    fileName.insert(0, listFile, 0, path+1);
//    cerr << fileName << '\n';
    fileQueue.emplace( std::move(fileName) );
  }
  cerr.flush();
  lf.close();

  // Attempt to open first file in stream:
  fileName = std::move(fileQueue.front());
  fileQueue.pop();
  open(id, fileName);
}

void caidaHandler::open(int id, string file) {
  // Automatically call open_list if file ends in ".files":
  const string ext(".files");
  if (file.compare(file.length()-ext.length(), string::npos, ext) == 0) {
    return open_list(id, file);
  }

  // Open PCAP file:
  pcap_.emplace(std::piecewise_construct,
                std::forward_as_tuple(id),
                std::forward_as_tuple(id, file.c_str()) );
  pcap_files_[id];  // ensure id exists
  pkt_pos_[id] = 0;

  // Attempt to queue first packet in stream:
  advance(id);
}

fp::Context caidaHandler::recv() {
  if (next_cxt_.empty()) {
    for (const auto& l : pcap_files_) {
      // Sanity check to ensure no dangling unread pcap files...
      assert(l.second.size() == 0);
    }
    return fp::Context();
  }

  // Swap priority_queue container objects without a built-in move optimization:
  fp::Context top = std::move( const_cast<fp::Context&>(next_cxt_.top()) );
  next_cxt_.pop();
  int id = top.in_port();

  // Queue next packet in stream:
  advance(id);
  return top;
}

int caidaHandler::rebound(fp::Context* cxt) {
  return pcap_.at(cxt->in_port()).rebound(cxt);
}
