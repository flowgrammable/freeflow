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

#include "wstp_server.hpp"
#include "wstp_link.hpp"

#include <iostream>
#include <sstream>
#include <csignal>


// WSTP Server new connection callblack hook:
std::vector<wstp_link>::iterator wstp_take_connection(WSLINK wslink);
extern "C" {
void wstp_server_connection(WSLinkServer server, WSLINK wslink);
}

extern wstp_env ENV;


////////////////////////////////////////
//// WSTP LinkServer Handle Wrapper ////
wstp_server::wstp_server(uint16_t port, std::string ip) :
  server_(nullptr) {
  int err = 0;
  if (!ip.empty()) {
    server_ = WSNewLinkServerWithPortAndInterface(ENV.borrow_handle(), port, ip.c_str(), nullptr, &err);
  }
  if (port != 0) {
    server_ = WSNewLinkServerWithPort(ENV.borrow_handle(), port, nullptr, &err);
  }
  else {
    server_ = WSNewLinkServer(ENV.borrow_handle(), nullptr, &err);
  }
  if (err != WSEOK) {
    std::string server(ip + ":" + std::to_string(port));
    std::cerr << "Failed to start wstp_server(" << server << ")" << std::endl;
    throw std::string("Failed wstp_server(" + server + ")");
  }
  WSRegisterCallbackFunctionWithLinkServer(server_, wstp_server_connection);

  const char* str = WSInterfaceFromLinkServer(server_, &err);
  ip = str;
  WSReleaseInterfaceFromLinkServer(server_, str);
  port = WSPortFromLinkServer(server_, &err);
  std::cout << "Launched WSTP server on " << ip <<":" << port << std::endl;
}


wstp_server::~wstp_server() {
  if (server_) {
    WSShutdownLinkServer(server_);
    server_ = nullptr;
  }
}


// move constructor
wstp_server::wstp_server(wstp_server&& other) :
  server_(std::exchange(other.server_, nullptr))
{}


// move assignment
wstp_server& wstp_server::operator=(wstp_server&& other) {
  server_ = (std::exchange(other.server_, nullptr));
  return *this;
}


auto wstp_server::borrow_handle() const {
  return server_;
}


std::string wstp_server::get_interface() const {
  int err;
  const char* str = WSInterfaceFromLinkServer(server_, &err);
  uint16_t port = WSPortFromLinkServer(server_, &err);
  std::string interface = std::string(str) + ":" + std::to_string(port);
  WSReleaseInterfaceFromLinkServer(server_, str);
  return interface;
}


//////////////////////////////////////
//// WSTP C ABI Callback Handlers ////
extern "C" {
// Define handler with C ABI for wstp_server connection callback:
void wstp_server_connection(WSLinkServer server, WSLINK wslink) {
  std::cerr << "New Connection to WSLinkServer received" << std::endl;
  try {
    auto link_it = wstp_take_connection(wslink);
    link_it->install();
  }
  catch (const std::runtime_error& e) {
    std::cerr << "Caught exception when initializing WSTP connection: "
              << e.what() << std::endl;
  }
}
}
