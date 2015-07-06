#include "plugin_method_write_localappdata_file.h"

#include "utils/File.h"
#include "utils/Encoders.h"

// writeLocalAppDataFile( filename, content, callback(status, message) )
PluginMethodWriteLocalAppDataFile::PluginMethodWriteLocalAppDataFile(
  NPObject* object, NPP npp) :
  PluginMethod(object, npp) {
}

//virtual 
PluginMethod* PluginMethodWriteLocalAppDataFile::Clone(
  NPObject* object, 
  NPP npp, 
  const NPVariant *args, 
  uint32_t argCount, 
  NPVariant *result) {

  PluginMethodWriteLocalAppDataFile* clone =
    new PluginMethodWriteLocalAppDataFile(object, npp);

  try {
    if (argCount < 3 ||
      !NPVARIANT_IS_STRING(args[0]) ||
      !NPVARIANT_IS_STRING(args[1]) ||
      !NPVARIANT_IS_OBJECT(args[2])) {
      NPN_SetException(
        __super::object_, 
        "invalid params passed to function");
      delete clone;
      return nullptr;
    }

    clone->callback_ = NPVARIANT_TO_OBJECT(args[2]);
    // add ref count to callback object so it won't delete
    NPN_RetainObject(clone->callback_);

    // convert into std::string
    clone->filename_.append(
      NPVARIANT_TO_STRING(args[0]).UTF8Characters,
      NPVARIANT_TO_STRING(args[0]).UTF8Length);

    clone->content_.append(
      NPVARIANT_TO_STRING(args[1]).UTF8Characters,
      NPVARIANT_TO_STRING(args[1]).UTF8Length);

    return clone;
  } catch(...) {

  }

  delete clone;
  return nullptr;
}

// virtual
bool PluginMethodWriteLocalAppDataFile::HasCallback() {
  return (nullptr != callback_);
}

// virtual
void PluginMethodWriteLocalAppDataFile::Execute() {
  message_.clear();
  
  std::wstring wide_filename = utils::Encoders::utf8_decode(filename_);

  // make sure there are no .. tricks
  if (std::wstring::npos != wide_filename.find(L"..")) {
    status_ = false;
    message_ = "can't use \"..\" in the filename parameter";
    return;
  }

  std::wstring path = utils::File::GetSpecialFolderWide(CSIDL_LOCAL_APPDATA);
  std::wstring filename = path + L"\\" + wide_filename;

  try {
    status_ = utils::File::WriteTextFile(filename, content_);
  } catch(...) {
    status_ = false;
  }

  if (!status_) {
    message_ = "unexpected error when trying to write to ";
    message_ += utils::Encoders::utf8_encode(filename);
  }
}

// virtual
void PluginMethodWriteLocalAppDataFile::TriggerCallback() {
  NPVariant args[2];
  NPVariant ret_val;

  BOOLEAN_TO_NPVARIANT(
    status_,
    args[0]);

  STRINGN_TO_NPVARIANT(
    message_.c_str(),
    message_.size(),
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