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

#ifdef __APPLE__
constexpr char ARGS[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
#else
constexpr char ARGS[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
#endif

using pkt_id_t = wstp_link::pkt_id_t;

// Forward declarations of get/put specilizations:
template<> int64_t wstp_link::get<int64_t>() const;
template<> double wstp_link::get<double>() const;
template<> std::string wstp_link::get<std::string>() const;
template<> int wstp_link::put<int64_t>(int64_t) const;
template<> int wstp_link::put<double>(double) const;
template<> int wstp_link::put<std::string>(std::string) const;

wstp_link::wstp_link() : env_(nullptr), link_(nullptr) {
  env_ = WSInitialize(nullptr);
  if (!env_)
    throw std::string("Failed WSInitialize.");

  int err = 0;
  link_ = WSOpenString(env_, ARGS, &err);
  if(!link_ || err != WSEOK) {
    std::cerr << "WSOpenArgcArgv Error: " << err << std::endl;
    throw std::string("Failed WSOpenArgv.");
  }
}

wstp_link::wstp_link(int argc, char* argv[]) : env_(nullptr), link_(nullptr) {
  env_ = WSInitialize(nullptr);
  if (!env_)
    throw std::string("Failed WSInitialize.");

  int err = 0;
  link_ = WSOpenArgcArgv(env_, argc, argv, &err);
  if(!link_ || err != WSEOK) {
    std::cerr << "WSOpenArgcArgv Error: " << err << std::endl;
    throw std::string("Failed WSOpenArgv.");
  }
}

wstp_link::~wstp_link() {
  deinit();
  closelink();
}

void wstp_link::closelink(void) {
  if (link_)
    WSClose(link_);
  link_ = nullptr;
}

void wstp_link::deinit(void) {
  if (env_)
    WSDeinitialize(env_);
  env_ = nullptr;
}

int wstp_link::reset() const {
  if (!WSClearError(link_)) {
    throw print_error();
  }
  if (!WSNewPacket(link_)) {
    throw print_error();
  }
  return 0;
}

bool wstp_link::error() const {
  return WSError(link_);
}

std::string wstp_link::get_error() const {
  if ( int code = WSError(link_) ) {
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

pkt_id_t wstp_link::receive() const {
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
    std::cerr << "Call WSTP Packet type." << std::endl;
    break;
  case EVALUATEPKT:
    std::cerr << "Evaluate WSTP Packet type." << std::endl;
    break;
  case RETURNPKT:
    std::cerr << "Return WSTP Packet type." << std::endl;
    break;
  case ILLEGALPKT:
    print_error();
    reset();
    goto begin; // break
  default:
    std::cerr << "Unsupported WSTP Packet type; ignoring..." << std::endl;
    goto begin;
  }
  return pkt_id;  // TODO: repeat until packet is fully received?
}

pkt_id_t wstp_link::receive(const pkt_id_t id) const {
  pkt_id_t rid;
  while((rid = receive()) != id) {
    std::cerr << "Skipping Packet with ID " << rid
              << ", waiting for " << id << std::endl;
  }
  return rid;
}

// Example Test Functions:
int wstp_link::factor_test(int n) const {
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
      throw print_error();
    }
  }

  // Expect list returned; len holds list length:
  int len = 0;
  if ( !WSTestHead(link_, "List", &len) ) {
    std::cerr << "WSTestHead is NULL" << std::endl;
    if (error()) {
      throw print_error();
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
      throw print_error();
    }
  }

//  WSPutFunction(link_, "Exit", 0);
  return EXIT_SUCCESS;
}

int wstp_link::factor_test2(int n) const {
  std::cout << "Factors of " << n << ":\n";

  // Send Mathematica command to Factor integer:
  auto factorCMD = std::make_tuple("FactorInteger", int64_t(n));
  put_function(factorCMD);
//  WSEndPacket(link_);

  using prime_t = int64_t;
  using expt_t = int64_t;
  using factor_t = std::tuple<prime_t, expt_t>;

  auto factors = receive_list<factor_t>();
  for (factor_t f : factors) {
    std::cout << std::get<0>(f) << " ^ " << std::get<1>(f) << std::endl;
  }
  return EXIT_SUCCESS;
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
int64_t wstp_link::get<int64_t>() const {
  static_assert(sizeof(wsint64) == sizeof(int64_t), "wsint64 is not 64-bits!");
  std::cerr << "DEBUG: Called get<int64_t>()" << std::endl;
  wsint64 i;
  if (!WSGetInteger64(link_, &i)) {
    print_error();
    reset();
  }
  return i;
}

// Real elements:
template<>
double wstp_link::get<double>() const {
  std::cerr << "DEBUG: Called get<double>()" << std::endl;
  double r;
  if (!WSGetReal64(link_, &r)) {
    print_error();
    reset();
  }
  return r;
}

// String elements:
template<>
std::string wstp_link::get<std::string>() const {
  std::cerr << "DEBUG: Called get<std::string>()" << std::endl;
  const char* cstr;
  if (!WSGetString(link_, &cstr)) {
    print_error();
    reset();
  }
  std::string s(cstr);
  WSReleaseString(link_, cstr);

  replace_all(s, "\\\\", "\\");  // Mathematica requires all '\' to be escaped.
  return s;
}


/// Push item to link specilizations ///
// Integer elements:
template<>
int wstp_link::put<int64_t>(int64_t i) const {
  static_assert(sizeof(wsint64) == sizeof(int64_t), "wsint64 is not 64-bits!");
  std::cerr << "DEBUG: Called put<int64_t>()" << std::endl;
  if (!WSPutInteger64(link_, i)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}

// Real elements:
template<>
int wstp_link::put<double>(double r) const {
  std::cerr << "DEBUG: Called put<double>()" << std::endl;
  if (!WSPutReal64(link_, r)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}

// String elements:
template<>
int wstp_link::put<std::string>(std::string s) const {
  std::cerr << "DEBUG: Called put<string>()" << std::endl;
  replace_all(s, "\\", "\\\\");  // Mathematica requires all '\' to be escaped.
  if (!WSPutString(link_, s.c_str())) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}

// Function:
template<>
int wstp_link::put_function<std::string>(std::string s, int args) const {
  std::cerr << "DEBUG: Called put_function<string>()" << std::endl;
  if (!WSPutFunction(link_, s.c_str(), args)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}

template<>
int wstp_link::put_function<const char*>(const char* cstr, int args) const {
  return put_function(std::string(cstr), args);
}

template<>
int wstp_link::put_function<int64_t>(int64_t i, int) const {
  return put(i);
}

int wstp_link::put_end() const {
  std::cerr << "DEBUG: Called put_function<string>()" << std::endl;
  if (!WSEndPacket(link_)) {
    print_error();
    reset();
    return 0;
  }
  return 1;
}
