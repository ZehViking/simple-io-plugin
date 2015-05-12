/*
Simple IO Plugin
Copyright (c) 2015 Overwolf Ltd.
*/
#include "event.h"

using namespace utils;

//-----------------------------------------------------------------------------
Event::Event() : event_(nullptr) {
}

//-----------------------------------------------------------------------------
Event::~Event() {
  Destroy(); // no really necessary
}

//-----------------------------------------------------------------------------
bool Event::Create(bool manual_reset,
  bool initial_state,
  const char* name /*= nullptr*/,
  bool secured /*= true*/) {
  if (IsCreated()) {
    return false;
  }

  BYTE  sd[SECURITY_DESCRIPTOR_MIN_LENGTH];
  SECURITY_ATTRIBUTES  sa;
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = &sd;
  InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
  SetSecurityDescriptorDacl(&sd, TRUE, (PACL)0, FALSE);

  event_.Reset(CreateEventA(
    secured ? nullptr : &sa,
    manual_reset ? TRUE : FALSE,
    initial_state ? TRUE : FALSE,
    name));

  exit_event_.Reset(CreateEventA(NULL, true, false, nullptr));

  return IsCreated();
}

//-----------------------------------------------------------------------------
bool Event::Open(const char* name) {
  event_.Reset(OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, TRUE, name));
  exit_event_.Reset(CreateEventA(NULL, true, false, nullptr));
  
  return IsCreated();
}

//-----------------------------------------------------------------------------
void Event::Destroy() {
  if (IsCreated()) {
    SetEvent(exit_event_.Get());
    event_.Reset();
    exit_event_.Reset();
  }
}

//-----------------------------------------------------------------------------
bool Event::IsCreated() {
  return ((nullptr != event_.Get()) &&
          (nullptr != exit_event_.Get()));
}

//-----------------------------------------------------------------------------
bool Event::Wait(DWORD timeout_in_milliseconds) {
  if (!IsCreated()) {
    return false;
  }

  HANDLE events[] = {
    event_.Get(),
    exit_event_.Get()
  };

  DWORD ret = WaitForMultipleObjects(
    2, 
    events, 
    false, 
    timeout_in_milliseconds);
  return (WAIT_OBJECT_0 == ret);
}

//-----------------------------------------------------------------------------
bool Event::Signal() {
  if (!IsCreated()) {
    return false;
  }

  return (TRUE == SetEvent(event_.Get()));
}

//-----------------------------------------------------------------------------
bool Event::Reset() {
  if (!IsCreated()) {
    return false;
  }

  return (TRUE == ResetEvent(event_.Get()));
}