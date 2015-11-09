
template<typename T>
int size_all(T t)
{
  return sizeof(t);
}


template<typename T, typename... Args>
int size_all(T t, Args... args)
{
  return sizeof(t) + size_all(args...);
}


template<typename T>
void synthesize_subkey(fp::Byte* key, int& begin, T subk)
{
  // convert T to a fp::Byte* and copy it into key with the range [begin, begin+sizeof(subkey))
  fp::Byte* subkey = reinterpret_cast<fp::Byte*>(&subk);
  std::copy(subkey, subkey + sizeof(subk), &key[begin]);
  
  begin += sizeof(subk);
}


template<typename T, typename... Args>
void synthesize_subkey(fp::Byte* key, int& begin, T subk, Args... args)
{
  // convert T to a fp::Byte* and copy it into key with the range [begin, begin+sizeof(subkey))
  fp::Byte* subkey = reinterpret_cast<fp::Byte*>(&subk);
  std::copy(subkey, subkey + sizeof(subk), &key[begin]);
  begin += sizeof(subk);

  // recurse
  synthesize_subkey(key, begin, args...);
}


template<typename... Args>
void synthesize_key(fp::Byte* key, Args... args)
{
  int size = size_all(args...);

  // FIXME: throw something more meaningful
  if (size > 128)
    throw 1;

  assert(size <= 128);

  int begin = 0; 

  synthesize_subkey(key, begin, args...);
}


template<typename... Args>
Key make_key(Args... args)
{
  int size = size_all(args...);

  fp::Byte key[128];

  synthesize_key(key, args...);

  return Key(key, size);
}