/*
  Simple IO Plugin
  Copyright (c) 2015 Overwolf Ltd.
*/
#ifndef UTILS_TXT_FILE_STREAM_H_
#define UTILS_TXT_FILE_STREAM_H_

#include <string>
#include "CriticalSectionLock.h"
#include "Event.h"


namespace utils {

class TxtFileStreamDelegate {
public:
  virtual void OnNewLine(const char* line, unsigned int len) = 0;
};

class TxtFileStream {
public:
  TxtFileStream();
  virtual ~TxtFileStream();

public:
  bool Initialize(
    const wchar_t* filename, 
    TxtFileStreamDelegate* delegate);
  
  bool StartListening();
  bool StopListening();

private:
  void ParseLines(const char* lines, int len);

private:
  int file_handle_;
  TxtFileStreamDelegate* delegate_;
  std::string accumulated_line_;

  bool listening_;

  CriticalSection critical_section_;

  Event reset_event_;
};


}; // namespace utils;

#endif // UTILS_TXT_FILE_STREAM_H_