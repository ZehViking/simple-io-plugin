/*
  Simple IO Plugin
  Copyright (c) 2015 Overwolf Ltd.
*/
#ifndef PLUGIN_METHODS_PLUGIN_METHOD_LISTEN_ON_FILE_H_
#define PLUGIN_METHODS_PLUGIN_METHOD_LISTEN_ON_FILE_H_

#include "plugin_method.h"
#include <memory>
#include <string>
#include <utils/TxtFileStream.h>

namespace utils {
class Thread;
}

class PluginMethodListenOnFile : public PluginMethod,
                                 public utils::TxtFileStreamDelegate {
public:
  PluginMethodListenOnFile(NPObject* object, NPP npp);

// PluginMethod
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

// utils::TxtFileStreamDelegate
public:
  virtual void OnNewLine(const char* line, unsigned int len);
  virtual void OnError(const char* message, unsigned int len);

public:
  bool HasMethod(NPIdentifier name);
  bool Execute(
    NPIdentifier name,
    const NPVariant *args,
    uint32_t argCount,
    NPVariant *result);

  bool Terminate();

private:
  void StartListening();

  bool ExecuteListenOnFile(
    const NPVariant *args,
    uint32_t argCount,
    NPVariant *result);
  bool ExecuteStopFileListen(
    const NPVariant *args,
    uint32_t argCount,
    NPVariant *result);

protected:
  NPObject* callback_;

  std::auto_ptr<utils::Thread> thread_;
  utils::TxtFileStream file_stream_;

  NPIdentifier id_listen_on_file_;
  NPIdentifier id_stop_file_listen_;
};

#endif // PLUGIN_METHODS_PLUGIN_METHOD_LISTEN_ON_FILE_H_