/*
Simple IO Plugin
Copyright (c) 2015 Overwolf Ltd.
*/
#include "TxtFileStream.h"
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace utils;

// leverage sequential blocks
const unsigned int kBufferSize = 1024 * 1024 * 2;
const unsigned int kWaitTimeout = 100;
const char kErrorFileNotAccessible[] = "file no longer accessible";
const char kErrorFileReadRetError[] = "file read returned error";
const char kErrorListenThreadStopped[] = "listening thread stopped - did you call listenOnFile again?";
const char kErrorFileTruncated[] = "the file was truncated - please restart the listener";
const char kErrorExceptionInListener[] = "exception caught in listener thread - please restart the listener";
const char kInfoListenThreadExit[] = "listener thread exited";

namespace utils {

unsigned int safe_read(int desc, void *ptr, size_t len) {
  unsigned int n_chars;

  if (len <= 0)
    return len;

#ifdef EINTR
  do {
    n_chars = _read(desc, ptr, len);
  } while (n_chars < 0 && errno == EINTR);
#else
  n_chars = read(desc, ptr, len);
#endif
  return n_chars;
}

}; // utils

TxtFileStream::TxtFileStream() :
  file_handle_(-1),
  delegate_(nullptr),
  skip_to_end_(false),
  listening_(false) {
}

TxtFileStream::~TxtFileStream() {
  StopListening();
  delegate_ = nullptr;
  reset_event_.Destroy();
}

bool TxtFileStream::Initialize(
  const wchar_t* filename, 
  TxtFileStreamDelegate* delegate,
  bool skip_to_end) {

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

  skip_to_end_ = skip_to_end;

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

  // skip read pointer to end
  if (skip_to_end_) {
    {
      CriticalSectionLock lock(critical_section_);
      if (-1 == file_handle_) {
        return false;
      }

      lseek(file_handle_, 0, SEEK_END);
    }
  }

  int buffer_size = kBufferSize;
  char* buffer = new char[buffer_size];

  int len = 0; 
  long current_file_len = GetFileSize();
  bool was_error_triggered = false;

  while ((len >= 0) && (current_file_len >= 0)) {
    {
      
      CriticalSectionLock lock(critical_section_);
      
      if (!ReadNext(
        len, 
        buffer, 
        buffer_size, 
        current_file_len)) {
        // ReadNext will always trigger an error if returns false
        was_error_triggered = true;
        break;
      }

    } // CriticalSectionLock


    if (0 == len) {
      if (reset_event_.Wait(kWaitTimeout)) {
        delegate_->OnError(
          kErrorListenThreadStopped,
          sizeof(kErrorListenThreadStopped));
        was_error_triggered = true;
        break; // it means we were signaled
      }
    }
  
  } // while

  // this is so that we don't trigger multiple errors
  if (!was_error_triggered) {
    delegate_->OnError(
      kInfoListenThreadExit,
      sizeof(kInfoListenThreadExit));
  }

  listening_ = false;

  delete[] buffer;

  return true;
}

bool TxtFileStream::StopListening() {
  {
    CriticalSectionLock lock(critical_section_);
    if (-1 != file_handle_) {
      _close(file_handle_);
      file_handle_ = -1;
    }
  }
  
  // signal event
  reset_event_.Signal();

  return true;
}

bool TxtFileStream::ReadNext(
  int &len, 
  char* buffer, 
  int buffer_size, 
  long &current_file_len) {
  
  __try {
    if (-1 == file_handle_) {
      delegate_->OnError(
        kErrorFileNotAccessible,
        sizeof(kErrorFileNotAccessible));
      return false;
    }


    len = safe_read(file_handle_, buffer, buffer_size);
    if (len < 0) {
      delegate_->OnError(
        kErrorFileReadRetError,
        sizeof(kErrorFileReadRetError));
      return false;
    }

    long size_change = GetFileSize();
    if (size_change < current_file_len) {
      // the file was truncated?
      delegate_->OnError(
        kErrorFileTruncated,
        sizeof(kErrorFileTruncated));
      return false;
    }

    current_file_len = size_change;

    ParseLines(buffer, len);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    delegate_->OnError(
      kErrorExceptionInListener,
      sizeof(kErrorExceptionInListener));
    return false;
  }
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

long TxtFileStream::GetFileSize() {
  struct _stat file_info;
  
  if (-1 == file_handle_) {
    return -1;
  }

  if (0 != _fstat(file_handle_, &file_info)) {
    return -1;
  }

  return file_info.st_size;
}