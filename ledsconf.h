/*
 * ledsconf.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: ledsconf.h,v 1.9 2012/11/20 14:32:28 wendel Exp $
 */

#ifndef _LED_CONF_H_
#define _LED_CONF_H_

#include <vdr/plugin.h>
#include <vdr/tools.h>

#include "seduservice.h"

//***************************************************************************
// cLedConf
//***************************************************************************

class cLedConf : public cListObject, public cSeduService
{
   public:
      
      cLedConf();
      
      bool Parse(const char* s);

      int X()                { return x; }
      int ToX()              { return toX; }
      int Y()                { return y; }
      int ToY()              { return toY; }
      int Pos()              { return lp; }
      int isValid()          { return x > na && y > na && lp > na; }
      const char* RgbOrder() { return rgbOrder; }

   private:

      bool parseRange(const char*& s, int& from, int& to);
      bool parseRgbOrder(const char*& p, char* rgbOrder);
      const char* skipWs(const char* p);

      int x;
      int toX;
      int y;
      int toY;
      int lp;
      char rgbOrder[3+TB];
};

//***************************************************************************
// cLedConfs
//***************************************************************************

class cLedConfs : public cConfig<cLedConf> 
{

};

extern cLedConfs ledConfs;

#endif // _LED_CONF_H_
