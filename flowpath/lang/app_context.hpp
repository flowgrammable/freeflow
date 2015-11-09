#ifndef FP_APP_CONTEXT_HPP
#define FP_APP_CONTEXT_HPP

#include "context.hpp"


// Keep track of the header and the what is currently
// being decoded in each stage
template<int MAX_EXTRACTS>
struct Header_state
{
  Header_state()
    : num_extracts(0)
  { }

  // We can keep track of the current header binding
  fp::Binding* current_header;
  // We can also keep track of the extracts that occur
  // in the current decoding stage.
  fp::Binding* extracts[MAX_EXTRACTS];
  // Keep track of the number of extractions in that phase
  uint8_t num_extracts;

  Header_state* next_header;
  Header_state* prev_header;
};


// The application context also contains environment
// in addition to the other context fields.
// Each application has to maintain a different sized
// Application_context because each application has a different
// number of fields/headers it could potentially decode
template<int MAX_HEADERS, int MAX_FIELDS, int MAX_EXTRACTS>
struct Application_context : fp::Context
{
  Application_context(Context const& c)
    : Context(c)
  { }

  virtual ~Application_context() { }

  fp::Environment<MAX_HEADERS> headers() const { return hdr_; }
  fp::Environment<MAX_FIELDS> fields() const { return fld_; }

  std::pair<fp::Byte*, int>  read_field(uint32_t);
  std::pair<fp::Byte*, int>  read_header(uint32_t);

  void      add_field_binding(uint32_t, uint16_t, uint16_t);
  void      pop_field_binding(uint32_t);
  void      add_header_binding(uint32_t, uint16_t, uint16_t);
  void      pop_header_binding(uint32_t);

  // We need to maintain the ordering of headers
  // as the environment is incapable of doing so.
  Header_state<MAX_EXTRACTS> header_state[MAX_HEADERS];
  // pointer to the immediate state
  Header_state<MAX_EXTRACTS>* state_; 

  fp::Environment<MAX_HEADERS> hdr_;
  fp::Environment<MAX_FIELDS> fld_;  
};

#include "app_context.ipp"


#endif