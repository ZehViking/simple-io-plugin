/*
Simple IO Plugin
Copyright (c) 2015 Overwolf Ltd.
*/
#ifndef UTILS_EVENT_H_
#define UTILS_EVENT_H_

#include "ScopedHandle.h"

namespace utils {

class Event {
public:
  Event();
  Event(const Event& obj) {};
  virtual ~Event();

public:
  bool Create(
    bool manual_reset,
    bool initial_state,
    const char* name = nullptr,
    bool secured = true);
  bool Open(const char* name);
  void Destroy();

  bool IsCreated();

  bool Wait(DWORD timeout_in_milliseconds = INFINITE);
  bool Signal();
  bool Reset();

private:
  EventScopedHandle event_;

  // used to quit the Wait function when |Destroy| is called
  EventScopedHandle exit_event_;
};

}; // namespace utils;

#endif // UTILS_EVENT_H_