///////////////////////////////////////////////////////////////////////////////
// FILE:          AndorAMH.h
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

#ifndef _ANDORAMH_H_
#define _ANDORAMH_H_

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include <string>
#include <map>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_PORT_CHANGE_FORBIDDEN    10004
#define ERR_UNRECOGNIZED_ANSWER      10009
#define ERR_UNSPECIFIED_ERROR        10010

#define ERR_OFFSET 10100

class AndorAMH : public CShutterBase<AndorAMH>
{
public:
   AndorAMH();
   ~AndorAMH();

   bool Busy();
   void GetName(char* pszName) const;
   int Initialize();
   int Shutdown();
      
   // Shutter API
   int SetOpen(bool open = true);
   int GetOpen(bool& open);
   int Fire(double deltaT);

   // action interface
   // ----------------
   int OnState(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnIntensity(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   int SetShutterPosition(bool state);
   bool initialized_;
   std:: string port_;
   MM::MMTime changedTime_;
   long intensity_;
   bool curState_;
};

#endif //_ANDORAMH_H_
