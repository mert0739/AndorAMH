///////////////////////////////////////////////////////////////////////////////
// FILE:          AndorAMH.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Andor AMH200 adapter, based on Prior LumenPro adapter
// COPYRIGHT:     University of California, San Francisco, 2006
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 06/01/2006
// AUTHOR:        John James, jj400@cam.ac.uk, 25/8/2015
//

#ifdef WIN32
   #include <windows.h>
   #define snprintf _snprintf 
#endif

#include "AndorAMH.h"
#include <cstdio>
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <boost/lexical_cast.hpp>

const char* g_AndorAMH="AndorAMH";

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_AndorAMH, MM::ShutterDevice, "Andor AMH200-FOS shutter");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   if (strcmp(deviceName, g_AndorAMH) == 0)
   {
      AndorAMH* s = new AndorAMH();
      return s;
   }
   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// AndorAMH 
// ~~~~~~~~

AndorAMH::AndorAMH() :
   initialized_(false), changedTime_(0.0), intensity_(1),
   curState_(false),  port_("Andor-AMH200-FOS")  //Included port string, as this should always be correct
{
   InitializeDefaultErrorMessages();
   SetErrorText(ERR_UNRECOGNIZED_ANSWER, "Unrecognised answer received from the device");

   // create pre-initialization properties
   // ------------------------------------

   // Name
   CreateProperty(MM::g_Keyword_Name, g_AndorAMH, MM::String, true);

   // Description
   CreateProperty(MM::g_Keyword_Description, "Andor AMH200-FOS shutter", MM::String, true);

   // Port
   CPropertyAction* pAct = new CPropertyAction (this, &AndorAMH::OnPort);
   CreateProperty(MM::g_Keyword_Port, "Andor-AMH200-FOS", MM::String, false, pAct, true);

   EnableDelay();

}

AndorAMH::~AndorAMH()
{
   Shutdown();
}

void AndorAMH::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_AndorAMH);
}

int AndorAMH::Initialize()
{
   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &AndorAMH::OnState);
   int ret = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;
   AddAllowedValue(MM::g_Keyword_State, "0");
   AddAllowedValue(MM::g_Keyword_State, "1");
      
   // Delay
   // -----
   pAct = new CPropertyAction (this, &AndorAMH::OnDelay);
   ret = CreateProperty(MM::g_Keyword_Delay, "0.0", MM::Float, false, pAct);
   if (ret != DEVICE_OK)
      return ret;
   
   // Intensity
   // ---------
   const char* intensityPropName = "Intensity";
   pAct = new CPropertyAction (this, &AndorAMH::OnIntensity);
   ret = CreateProperty(intensityPropName, "100", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;
   ret = SetPropertyLimits(intensityPropName,1,100); //This provides a slider but with no access to Off (0) state
   if (ret != DEVICE_OK)
      return ret;

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;
   
   // set initial values
   SetProperty(MM::g_Keyword_State, curState_ ? "1" : "0");
   
   // Set Time for Busy flag
   changedTime_ = GetCurrentMMTime();
   
   initialized_ = true;

   return DEVICE_OK;
}

int AndorAMH::Shutdown()
{
   if (initialized_)
   {
      int ret=SetShutterPosition(false);   // To make sure the shutter is closed before quitting MM
      if (ret != DEVICE_OK)
         return ret;
      initialized_ = false;
   }
   return DEVICE_OK;
}

bool AndorAMH::Busy()
{
   MM::MMTime interval = GetCurrentMMTime() - changedTime_;
   MM::MMTime delay(GetDelayMs()*1000.0);
   if (interval < delay)
      return true;
   else
      return false;
}

int AndorAMH::SetOpen(bool open)
{
   long pos;
   if (open)
      pos = 1;
   else
      pos = 0;
   return SetProperty(MM::g_Keyword_State, CDeviceUtils::ConvertToString(pos));
}

int AndorAMH::GetOpen(bool& open)
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_State, buf);
   if (ret != DEVICE_OK)
      return ret;
   long pos = atol(buf);
   pos == 1 ? open = true : open = false;

   return DEVICE_OK;
}

int AndorAMH::Fire(double /*deltaT*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

/**
 * Sends an open/close command through the serial port.
 */
int AndorAMH::SetShutterPosition(bool state)
{

   // First Clear serial port from previous stuff, now using MM's function
   int ret = PurgeComPort(port_.c_str());
   if (ret != DEVICE_OK)
      return ret;

   std::ostringstream command;
   command << "LIGHT," << (state ? intensity_ : 0);

   // send command
   ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\r");
   if (ret != DEVICE_OK)
      return ret;

   // block/wait for acknowledge, or until we time out;
   std::string answer;
   ret = GetSerialAnswer(port_.c_str(), "\r", answer);
   if (ret != DEVICE_OK)
      return ret;

   // Set timer for Busy signal
   changedTime_ = GetCurrentMMTime();

   if (answer.substr(0,1).compare("R") == 0)
   {
      return DEVICE_OK;
   }
   else if (answer.substr(0, 1).compare("E") == 0 && answer.length() > 2)
   {
      int errNo = atoi(answer.substr(2).c_str());
      std::string messg = boost::lexical_cast<std::string,int>(errNo);
      LogMessage("Error in received answer, giving code: " + (messg),true);
      return ERR_OFFSET + errNo;
   }

   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int AndorAMH::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // will return cached value
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);
      curState_ = pos == 0 ? false : true;

      // apply the value
      return SetShutterPosition(curState_);
   }

   return DEVICE_OK;
}

int AndorAMH::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(port_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         // revert
         pProp->Set(port_.c_str());
         return ERR_PORT_CHANGE_FORBIDDEN;
      }

      pProp->Get(port_);
   }

   return DEVICE_OK;
}

int AndorAMH::OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(this->GetDelayMs());
   }
   else if (eAct == MM::AfterSet)
   {
      double delay;
      pProp->Get(delay);
      this->SetDelayMs(delay);
   }

   return DEVICE_OK;
}

int AndorAMH::OnIntensity(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(intensity_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(intensity_);
      if (curState_)
         return SetShutterPosition(curState_);
   }

   return DEVICE_OK;
}