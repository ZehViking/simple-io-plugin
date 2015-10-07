#include "utils.h"

#include <shlwapi.h>
#pragma comment(lib,"Shlwapi.lib")

const char kListenOnFileMethodName[] = "listenOnFile";
const char kStopFileListenMethodName[] = "stopFileListen";

const char kQAFlagsPath[] = "Software\\OverwolfQA";
const char kSimpleIOTraceEnabled[] = "SimpleIOTrace";

namespace utils {

bool ShouldWriteToTrace() {
  DWORD Value;
  DWORD BufferSize = sizeof(DWORD);
  LSTATUS Ret = SHRegGetValue(
    HKEY_CURRENT_USER,
    kQAFlagsPath,
    kSimpleIOTraceEnabled,
    (SRRF_RT_REG_BINARY | SRRF_RT_REG_DWORD),
    NULL,
    &Value,
    &BufferSize);

  return (Ret == ERROR_SUCCESS && BufferSize == sizeof(DWORD) && Value != 0);
}

};