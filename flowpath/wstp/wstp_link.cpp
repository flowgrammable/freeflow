/* Wolfram Symbolic Transfer Protocol (WSTP)
 *
 * 'Packet' Types (structural bundles of messages to/from the Kernel:
 * https://reference.wolfram.com/language/ref/c/WSNextPacket.html
 *
 * Function Call Interface:
 * CALLPKT	CallPacket[integer,list]	request to invoke the external function numbered integer with arguments list
 * ENTEREXPRPKT	EnterExpressionPacket[expr]	evaluate expr
 * RETURNEXPRPKT	ReturnExpressionPacket[expr]	result of EnterExpressionPacket[] evaluation
 * EVALUATEPKT	EvaluatePacket[expr]	evaluate expr while avoiding the kernel main loop
 * RETURNPKT	ReturnPacket[expr]	result of a calculation
 * ENTERTEXTPKT	EnterTextPacket[string]	parse string and evaluate as an expression
 * RETURNTEXTPKT	ReturnTextPacket[string]	formatted text representation of a result
 *
 * Variable I/O:
 * INPUTNAMEPKT	InputNamePacket[string]	name to be assigned to the next input (usually In[n]:=)
 * INPUTPKT	InputPacket[]	prompt for input, as generated by the Wolfram Language's Input[] function
 * INPUTSTRPKT	InputStringPacket[]	request input as a string
 * OUTPUTNAMEPKT	OutputNamePacket[string]	name to be assigned to the next output (usually Out[n]=)
 * MENUPKT	MenuPacket[integer,string]	menu request with title string
 *
 * Dialog I/O:
 * BEGINDLGPKT	BeginDialogPacket[integer]	start a dialog subsession referenced by integer
 * ENDDLGPKT	EndDialogPacket[integer]	end the dialog subsession referenced by integer
 *
 * Language Error Handling:
 * MESSAGEPKT	MessagePacket[symbol,string]	Wolfram Language message identifier (symbol::string)
 * SYNTAXPKT	SyntaxPacket[integer]	position at which a syntax error was detected in the input line
 * TEXTPKT	TextPacket[string]	text output from the Wolfram Language, as produced by Print[]
 *
 * Depcricated:
 * DISPLAYENDPKT	DisplayEndPacket[]	obsolete PostScript graphics-related packet
 * DISPLAYPKT	DisplayPacket[]	obsolete PostScript graphics-related packet
 * RESUMEPKT	ResumePacket[]	obsolete packet
 * SUSPENDPKT	SuspendPacket[]	obsolete packet
 */

#include "wstp_link.hpp"

#include <iostream>
#include <sstream>
#include <csignal>
#include <tuple>

using pkt_id_t = wstp_link::pkt_id_t;

// Forward declarations of get/put specilizations:
extern "C" {
void wstp_link_err_handler(WSLINK link, int msg, int arg);
void wstp_ENVsig_handler(int signal);
}

template<> int wstp_link::get<int>();
template<> int64_t wstp_link::get<int64_t>();
template<> double wstp_link::get<double>();
template<> std::string wstp_link::get<std::string>();
template<> int wstp_link::put<int64_t>(int64_t);
template<> int wstp_link::put<double>(double);
template<> int wstp_link::put<std::string>(std::string);
template<> int wstp_link::put<const char*>(const char*);
template<> int wstp_link::put_function<std::string>(std::string, int);
template<> int wstp_link::put_function<const char*>(const char*, int);
template<> int wstp_link::put_function<int64_t>(int64_t, int);
template<> int wstp_link::put_symbol<std::string>(std::string);
template<> int wstp_link::put_symbol<const char*>(const char*);


// Global Initialization:
std::vector<wstp_link::def_t> wstp_link::wstp_signatures_;


/////////////////////////////////////////
//// WSTP Enviornment Handle Wrapper ////
wstp_env ENV;  // single global instace.

wstp_env::wstp_env(WSEnvironmentParameter ENVparam) {
  handle_ = WSInitialize(ENVparam);
  if (!handle_) {
    throw std::string("WSInitialize Failed!");
  }

  int status = WSSetSignalHandlerFromFunction(handle_, SIGINT, wstp_ENVsig_handler);
  if (status != WSEOK) {
    std::cerr << "Failed to set signal handler using "
                 "WSSetSignalHandlerFromFunction()" << std::endl;
    // No-throw error condition.
  }
}


wstp_env::~wstp_env() {
  WSDeinitialize(handle_);
}


WSENV wstp_env::borrow_handle() const {
  return handle_;
}


//////////////////////////////////
//// WSTP Link Handle Wrapper ////
wstp_link::wstp_link(std::string args) {
  int err = 0;
  link_ = WSOpenString(ENV.borrow_handle(), args.c_str(), &err);
  if(!link_ || err != WSEOK) {
    std::cerr << "WSOpenString Error: " << err << std::endl;
    throw std::runtime_error{"Failed WSOpenString."};
  }
  if (!(err = WSActivate(link_))) {
    std::cerr << "WSActivate Error: " << err << std::endl;
    throw std::runtime_error{print_error()};
  }
  log();
}


wstp_link::wstp_link(WSLINK link) : link_(link) {
  int err = 0;
  if (!link_ || !(err = WSActivate(link_))) {
    std::cerr << "WSActivate Error: " << err << std::endl;
    throw std::runtime_error{print_error()};
  }
  log();
}


wstp_link::~wstp_link() {
  if (worker_.joinable()) {
    worker_stop_ = true;
    worker_.join();
  }
  if (link_ && WSError(link_) == 0) {
    // library bug: WSClose seems to segfault if the library already closed connection...
    // - e.g. "wstp_link error 1: WSTP connection was lost."
    WSClose(link_);
    link_ = nullptr;
  }
}

// move constructor
wstp_link::wstp_link(wstp_link&& other) :
  link_(std::exchange(other.link_, nullptr)),
  worker_(std::move(other.worker_)),
  worker_stop_(other.worker_stop_)
{
  bind(); // initializes new worker_fTable.
}

// move assignment
wstp_link& wstp_link::operator=(wstp_link&& other) {
  link_ = (std::exchange(other.link_, nullptr));
  std::swap(worker_, other.worker_);
  worker_stop_ = other.worker_stop_;

  bind(); // initializes new worker_fTable.
  return *this;
}


wsint64 wstp_link::close_fn() {
  worker_stop_ = true;
  return 0;
}


wsint64 wstp_link::wakeup_fn() {
  // check for any compute jobs, then return...
  const auto p1 = std::chrono::system_clock::now();
  const int epoc = std::chrono::duration_cast<std::chrono::seconds>(
                    p1.time_since_epoch()).count();
  return factor_test2(epoc);
}


int wstp_link::register_fn(def_t def) {
  static std::once_flag once;
  auto init_fn = [](){
    wstp_signatures_.emplace_back( def_t( sig_t("FFClose[]", ""), nullptr) );
    wstp_signatures_.emplace_back( def_t( sig_t("FFWakeup[]", ""), nullptr) );
  };
  std::call_once(once, init_fn);

  wstp_signatures_.insert(wstp_signatures_.end(), def);
  return wstp_signatures_.size();
}


void wstp_link::bind() {
  using fTable_t = decltype(worker_fTable_);
  fTable_t next;

  for (size_t i = 0; i < wstp_signatures_.size(); i++) {
    switch (i) {
    case 0:
      next.emplace_back( std::bind(&wstp_link::close_fn, this) );
      break;
    case 1:
      next.emplace_back( std::bind(&wstp_link::wakeup_fn, this) );
      break;
    default:
      const fn_t& fn = std::get<fn_t>(wstp_signatures_[i]);
      next.emplace_back(fn);
    }
  }

  std::swap(worker_fTable_, next);
}

// Requires Mathematica to initialize the link as Install[] instead of Connect[]:
// - Must be called following connection setup!
int wstp_link::install() {
  int count = 0;

  for (int i = 0; i < wstp_signatures_.size(); i++) {
    const sig_t& sig = std::get<sig_t>(wstp_signatures_[i]);
    count += put_function("DefineExternal", 3);
    count += put(std::tuple_cat( sig, std::make_tuple(i)) );
  }
  count += put_symbol("End");

  if (error()) {
    print_error();
  }

  bind();

  // Start thread to serve as function handler:
  worker_ = std::thread(&wstp_link::receive_worker, this);
  return count;
}


void wstp_link::receive_worker() {
  std::cerr << "WSTP worker thread started." << std::endl;
  try {
    if (!WSSetMessageHandler(link_, wstp_link_err_handler)) {
      std::cerr << "WARNING: Unable to set wstp error message handler." << std::endl;
      print_error();
    }
    while (!worker_stop_) {
      if (ready()) {
        pkt_id_t id = receive();
      }
      else {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  }
  catch (const std::runtime_error e) {
    std::cerr << "ERROR: Caught expection in wstp interface:\n" << e.what() << std::endl;
  }
  std::cerr << "WSTP worker thread terminated." << std::endl;
  worker_stop_ = true;
}


int wstp_link::ready() const {
  flush();
  int i;
  if (!(i=WSReady(link_))) {
    print_error();
  }
  return i;
}


int wstp_link::flush() const {
  int i;
  if (!(i=WSFlush(link_))) {
    throw std::runtime_error{print_error()};
  }
  return i;
}


void wstp_link::reset() {
  if (!WSClearError(link_)) {
    throw std::runtime_error{print_error()};
  }
  if (!WSNewPacket(link_)) {
    throw std::runtime_error{print_error()};
  }
}


int wstp_link::error() const {
  return WSError(link_);
}


bool wstp_link::alive() const {
  return !worker_stop_;
}


std::string wstp_link::get_error() const {
  if (int code = WSError(link_)) {
    const char* err = WSErrorMessage(link_);
    std::stringstream ss;
    ss << "wstp_link error " << code << ": " << err;
    WSReleaseErrorMessage(link_, err);
    return ss.str();
  } else {
    return std::string();
  }
}


std::string wstp_link::print_error() const {
  if (error()) {
    std::string err = get_error();
    std::cerr << err << std::endl;
    return err;
  }
  return std::string();
}


bool wstp_link::log(std::string filename) {
  if (filename.empty()) {
    const char* cstr = nullptr;
    if (!WSLogFileNameForLink(link_, &cstr)) {
      return false;
    }
    filename = std::string(cstr);
    WSReleaseLogFileNameForLink(link_, cstr);
  }
  if (!WSLogStreamToFile(link_, filename.c_str())) {
    print_error();
    return false;
  }
  return true;
}


int wstp_link::decode_call() {
  const int func = get<int>();
  if (func < 0 || func >= worker_fTable_.size()) {
    std::cerr << "Unrecognized Function ID: " << func << std::endl;
    put_symbol("$Failed");  // TODO: verify appropriate error...
    return 0;
  }
  const size_t funcID = static_cast<size_t>(func);

  // Execute appropriate function:
  std::cerr << "Evaluate: "
            << std::get<0>( std::get<sig_t>(wstp_signatures_[funcID]) )
            << std::endl;
  return_t v = worker_fTable_[funcID](0);

  // Send result back:
  put_function("ReturnPacket", 1);
  if (std::holds_alternative<ts_t>(v)) {
    const ts_t& events = std::get<ts_t>(v);
    std::cerr << "Returning list of size: " << events.size()-1 << std::endl;
    WSPutInteger64List(link_, events.data()+1, static_cast<int>(events.size()-1));
  }
  else if (std::holds_alternative<wsint64>(v)) {
    auto x = std::get<wsint64>(v);
    std::cerr << "Returning wsint64: " << x << std::endl;
    WSPutInteger64(link_, x);
  }
  else {
    std::cerr << "Invalid return variant!" << std::endl;
    put_symbol("$Failed");  // TODO: verify appropriate error...
  }
  put_end();
  flush();

  return func;
}


pkt_id_t wstp_link::receive() {
  // Skip any unconsumed data if needed:
begin:
  if (!WSNewPacket(link_)) {
    print_error();
    reset();
  }

  // Get ID of next packet:
  pkt_id_t pkt_id = WSNextPacket(link_);
  switch (pkt_id) {
  case CALLPKT:
//    std::cerr << "Call WSTP Packet type." << std::endl;
    decode_call();
    break;
  case EVALUATEPKT:
    std::cerr << "Unhandled Evaluate WSTP Packet type." << std::endl;
    break;
  case RETURNPKT:
    std::cerr << "Unhandled Return WSTP Packet type." << std::endl;
    break;
  case ILLEGALPKT:
    if (error() == 11) {
      std::cerr << "WSTP EOF Packet (Connection closed).\n" << std::endl;
      throw std::runtime_error{"WSTP Connection Closed."};
    }
    else {
      std::cerr << "Unknown/Illegal WSTP Packet type." << std::endl;
    }
    print_error();
    reset();
    return pkt_id;
  default:
    std::cerr << "Unsupported WSTP Packet type; ignoring..." << std::endl;
    goto begin;
  }

  if (error()) {
    print_error();
  }
  return pkt_id;  // TODO: repeat until packet is fully received?
}


pkt_id_t wstp_link::receive(const pkt_id_t id) {
  pkt_id_t rid;
  while((rid = receive()) != id) {
    std::cerr << "Skipping Packet with ID " << rid
              << ", waiting for " << id << std::endl;
  }
  return rid;
}


// Example Test Functions:
int wstp_link::factor_test(int n) {
  std::cout << "Factors of " << n << ":\n";

  // Send actual Mathematica command:
  WSPutFunction(link_, "EvaluatePacket", 1L);
  WSPutFunction(link_, "FactorInteger", 1L);
  WSPutInteger(link_, n);
  WSEndPacket(link_);

  int pkt = 0;
  // Skip everything until Return packet type is found:
  while( (pkt = WSNextPacket(link_)) != RETURNPKT) {
    WSNewPacket(link_);
    if (error()) {
      throw std::runtime_error{print_error()};
    }
  }

  // Expect list returned; len holds list length:
  int len = 0;
  if ( !WSTestHead(link_, "List", &len) ) {
    std::cerr << "WSTestHead is NULL" << std::endl;
    if (error()) {
      throw std::runtime_error{print_error()};
    }
  }

  // For each element in list,
  for (int k = 1; k <= len; k++) {
    int lenp = 0, prime = 0, expt = 0;
    if (WSTestHead(link_, "List", &lenp)
        &&  lenp == 2
        &&  WSGetInteger(link_, &prime)
        &&  WSGetInteger(link_, &expt) )
    {
      std::cout << prime << " ^ " << expt << std::endl;
    } else {
      std::cerr << "Failed to decode WSTestHead" << std::endl;
      throw std::runtime_error{print_error()};
    }
  }

//  WSPutFunction(link_, "Exit", 0);
  return EXIT_SUCCESS;
}

int wstp_link::factor_test2(int n) {
  std::cout << "Factors of " << n << ":\n";

  // Send Mathematica command to Factor integer:
  auto factorCMD = std::make_tuple("FactorInteger", int64_t(n));
  put_function(factorCMD);

  using prime_t = int64_t;
  using expt_t = int64_t;
  using factor_t = std::tuple<prime_t, expt_t>;

  auto factors = receive_list<factor_t>();
  for (factor_t f : factors) {
    std::cout << std::get<0>(f) << " ^ " << std::get<1>(f) << std::endl;
  }
  return factors.size();
}


/// Specilizations to get items from link ///
// Helpers:
void replace_all(std::string& str, std::string match, std::string replace) {
  auto pos = str.find(match);
  while (pos != str.npos) {
    str.replace(pos, match.size(), replace);
    pos = str.find(match, pos+replace.size());
  }
}


// Integer elements:
template<>
int64_t wstp_link::get<int64_t>() {
  static_assert(sizeof(wsint64) == sizeof(int64_t), "wsint64 is not 64-bits!");
//  std::cerr << "DEBUG: Called get<int64_t>()";
  wsint64 i;
  if (!WSGetInteger64(link_, &i)) {
    std::cerr << std::endl;
    print_error();
    reset();
  }
//  std::cerr << " = " << i << std::endl;
  return i;
}


template<>
int wstp_link::get<int>() {
//  std::cerr << "DEBUG: Called get<int>()";
  int i;
  if (!WSGetInteger(link_, &i)) {
    std::cerr << std::endl;
    print_error();
    reset();
  }
//  std::cerr << " = " << i << std::endl;
  return i;
}


// Real elements:
template<>
double wstp_link::get<double>() {
//  std::cerr << "DEBUG: Called get<double>()";
  double r;
  if (!WSGetReal64(link_, &r)) {
    std::cerr << std::endl;
    print_error();
    reset();
  }
//  std::cerr << " = " << r << std::endl;
  return r;
}


// String elements:
template<>
std::string wstp_link::get<std::string>() {
  std::cerr << "DEBUG: Called get<std::string>()";
  const char* cstr;
  if (!WSGetString(link_, &cstr)) {
    std::cerr << std::endl;
    print_error();
    reset();
  }
  std::string s(cstr);
  WSReleaseString(link_, cstr);

  replace_all(s, "\\\\", "\\");  // Mathematica requires all '\' to be escaped.
  std::cerr << " = " << s << std::endl;
  return s;
}


/// Push item to link specilizations ///
// Integer elements:
template<>
int wstp_link::put<int64_t>(int64_t i) {
  static_assert(sizeof(wsint64) == sizeof(int64_t), "wsint64 is not 64-bits!");
//  std::cerr << "DEBUG: Called put<int64_t>(" << i << ")" << std::endl;
  if (!WSPutInteger64(link_, i)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}

//WSPutInteger
template<>
int wstp_link::put<int>(int i) {
//  std::cerr << "DEBUG: Called put<int>(" << i << ")" << std::endl;
  if (!WSPutInteger(link_, i)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}
//template<>
//int wstp_link::put<std::vector<int64_t>>(std::vector<int64_t> list) const {
//  static_assert(sizeof(wsint64) == sizeof(int64_t), "wsint64 is not 64-bits!");
//  std::cerr << "DEBUG: Called put<std::vector<int64_t>>(length(" << list.size() << "))" << std::endl;
//  if (!WSPutInteger64(link_, i)) {
//    print_error();
//    reset();
//    return 0;
//  }
//  return 1;
//}


// Real elements:
template<>
int wstp_link::put<double>(double r) {
//  std::cerr << "DEBUG: Called put<double>(" << r << ")" << std::endl;
  if (!WSPutReal64(link_, r)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}


// String elements:
template<>
int wstp_link::put<std::string>(std::string s) {
//  std::cerr << "DEBUG: Called put<string>(" << s << ")" << std::endl;
  replace_all(s, "\\", "\\\\");  // Mathematica requires all '\' to be escaped.
  if (!WSPutString(link_, s.c_str())) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}
template<>
int wstp_link::put<const char*>(const char* cstr) {
  return put(std::string(cstr));
}


// Function:
template<>
int wstp_link::put_function<std::string>(std::string s, int args) {
//  std::cerr << "DEBUG: Called put_function<string>(" << s << ")" << std::endl;
  if (!WSPutFunction(link_, s.c_str(), args)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}
template<>
int wstp_link::put_function<const char*>(const char* cstr, int args) {
  return put_function(std::string(cstr), args);
}


// Symbol:
template<>
int wstp_link::put_symbol<std::string>(std::string s) {
//  std::cerr << "DEBUG: Called put_symbol<string>(" << s << ")" << std::endl;
  if (!WSPutSymbol(link_, s.c_str())) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}
template<>
int wstp_link::put_symbol<const char*>(const char* cstr) {
  return put_symbol(std::string(cstr));
}


// TODO: this seems like a hack and should be factored out?
template<>
int wstp_link::put_function<int64_t>(int64_t i, int) {
  return put(i);
}


int wstp_link::put_end() {
//  std::cerr << "DEBUG: Called put_end()" << std::endl;
  if (!WSEndPacket(link_)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}


//////////////////////////////////////
//// WSTP C ABI Callback Handlers ////
extern "C" {
// Define handler with C ABI for wstp_link error callback:
void wstp_link_err_handler(WSLINK link, int msg, int arg) {
  switch (msg) {
  case WSInterruptMessage:
    std::cerr << "Unhandled WSInterruptMessage callback message!" << std::endl;
    break;
  case WSAbortMessage:
    std::cerr << "Unhandled WSAbortMessage callback message!" << std::endl;
    break;
  case WSTerminateMessage:
    std::cerr << "Unhandled WSTerminateMessage callback message!" << std::endl;
    break;
  default:
    std::cerr << "WARNING: Unexpected callback message type: " << msg << std::endl;
  }
}

// Define handler with C ABI for wstp_env signal callback:
void wstp_ENVsig_handler(int signal) {
  std::cerr << "Signal " << signal << " received;";
  switch (signal) {
  case SIGINT:
    std::cerr << " exiting." << std::endl;
    std::exit(EXIT_SUCCESS);
  default:
    std::cerr << " no action." << std::endl;
  }
}
}
