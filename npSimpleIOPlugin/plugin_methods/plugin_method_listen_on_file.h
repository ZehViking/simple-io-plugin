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

public:
  virtual void OnNewLine(const char* line, unsigned int len);

public:
  bool HasMethod(NPIdentifier name);
  bool Execute(
    const NPVariant *args,
    uint32_t argCount,
    NPVariant *result);

  bool Terminate();

private:
  void StartListening();

protected:
  NPObject* callback_;

  std::auto_ptr<utils::Thread> thread_;
  utils::TxtFileStream file_stream_;

};

#endif // PLUGIN_METHODS_PLUGIN_METHOD_LISTEN_ON_FILE_H_