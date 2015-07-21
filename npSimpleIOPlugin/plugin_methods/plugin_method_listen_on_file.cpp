#include "plugin_method_listen_on_file.h"

#include <utils/File.h>
#include <utils/Encoders.h>
#include <utils/Thread.h>

const char kListenOnFileMethodName[] = "listenOnFile";

// listenOnFile( filename, skipToEnd, callback(status, data) )
PluginMethodListenOnFile::PluginMethodListenOnFile(NPObject* object, NPP npp) :
  PluginMethod(object, npp) {
}

//virtual 
PluginMethod* PluginMethodListenOnFile::Clone(
  NPObject* object, 
  NPP npp, 
  const NPVariant *args, 
  uint32_t argCount, 
  NPVariant *result) {
  return nullptr;
}

// virtual
bool PluginMethodListenOnFile::HasCallback() {
  return false;
}

// virtual
void PluginMethodListenOnFile::Execute() {
  //std::wstring wide_filename = utils::Encoders::utf8_decode(filename_);
}

// virtual
void PluginMethodListenOnFile::TriggerCallback() {
}

//virtual 
void PluginMethodListenOnFile::OnNewLine(const char* line, unsigned int len) {
  NPVariant args[2];
  NPVariant ret_val;

  BOOLEAN_TO_NPVARIANT(
    true,
    args[0]);

  STRINGN_TO_NPVARIANT(
    line,
    len,
    args[1]);

  // fire callback
  NPN_InvokeDefault(
    npp_,
    callback_,
    args,
    2,
    &ret_val);

  NPN_ReleaseVariantValue(&ret_val);
}

//virtual 
void PluginMethodListenOnFile::OnError(const char* message, unsigned int len) {
  NPVariant args[2];
  NPVariant ret_val;

  BOOLEAN_TO_NPVARIANT(
    false,
    args[0]);

  STRINGN_TO_NPVARIANT(
    message,
    len,
    args[1]);

  // fire callback
  NPN_InvokeDefault(
    npp_,
    callback_,
    args,
    2,
    &ret_val);

  NPN_ReleaseVariantValue(&ret_val);
}


bool PluginMethodListenOnFile::HasMethod(NPIdentifier name) {
  static NPIdentifier id_listen_on_file =
    NPN_GetStringIdentifier(kListenOnFileMethodName);
  if (name == id_listen_on_file) {
    return true;
  }

  return false;
}

bool PluginMethodListenOnFile::Execute(
  const NPVariant *args,
  uint32_t argCount,
  NPVariant *result) {

  std::string filename;
  bool skip_to_end = false;

  try {
    if (argCount < 3 ||
      !NPVARIANT_IS_STRING(args[0]) ||
      !NPVARIANT_IS_BOOLEAN(args[1]) ||
      !NPVARIANT_IS_OBJECT(args[2])) {
      NPN_SetException(
        __super::object_, 
        "invalid or missing params passed to function - expecting 3 params: "
        "filename, skipToEnd, callback(status, data)");
      return false;
    }

    callback_ = NPVARIANT_TO_OBJECT(args[2]);
    skip_to_end = NPVARIANT_TO_BOOLEAN(args[1]);

    // add ref count to callback object so it won't delete
    NPN_RetainObject(callback_);

    filename.append(
      NPVARIANT_TO_STRING(args[0]).UTF8Characters,
      NPVARIANT_TO_STRING(args[0]).UTF8Length);  
  } catch(...) {

  }

  if (nullptr == thread_.get()) {
    thread_.reset(new utils::Thread);
    if (!thread_->Start()) {
      NPN_SetException(
        __super::object_,
        "an unexpected error occurred - couldn't start file listening thread");
      return false;
    }
  }

  std::wstring wide_filename = utils::Encoders::utf8_decode(filename);

  if (!file_stream_.Initialize(wide_filename.c_str(), this, skip_to_end)) {
    NPN_SetException(
      __super::object_,
      "an unexpected error occurred - couldn't open the file for read access");
    return false;
  }

  return thread_->PostTask(
    std::bind(
    &PluginMethodListenOnFile::StartListening,
    this));
}

bool PluginMethodListenOnFile::Terminate() {
  file_stream_.StopListening();

  if (nullptr != thread_.get()) {
    thread_->Stop();
  }
  return true;
}

void PluginMethodListenOnFile::StartListening() {
  file_stream_.StartListening();
}
