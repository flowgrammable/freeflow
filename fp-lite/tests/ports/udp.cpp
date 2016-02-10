#include <iostream>

#include "port.hpp"
#include "port_udp.hpp"
#include "port_table.hpp"

using namespace fp;

int
main(int argc, char** argv)
{
	// Make a port table.
	Port_table port_table;

	// Create 2 ports.
	Port* p1 = port_table.alloc(Port::Type::udp, ":5000");
	std::cout << "Created p1 with id '" << p1->id_ << "'\n";

	Port* p2 = port_table.alloc(Port::Type::udp, ":5001");
	std::cout << "Created p2 with id '" << p2->id_ << "'\n";

	// Open ports.
	p1->open();
	std::cout << "Opened p1 on port " << p1->id_ << '\n';
	
	p2->open();
	std::cout << "Opened p2 on port " << p2->id_ << '\n';

	// Display functionality.
	//
	// Up ports.
	p1->up();
	std::cout << "Set p1 to 'up'   : config_.down='" << (p1->config_.down ? "true":"false") << "'\n";
	
	p2->up();
	std::cout << "Set p2 to 'up'   : config_.down='" << (p2->config_.down ? "true":"false") << "'\n";

	
	// TODO: Verify the ports are able to send/recv.	

	
	// Down ports.
	p1->down();
	std::cout << "Set p1 to 'down' : config_.down='" << (p1->config_.down ? "true":"false") << "'\n";
	
	p2->down();
	std::cout << "Set p2 to 'down' : config_.down='" << (p2->config_.down ? "true":"false") << "'\n";	

	// Close ports.
	p1->close();
	std::cout << "Closed p1 on port " << p1->id_ << '\n';
	
	p2->close();
	std::cout << "Closed p2 on port " << p2->id_ << '\n';

	// Remove the ports.
	port_table.dealloc(p1->id_);
	std::cout << "Removed p1\n";

	port_table.dealloc(p2->id_);
	std::cout << "Removed p2\n";
	

	// TODO: Anything else?
	return 0;
}