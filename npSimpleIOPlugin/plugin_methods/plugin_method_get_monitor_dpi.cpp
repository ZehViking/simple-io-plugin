#include "plugin_method_get_monitor_dpi.h"

// getMonitorDPI( monitorHandleId, callback(status, dpi) )
PluginMethodGetMonitorDPI::PluginMethodGetMonitorDPI(NPObject* object, NPP npp) : 
  PluginMethod(object, npp) {
}

//virtual 
PluginMethod* PluginMethodGetMonitorDPI::Clone(
  NPObject* object, 
  NPP npp, 
  const NPVariant *args, 
  uint32_t argCount, 
  NPVariant *result) {

  PluginMethodGetMonitorDPI* clone = 
    new PluginMethodGetMonitorDPI(object, npp);

  try {
    if (argCount < 2 ||
      !NPVARIANT_IS_DOUBLE(args[0]) ||
      !NPVARIANT_IS_OBJECT(args[1])) {
      NPN_SetException(
        __super::object_, 
        "invalid params passed to function");
      delete clone;
      return nullptr;
    }

    clone->callback_ = NPVARIANT_TO_OBJECT(args[1]);
    // add ref count to callback object so it won't delete
    NPN_RetainObject(clone->callback_);

    clone->monitor_handle_ = NPVARIANT_TO_DOUBLE(args[0]);

    return clone;
  } catch(...) {

  }

  delete clone;
  return nullptr;
}

// virtual
bool PluginMethodGetMonitorDPI::HasCallback() {
  return (nullptr != callback_);
}

// virtual
void PluginMethodGetMonitorDPI::Execute() {
  // default to fail
  status_ = false;

#ifndef DPI_ENUMS_DECLARED
  typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
  } PROCESS_DPI_AWARENESS;

  typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
  } MONITOR_DPI_TYPE;

#define DPI_ENUMS_DECLARED
#endif // (DPI_ENUMS_DECLARED)

  typedef HRESULT(CALLBACK *SetProcessDpiAwarenessPTR)(
    _In_ PROCESS_DPI_AWARENESS value);
  typedef HRESULT(CALLBACK *GetProcessDpiAwarenessPTR)(
    _In_opt_ HANDLE hprocess, 
    _Out_ PROCESS_DPI_AWARENESS *value);
  typedef HRESULT(CALLBACK *GetDpiForMonitorPTR)(
    _In_ HMONITOR hmonitor,
    _In_ MONITOR_DPI_TYPE dpiType,
    _Out_ UINT *dpiX,
    _Out_ UINT *dpiY);

  // try to access DPI API (Windows 8.1 and above)
  HMODULE hmod = LoadLibraryA("Shcore.dll");
  if (nullptr == hmod) {
    return;
  }

  SetProcessDpiAwarenessPTR set_dpi_awarnss = (SetProcessDpiAwarenessPTR)GetProcAddress(hmod, "SetProcessDpiAwareness");
  GetProcessDpiAwarenessPTR get_dpi_awarnss = (GetProcessDpiAwarenessPTR)GetProcAddress(hmod, "GetProcessDpiAwareness");
  GetDpiForMonitorPTR get_dpi_for_monitor = (GetDpiForMonitorPTR)GetProcAddress(hmod, "GetDpiForMonitor");

  if ((nullptr == set_dpi_awarnss) ||
    (nullptr == get_dpi_awarnss) ||
    (nullptr == get_dpi_for_monitor)) {
    FreeLibrary(hmod);
    return;
  }

  // 1. get and remember the current state
  PROCESS_DPI_AWARENESS current_state;
  if (FAILED(get_dpi_awarnss(NULL, &current_state))) {
    FreeLibrary(hmod);
    return;
  }

  // 2. set to per-monitor-awareness
  if (FAILED(set_dpi_awarnss(PROCESS_PER_MONITOR_DPI_AWARE))) {
    FreeLibrary(hmod);
    return;
  }

  // 3. get the monitor's DPI
  UINT dpiX, dpiY;
  if (SUCCEEDED(get_dpi_for_monitor(
    (HMONITOR)(uint32_t)monitor_handle_, 
    (MONITOR_DPI_TYPE)MDT_EFFECTIVE_DPI, 
    &dpiX, 
    &dpiY))) {
    dpi_ = (uint32_t)dpiX;
    status_ = true;
  }

  // 4. reset back to the original awareness
  set_dpi_awarnss(current_state);

  FreeLibrary(hmod);
}

// virtual
void PluginMethodGetMonitorDPI::TriggerCallback() {
  NPVariant args[2];
  NPVariant ret_val;

  DOUBLE_TO_NPVARIANT(
    dpi_,
    args[1]);

  BOOLEAN_TO_NPVARIANT(
    status_,
    args[0]);

  // fire callback
  NPN_InvokeDefault(
    __super::npp_,
    callback_,
    args,
    2,
    &ret_val);

  NPN_ReleaseVariantValue(&ret_val);
}