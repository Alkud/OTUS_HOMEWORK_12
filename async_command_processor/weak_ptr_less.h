// weak_ptr_less.h in Otus homework#12 project

#pragma once

#include <memory>

template <class T>
struct WeakPtrLess
{
  bool operator()(const std::weak_ptr<T>& a,
                   const std::weak_ptr<T>& b)
  {
    if (a.expired() == true)
    {
      return false;
    }
    if (b.expired() == true)
    {
      return true;
    }
    return a.lock() < b.lock();
  }
};

template <class T>
bool operator==(const std::weak_ptr<T>& a,
                   const std::weak_ptr<T>& b)
{
  if (a.expired() == true || b.expired() == true)
  {
    return false;
  }
  return a.lock() == b.lock();
}
