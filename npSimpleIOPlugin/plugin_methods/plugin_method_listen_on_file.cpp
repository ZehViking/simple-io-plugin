#include "plugin_method_listen_on_file.h"

#include <utils/File.h>
#include <utils/Encoders.h>
#include <utils/Thread.h>
#include <shlwapi.h>
#pragma comment(lib,"Shlwapi.lib")


const char kListenOnFileMethodName[] = "listenOnFile";
const char kStopFileListenMethodName[] = "stopFileListen";

const char kQAFlagsPath[] = "Software\\OverwolfQA";
const char kSimpleIOTraceEnabled[] = "SimpleIOTrace";

bool WriteToTrace() {
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

// listenOnFile("id", filename, skipToEnd, callback(id, status, data) )
// stopFileListen("id" )
PluginMethodListenOnFile::PluginMethodListenOnFile(NPObject* object, NPP npp) :
  PluginMethod(object, npp) {

  id_listen_on_file_ =
    NPN_GetStringIdentifier(kListenOnFileMethodName);

  id_stop_file_listen_ =
    NPN_GetStringIdentifier(kStopFileListenMethodName);

}

//virtual 
void PluginMethodListenOnFile::OnNewLine(
  const char* id, 
  const char* line, 
  unsigned int len) {

  TextFileIdToCallbackMap::iterator iter = ids_to_callbacks_.find(id);
  if ((iter == ids_to_callbacks_.end()) || (nullptr == iter->second)) {
    OutputDebugStringA("SimpleIOPlugin OnNewLine - missing callback error!");
    return;
  }

  static bool write_to_trace = WriteToTrace();

  if (write_to_trace) {
    std::string str = "SimpleIOPlugin OnNewLine - [";
    str += id;
    str += "] ";
    str += line;
    OutputDebugStringA(str.c_str());
  }

  NPVariant args[3];
  NPVariant ret_val;

  STRINGN_TO_NPVARIANT(
    id,
    strlen(id),
    args[0]);

  BOOLEAN_TO_NPVARIANT(
    true,
    args[1]);

  STRINGN_TO_NPVARIANT(
    line,
    len,
    args[2]);

  // fire callback
  NPN_InvokeDefault(
    npp_,
    iter->second,
    args,
    3,
    &ret_val);

  NPN_ReleaseVariantValue(&ret_val);
}

//virtual 
void PluginMethodListenOnFile::OnError(
  const char* id, 
  const char* message, 
  unsigned int len) {

  TextFileIdToCallbackMap::iterator iter = ids_to_callbacks_.find(id);
  if ((iter == ids_to_callbacks_.end()) || (nullptr == iter->second)) {
    OutputDebugStringA("SimpleIOPlugin OnNewLine - missing callback error!");
    return;
  }

  static bool write_to_trace = WriteToTrace();

  if (write_to_trace) {
    std::string str = "SimpleIOPlugin OnError - [";
    str += id;
    str += "] ";
    str += message;
    OutputDebugStringA(str.c_str());
  }

  NPVariant args[3];
  NPVariant ret_val;

  STRINGN_TO_NPVARIANT(
    id,
    strlen(id),
    args[0]);

  BOOLEAN_TO_NPVARIANT(
    false,
    args[1]);

  STRINGN_TO_NPVARIANT(
    message,
    len,
    args[2]);

  // fire callback
  NPN_InvokeDefault(
    npp_,
    iter->second,
    args,
    3,
    &ret_val);

  NPN_ReleaseVariantValue(&ret_val);
}


bool PluginMethodListenOnFile::HasMethod(NPIdentifier name) {
  if (name == id_listen_on_file_) {
    return true;
  }

  if (name == id_stop_file_listen_) {
    return true;
  }

  return false;
}

bool PluginMethodListenOnFile::Execute(
  NPIdentifier name,
  const NPVariant *args,
  uint32_t argCount,
  NPVariant *result) {

  if (name == id_listen_on_file_) {
    return ExecuteListenOnFile(args, argCount, result);
  }

  if (name == id_stop_file_listen_) {
    return ExecuteStopFileListen(args, argCount, result);
  }

  return false;
}

bool PluginMethodListenOnFile::Terminate() {
  TextFileThreadMap::iterator iter = threads_.begin();
  while (iter != threads_.end()) {
    iter->second.first.StopListening();
    iter->second.second.Stop();
    iter++;
  }
  threads_.clear();

  // clear callbacks
  TextFileIdToCallbackMap::iterator iter_callbacks = ids_to_callbacks_.begin();
  while (iter_callbacks != ids_to_callbacks_.end()) {
    NPN_ReleaseObject(iter_callbacks->second);
    iter_callbacks++;
  } 
  ids_to_callbacks_.clear();

  return true;
}

void PluginMethodListenOnFile::StartListening(const char* id) {
  TextFileThreadMap::iterator iter = threads_.find(id);
  if (iter == threads_.end()) {
    OutputDebugStringA(
      "PluginMethodListenOnFile::StartListening - bad id passed!");
    delete[] id;
    return; //??
  }

  if (!iter->second.first.StartListening()) {
    OutputDebugStringA(
      "PluginMethodListenOnFile::StartListening - failed to StartListening");
  }

  delete[] id;
}

// listenOnFile(id, filename, skipToEnd, callback(status, data) )
bool PluginMethodListenOnFile::ExecuteListenOnFile(
  const NPVariant *args,
  uint32_t argCount,
  NPVariant *result) {
  std::string id;
  std::string filename;
  bool skip_to_end = false;
  NPObject* callback = nullptr;

  try {
    if (argCount < 4 ||
      !NPVARIANT_IS_STRING(args[0]) ||
      !NPVARIANT_IS_STRING(args[1]) ||
      !NPVARIANT_IS_BOOLEAN(args[2]) ||
      !NPVARIANT_IS_OBJECT(args[3])) {
      NPN_SetException(
        object_, 
        "invalid or missing params passed to function - expecting 4 params: "
        "id, filename, skipToEnd, callback(id, status, data)");
      return false;
    }

    callback = NPVARIANT_TO_OBJECT(args[3]);
    skip_to_end = NPVARIANT_TO_BOOLEAN(args[2]);

    // add ref count to callback object so it won't delete
    NPN_RetainObject(callback);

    id.append(
      NPVARIANT_TO_STRING(args[0]).UTF8Characters,
      NPVARIANT_TO_STRING(args[0]).UTF8Length);

    filename.append(
      NPVARIANT_TO_STRING(args[1]).UTF8Characters,
      NPVARIANT_TO_STRING(args[1]).UTF8Length);  
  } catch(...) {

  }


  TextFileThreadMap::iterator iter = threads_.find(id);
  if (iter != threads_.end()) {
    if (!iter->second.first.StopListening()) {
      NPN_SetException(
        __super::object_,
        "an unexpected error occurred - couldn't stop existing listener");
      return false;
    }
    
    if (!iter->second.second.Stop()) {
      NPN_SetException(
        __super::object_,
        "an unexpected error occurred - couldn't stop existing listener thread");
      return false;
    }
  }

  std::wstring wide_filename = utils::Encoders::utf8_decode(filename);
  if (!threads_[id].first.Initialize(
    id.c_str(),
    wide_filename.c_str(), 
    this, 
    skip_to_end)) {
    NPN_SetException(
      __super::object_,
      "an unexpected error occurred - couldn't open the file for read access");
    return false;
  }

  
  if (!threads_[id].second.Start()) {
    NPN_SetException(
      __super::object_,
      "an unexpected error occurred - couldn't start file listening thread");
    return false;
  }

  // set a callback
  TextFileIdToCallbackMap::iterator iter_callback = ids_to_callbacks_.find(id);
  if (iter_callback != ids_to_callbacks_.end()) {
    if (nullptr != ids_to_callbacks_[id]) {
      NPN_ReleaseObject(ids_to_callbacks_[id]);
    }
  }

  ids_to_callbacks_[id] = callback;

  char* id_to_pass = new char[id.size()+1];
  strcpy(id_to_pass, id.c_str());

  return threads_[id].second.PostTask(
    std::bind(
    &PluginMethodListenOnFile::StartListening,
    this,
    id_to_pass));
}

bool PluginMethodListenOnFile::ExecuteStopFileListen(
  const NPVariant *args,
  uint32_t argCount,
  NPVariant *result) {

  std::string id;
  try {
    if (argCount < 1 || !NPVARIANT_IS_STRING(args[0])) {
      NPN_SetException(
        object_,
        "invalid or missing params passed to function - expecting 1 params: "
        "id");
      return false;
    }

    id.append(
      NPVARIANT_TO_STRING(args[0]).UTF8Characters,
      NPVARIANT_TO_STRING(args[0]).UTF8Length);
  }
  catch (...) {

  }

  TextFileThreadMap::iterator iter = threads_.find(id);
  if (iter != threads_.end()) {
    if (!iter->second.first.StopListening()) {
      NPN_SetException(
        __super::object_,
        "an unexpected error occurred - couldn't stop existing listener");
      return false;
    }

    if (!iter->second.second.Stop()) {
      NPN_SetException(
        __super::object_,
        "an unexpected error occurred - couldn't stop existing listener thread");
      return false;
    }

    threads_.erase(id);

    if (nullptr != ids_to_callbacks_[id]) {
      NPN_ReleaseObject(ids_to_callbacks_[id]);
      ids_to_callbacks_[id] = nullptr;
      ids_to_callbacks_.erase(id);
    }
  }

  return true;
}
