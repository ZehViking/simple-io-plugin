/*
  Simple IO Plugin
  Copyright (c) 2014 Overwolf Ltd.
*/
#include "nsScriptableObjectSimpleIO.h"
#include "utils/Thread.h"
#include "utils/File.h"
#include "utils/TxtFileStream.h"

#include "plugin_methods/plugin_method.h"
#include "plugin_methods/plugin_method_file_exists.h"
#include "plugin_methods/plugin_method_is_directory.h"
#include "plugin_methods/plugin_method_get_text_file.h"
#include "plugin_methods/plugin_method_get_binary_file.h"
#include "plugin_methods/plugin_method_write_localappdata_file.h"
#include "plugin_methods/plugin_method_get_monitor_dpi.h"

#include "plugin_methods/plugin_method_listen_on_file.h"

#define REGISTER_METHOD(name, class) { \
  methods_[NPN_GetStringIdentifier(name)] = \
    new class(this, npp_); \
}

#define REGISTER_GET_PROPERTY(name, csidl) { \
  properties_[NPN_GetStringIdentifier(name)] = \
    utils::File::GetSpecialFolderUtf8(csidl); \
}

nsScriptableObjectSimpleIO::nsScriptableObjectSimpleIO(NPP npp) :
  nsScriptableObjectBase(npp),
  shutting_down_(false) {
}

nsScriptableObjectSimpleIO::~nsScriptableObjectSimpleIO(void) {
  shutting_down_ = true;
  
  if (thread_.get()) {
    thread_->Stop();
  }

  if (nullptr != listen_on_file_method_.get()) {
    listen_on_file_method_->Terminate();
    listen_on_file_method_.reset();
  }
}

bool nsScriptableObjectSimpleIO::Init() {
#pragma region public methods
  REGISTER_METHOD("fileExists", PluginMethodFileExists);
  REGISTER_METHOD("isDirectory", PluginMethodIsDirectory);
  REGISTER_METHOD("getTextFile", PluginMethodGetTextFile);
  REGISTER_METHOD("getBinaryFile", PluginMethodGetBinaryFile);
  REGISTER_METHOD("writeLocalAppDataFile", PluginMethodWriteLocalAppDataFile);
  REGISTER_METHOD("getMonitorDPI", PluginMethodGetMonitorDPI);

  listen_on_file_method_.reset(new PluginMethodListenOnFile(this, npp_));
#pragma endregion public methods

#pragma region read-only properties
  REGISTER_GET_PROPERTY("PROGRAMFILES", CSIDL_PROGRAM_FILES);
  REGISTER_GET_PROPERTY("PROGRAMFILESX86", CSIDL_PROGRAM_FILESX86);
  REGISTER_GET_PROPERTY("COMMONFILES", CSIDL_PROGRAM_FILES_COMMON);
  REGISTER_GET_PROPERTY("COMMONFILESX86", CSIDL_PROGRAM_FILES_COMMONX86);
  REGISTER_GET_PROPERTY("COMMONAPPDATA", CSIDL_COMMON_APPDATA);
  REGISTER_GET_PROPERTY("DESKTOP", CSIDL_DESKTOP);
  REGISTER_GET_PROPERTY("WINDIR", CSIDL_WINDOWS);
  REGISTER_GET_PROPERTY("SYSDIR", CSIDL_SYSTEM);
  REGISTER_GET_PROPERTY("SYSDIRX86", CSIDL_SYSTEMX86);

  REGISTER_GET_PROPERTY("MYDOCUMENTS", CSIDL_MYDOCUMENTS);
  REGISTER_GET_PROPERTY("MYVIDEOS", CSIDL_MYVIDEO);
  REGISTER_GET_PROPERTY("MYPICTURES", CSIDL_MYPICTURES);
  REGISTER_GET_PROPERTY("MYMUSIC", CSIDL_MYMUSIC);
  REGISTER_GET_PROPERTY("COMMONDOCUMENTS", CSIDL_COMMON_DOCUMENTS);
  REGISTER_GET_PROPERTY("FAVORITES", CSIDL_FAVORITES);
  REGISTER_GET_PROPERTY("FONTS", CSIDL_FONTS);
  REGISTER_GET_PROPERTY("HISTORY", CSIDL_HISTORY);
  REGISTER_GET_PROPERTY("STARTMENU", CSIDL_STARTMENU);

  REGISTER_GET_PROPERTY("LOCALAPPDATA", CSIDL_LOCAL_APPDATA);

  // DPI
  SetDPIProperty();

#pragma endregion read-only properties

  thread_.reset(new utils::Thread());
  return thread_->Start();
}

bool nsScriptableObjectSimpleIO::HasMethod(NPIdentifier name) {
#ifdef _DEBUG
  NPUTF8* name_utf8 = NPN_UTF8FromIdentifier(name);
  NPN_MemFree((void*)name_utf8);
#endif

  // listenOnFile
  if (listen_on_file_method_->HasMethod(name)) {
    return true;
  }

  // does the method exist?
  return (methods_.find(name) != methods_.end());
}

bool nsScriptableObjectSimpleIO::Invoke(
  NPIdentifier name, 
  const NPVariant *args, 
  uint32_t argCount, 
  NPVariant *result) {
#ifdef _DEBUG
      NPUTF8* szName = NPN_UTF8FromIdentifier(name);
      NPN_MemFree((void*)szName);
#endif

  // handle listenOnFile
  if (listen_on_file_method_->HasMethod(name)) {
    return listen_on_file_method_->Execute(name, args, argCount, result);
  }

  // dispatch method to appropriate handler
  MethodsMap::iterator iter = methods_.find(name);
  
  if (iter == methods_.end()) {
    // should never reach here
    NPN_SetException(this, "bad function called??");
    return false;
  }

  PluginMethod* plugin_method = 
    iter->second->Clone(this, npp_, args, argCount, result);

  if (nullptr == plugin_method) {
    return false;
  }

  // post to separate thread so that we are responsive
  return thread_->PostTask(
    std::bind(
    &nsScriptableObjectSimpleIO::ExecuteMethod, 
    this,
    plugin_method));
}

/************************************************************************/
/* Public properties
/************************************************************************/
bool nsScriptableObjectSimpleIO::HasProperty(NPIdentifier name) {
#ifdef _DEBUG
  NPUTF8* name_utf8 = NPN_UTF8FromIdentifier(name);
  NPN_MemFree((void*)name_utf8);
#endif

  // does the property exist?
  return (properties_.find(name) != properties_.end());
}

bool nsScriptableObjectSimpleIO::GetProperty(
  NPIdentifier name, NPVariant *result) {

  PropertiesMap::iterator iter = properties_.find(name);
  if (iter == properties_.end()) {
    NPN_SetException(this, "unknown property");
    return true;
  }

  char *resultString = (char*)NPN_MemAlloc(iter->second.size());
  memcpy(
    resultString, 
    iter->second.c_str(), 
    iter->second.size());

  STRINGN_TO_NPVARIANT(resultString, iter->second.size(), *result);

  return true;
}

bool nsScriptableObjectSimpleIO::SetProperty(
  NPIdentifier name, const NPVariant *value) {
  NPN_SetException(this, "this property is read-only!");
  return true;
}

/************************************************************************/
/*
/************************************************************************/
void nsScriptableObjectSimpleIO::ExecuteMethod(PluginMethod* method) {
  if (shutting_down_) {
    return;
  }

  if (nullptr == method) {
    return;
  }

  method->Execute();

  if (!method->HasCallback()) {
    delete method;
    return;
  }

  NPN_PluginThreadAsyncCall(
    npp_, 
    nsScriptableObjectSimpleIO::ExecuteCallback, 
    method);
}

//static
void nsScriptableObjectSimpleIO::ExecuteCallback(void* method) {
  if (nullptr == method) {
    return;
  }

  PluginMethod* plugin_method = reinterpret_cast<PluginMethod*>(method);
  plugin_method->TriggerCallback();

  delete plugin_method;
}

bool GetRegistryKeyValue(HKEY key, const char* subkey, const char* value_name, DWORD& value) {
  DWORD size = sizeof(DWORD);
  if (ERROR_SUCCESS != RegGetValue(
    key,
    subkey,
    value_name, 
    RRF_RT_DWORD,
    NULL,
    (PVOID)&value,
    &size)) {
    return false;
  }

  return true;
}

bool GetRegistryKeyValue(HKEY key, const char* subkey, const char* value_name, std::string& value) {
  // first get the size
  DWORD size = 0;
  if (ERROR_SUCCESS != RegGetValue(
    key,
    subkey,
    value_name,
    RRF_RT_REG_SZ,
    NULL,
    NULL,
    &size)) {
    return false;
  }

  if (size <= 0) {
    return false;
  }

  std::vector<char> temp_data;
  // allocate memory
  temp_data.resize(size);
  if (ERROR_SUCCESS != RegGetValue(
    key,
    subkey,
    value_name,
    RRF_RT_REG_SZ,
    NULL,
    temp_data.data(),
    &size)) {
    return false;
  }

  value.assign(temp_data.begin(), temp_data.end());
  return true;
}

void nsScriptableObjectSimpleIO::SetDPIProperty() {
  // on windows 8 and above, according to this: 
  // https://technet.microsoft.com/en-us/library/dn528846.aspx
  // 
  // we need to see if the user overrides the scaling by
  // reading the Win8DpiScaling for 0 or 1
  // 1 - use GetDeviceCaps LOGPIXELSX
  // 0 - read scale from: DesktopDPIOverride where:
  //  0 - 96
  //  1 - 120
  //  2 - 144
  //  3 - 192
  DWORD win8_dpi_scaling = 1;
  if (GetRegistryKeyValue(
    HKEY_CURRENT_USER, 
    "Control Panel\\Desktop",
    "Win8DpiScaling",
    win8_dpi_scaling)) {
    
    if (0 == win8_dpi_scaling) {
      std::string desktop_dpi_override;
      if (GetRegistryKeyValue(
        HKEY_CURRENT_USER,
        "Control Panel\\Desktop",
        "DesktopDPIOverride",
        desktop_dpi_override)) {

        properties_[NPN_GetStringIdentifier("DPI")] = "96";

        if ('1' == desktop_dpi_override.at(0)) {
          properties_[NPN_GetStringIdentifier("DPI")] = "120";
        } else if ('2' == desktop_dpi_override.at(0)) {
          properties_[NPN_GetStringIdentifier("DPI")] = "144";
        } else if ('3' == desktop_dpi_override.at(0)) {
          properties_[NPN_GetStringIdentifier("DPI")] = "192";
        }

        return;
      }
    }
  }

  HDC screen = GetDC(0);
  int dpiX = GetDeviceCaps(screen, LOGPIXELSX);
  ReleaseDC(0, screen);

  char convert[100] = { 0 };
  sprintf_s(convert, 100, "%d", dpiX);

  properties_[NPN_GetStringIdentifier("DPI")] = convert;
}
