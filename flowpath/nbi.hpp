#ifndef FP_NBI_HPP
#define FP_NBI_HPP

#include "context.hpp"

namespace fp
{

// ??? Not sure how we're handling these
// TODO: figure out if we need explicit messages or if function calls
// are satisfactory
struct Message {};

// notify the controller
void on_error(Message*);

// notify the controller on changes to a table's capabilities
void on_table_change(Message*);

// notify the controller on changes to flow entries
void on_flow_change(Message*);
void on_flow_removal(Message*);
void on_flow_timeout(Message*);

// notify the controller that port status has changed
void on_port_change(Port*);

// used to transfer control of a packet to the controller
void on_packetin_message(Packet*);

} // end namespace fp




#endif