#ifndef UTIL_CONTAINER_HPP
#define UTIL_CONTAINER_HPP


template<typename Iterator>
class KeyIterator : public Iterator {
  using Pair = typename Iterator::value_type;
  using Key = typename Pair::first_type;
  using Value = typename Pair::second_type;

public:
    KeyIterator() : Iterator() {};
    KeyIterator(Iterator it) : Iterator(it) {};

    Key* operator->() {
      Pair& item = Iterator::operator*();
      return &(item.first);
    }
    Key& operator*() {
      Pair& item = Iterator::operator*();
      return item.first;
    }
    Value& getValue() {
      Pair& item = Iterator::operator*();
      return item.second;
    }
};


template<typename Iterator>
class ValueIterator : public Iterator {
  using Pair = typename Iterator::value_type;
  using Key = typename Pair::first_type;
  using Value = typename Pair::second_type;

public:
    ValueIterator() : Iterator() {};
    ValueIterator(Iterator it) : Iterator(it) {};

    Value* operator->() {
      Pair& item = Iterator::operator*();
      return &(item.second);
    }
    Value& operator*() {
      Pair& item = Iterator::operator*();
      return item.second;
    }
    Key& getKey() {
      Pair& item = Iterator::operator*();
      return item.first;
    }
};

#endif // UTIL_CONTAINER_HPP
