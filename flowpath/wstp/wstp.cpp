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

using pkt_id_t = wstp_link::pkt_id_t;


//////////////////////////////
//// WSTP Top-level Class ////
bool wstp::unlink_all_ = false;
std::mutex wstp::mtx_;
std::vector<std::unique_ptr<wstp_link>> wstp::links_;
std::vector<std::unique_ptr<wstp_server>> wstp::servers_;

// Move is broken...
//void wstp::take_connection(wstp_link&& link) {
//  std::lock_guard lck(mtx_);
//  links_.emplace_back(std::move(link));
//}

std::vector<std::unique_ptr<wstp_link>>::iterator
wstp::take_connection(WSLINK wslink) {
  std::lock_guard lck(mtx_);
  auto link = std::make_unique<wstp_link>(wslink);
  return links_.emplace(links_.end(), std::move(link));
}

// Used by main() thread to wait until all WSTP connections close before exit.
void wstp::wait_for_unlink() {
  while(!unlink_all_) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}

std::vector<std::unique_ptr<wstp_link>>::iterator
wstp_take_connection(WSLINK wslink) {
  return wstp::take_connection(wslink);
}
