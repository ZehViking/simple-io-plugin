/*
  Simple IO Plugin
  Copyright (c) 2015 Overwolf Ltd.
*/
#ifndef PLUGIN_METHODS_PLUGIN_METHOD_GET_MONITOR_DPI_H_
#define PLUGIN_METHODS_PLUGIN_METHOD_GET_MONITOR_DPI_H_

#include "plugin_method.h"
#include <string>

class PluginMethodGetMonitorDPI : public PluginMethod {
public:
  PluginMethodGetMonitorDPI(NPObject* object, NPP npp);

public:
  virtual PluginMethod* Clone(
    NPObject* object, 
    NPP npp, 
    const NPVariant *args, 
    uint32_t argCount, 
    NPVariant *result);
  virtual bool HasCallback();
  virtual void Execute();
  virtual void TriggerCallback();

protected:
  double monitor_handle_;
  NPObject* callback_;

  // callack
  bool status_;
  double dpi_;
};


#endif // PLUGIN_METHODS_PLUGIN_METHOD_GET_MONITOR_DPI_H_