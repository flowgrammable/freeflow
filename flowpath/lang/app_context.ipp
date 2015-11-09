
// extract the given field from the header
template<int H, int F, int E>
  std::pair<fp::Byte*, int>
  Application_context<H, F, E>::read_field(uint32_t field)
  {
    fp::Binding_list bl = fld_.bindings(field);
    if (bl.is_empty())
      return {nullptr, 0};

    fp::Binding const* b = bl.get_top();

    // TODO: access packet buffer with binding information and return
    return {nullptr, 0};
  }


template<int H, int F, int E>
  std::pair<fp::Byte*, int>
  Application_context<H, F, E>::read_header(uint32_t header)
  {
    fp::Binding_list bl = hdr_.bindings(header);
    if (bl.is_empty())
      return {nullptr, 0};

    fp::Binding const* b = bl.get_top();

    // TODO: access packet buffer with binding information and return
    return {nullptr, 0};
  }
  

template<int H, int F, int E>
  void
  Application_context<H, F, E>::add_field_binding(uint32_t field, uint16_t offset, uint16_t len)
  {
    fp::Binding_list bl = fld_.bindings(field);
    if (bl.is_full())
      // TODO: error print
      // drop packet?
      return;

    bl.insert(offset, len);
  }


template<int H, int F, int E>
  void
  Application_context<H, F, E>::pop_field_binding(uint32_t field)
  {
    fp::Binding_list bl = fld_.bindings(field);
    if (bl.is_empty())
      return; // error?

    bl.pop();
  }


template<int H, int F, int E>
  void
  Application_context<H, F, E>::add_header_binding(uint32_t header, uint16_t offset, uint16_t len)
  {
    fp::Binding_list bl = hdr_.bindings(header);
    if (bl.is_full())
      // TODO: error print
      // drop packet?
      return;

    bl.insert(offset, len);
  }


template<int H, int F, int E>
  void
  Application_context<H, F, E>::pop_header_binding(uint32_t header)
  {
    fp::Binding_list bl = hdr_.bindings(header);
    if (bl.is_empty())
      return; // error?

    bl.pop();
  }