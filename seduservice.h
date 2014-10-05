/*
 * seduservice.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: seduservice.h,v 1.3 2012/11/22 18:02:59 wendel Exp $
 */

#ifndef __SEDU_SERVICE_H
#define __SEDU_SERVICE_H

//***************************************************************************
// Pixel - format as provided by softhddevice 
//***************************************************************************

struct Pixel
{
   unsigned char b;
   unsigned char g;
   unsigned char r;
   unsigned char a;
};

//***************************************************************************
// Sedu Service
//***************************************************************************

class cSeduService
{
   public:

      enum Cinebars
      {
         cbHorizontal,
         cbVertical,
         cbBoth,
         cbCount
      };

      enum SeduMode
      {
         smMiniDMX,
         smTpm2,
		 smMiniDMX_A1,
		 smMiniDMX_B0,
         smCount
      };

      enum ViewMode
      {
         vmAtmo,
         vmFixedCol,
         vmRainbow,
         vmColorWheel,
         vmColorWheelStatic,
         vmBlack,
         vmDetached,
         vmCount
      };

      enum LedPosition
      {
         lpTop,
         lpLeft,
         lpBottom,
         lpRight
      };

      struct cLed
      {
         int x;
         int toX;
         int y;
         int toY;
         LedPosition lp;
      };

      // static

      void rgb2hsv(int r, int g, int b, double* h, double* s, double* v);
      Pixel hsv2rgb(int h, double s, double v);

      static const char* toName(ViewMode vm);
      static const char* viewModes[];
};

#endif // __SEDU_SERVICE_H
