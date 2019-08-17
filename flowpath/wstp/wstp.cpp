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

#include "wstp.hpp"

#include <iostream>
#include <sstream>
#include <csignal>


/////////////////
//// GLOBALS ////
static std::unique_ptr<wstp_singleton> wstp_global;
extern std::shared_ptr<wstp_env> ENV;

//////////////////////////////
//// WSTP Top-level Class ////
wstp_singleton* wstp_singleton::singleton_ = nullptr;

wstp_singleton::wstp_singleton() {
  if (ENV) {
    env_ = ENV;
  }
  else {
    env_ = std::make_shared<wstp_env>();
  }

  if (singleton_ == nullptr) {
    singleton_ = this;
  }
}

wstp_singleton::~wstp_singleton() {
  if (singleton_ == this) {
    singleton_ = nullptr;
  }
}

std::vector<wstp_link>::iterator
wstp_singleton::take_connection(wstp_link&& link) {
  std::lock_guard lck(mtx_);
  return links_.insert(links_.end(), std::forward<wstp_link>(link));
}


std::vector<wstp_link>::iterator
wstp_singleton::take_connection(WSLINK wslink) {
  std::lock_guard lck(mtx_);
  return links_.emplace(links_.end(), wslink);
}


// Used by main() thread to wait until all WSTP connections close before exit.
void wstp_singleton::wait_for_unlink() {
  while(true) {
    bool alive = false;
    for (const auto& link : links_) {
      alive |= link.alive();
    }
    if (!alive) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}


wstp_singleton& wstp_singleton::getInstance() {
  if (singleton_) {
    return *singleton_;
  }
  wstp_global = std::make_unique<wstp_singleton>();
  return wstp_singleton::getInstance();
}


std::vector<wstp_link>::iterator
wstp_take_connection(WSLINK wslink) {
  wstp_singleton& w = wstp_singleton::getInstance();
  return w.take_connection(wslink);
}
