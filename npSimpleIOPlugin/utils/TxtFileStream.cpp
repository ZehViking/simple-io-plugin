/*
Simple IO Plugin
Copyright (c) 2015 Overwolf Ltd.
*/
#include "TxtFileStream.h"
#include <io.h>
#include <fcntl.h>

using namespace utils;

// leverage sequential blocks
const unsigned int kBufferSize = 1024 * 1024 * 2;
const unsigned int kWaitTimeout = 100;

namespace utils {

unsigned int safe_read(int desc, void *ptr, size_t len) {
  unsigned int n_chars;

  if (len <= 0)
    return len;

#ifdef EINTR
  do {
    n_chars = read(desc, ptr, len);
  } while (n_chars < 0 && errno == EINTR);
#else
  n_chars = read(desc, ptr, len);
#endif
  return n_chars;
}

};

TxtFileStream::TxtFileStream() :
  file_handle_(-1),
  delegate_(nullptr),
  listening_(false) {
}

TxtFileStream::~TxtFileStream() {
  StopListening();
  delegate_ = nullptr;
  reset_event_.Destroy();
}

bool TxtFileStream::Initialize(
  const wchar_t* filename, 
  TxtFileStreamDelegate* delegate) {

  if ((nullptr == filename) || (nullptr == delegate)) {
    return false;
  }

  if (!StopListening()) {
    return false;
  }

  if (!reset_event_.IsCreated()) {
    if (!reset_event_.Create(false, true)) {
      return false;
    }
  }

  reset_event_.Reset();

  delegate_ = delegate;

  accumulated_line_.clear();

  {
    CriticalSectionLock lock(critical_section_);
    file_handle_ = _wopen(filename, O_RDONLY);
  }

  return (-1 != file_handle_);
}

bool TxtFileStream::StartListening() {
  if (listening_) {
    return false;
  }

  int buffer_size = kBufferSize;
  char* buffer = new char[buffer_size];

  int len = 0;

  while (len >= 0) {
    {
      CriticalSectionLock lock(critical_section_);
      if (-1 == file_handle_) {
        break;
      }

      len = safe_read(file_handle_, buffer, buffer_size);
      //len = _fread_nolock(buffer, 1, buffer_size, file_handle_);
      ParseLines(buffer, len);
    }

    if (0 == len) {
      if (reset_event_.Wait(kWaitTimeout)) {
        break; // it means we were signaled
      }
    }
  }

  listening_ = false;

  delete[] buffer;

  return true;
}

bool TxtFileStream::StopListening() {
  {
    CriticalSectionLock lock(critical_section_);
    if (-1 != file_handle_) {
      close(file_handle_);
      file_handle_ = -1;
    }
  }
  
  // signal event
  reset_event_.Signal();

  return true;
}

void TxtFileStream::ParseLines(const char* lines, int len) {
  if ((nullptr == lines) || (0 == len)) {
    return;
  }

  const char* start_line = lines;
  int start_line_index = 0;

  for (int i = 0; i < len; i++) {
    int eol_len = 0;

    if ((i + 1 < len) &&
        (lines[i] == '\r') &&
        (lines[i+1] == '\n')) {
      eol_len = 2;
    } else if (lines[i] == '\n') {
      eol_len = 1;
    }

    if (eol_len > 0) {
      accumulated_line_.append(&lines[start_line_index], &lines[i]);
      delegate_->OnNewLine(
        accumulated_line_.c_str(),
        accumulated_line_.size());
      accumulated_line_.clear();

      i += (eol_len-1); // we'll add +1 in the for loop
      if (i + 1 < len) {
        start_line = &lines[i + 1];
        start_line_index = i + 1;
      } else {
        start_line = nullptr;
      }
    }
  }

  // we may end up with a new line - handle it
  if (start_line != nullptr) {
    if (*start_line == '\n') {
      delegate_->OnNewLine("", 0);
    } else if (*start_line != '\r') {
      accumulated_line_.append(start_line, &lines[len]);
    }
  }
}
