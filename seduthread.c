/*
 * seduthred.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: seduthread.c,v 1.148 2012/11/28 06:29:24 wendel Exp $
 */


#include <regex.h>

#include <vdr/plugin.h>

#include "softhdservice.h"
#include "seduthread.h"
#include "config.h"

//***************************************************************************
// Class cSeduThread
//***************************************************************************
//***************************************************************************
// Object
//***************************************************************************

cSeduThread::cSeduThread()
{
   loopActive = false;
   pixAverage = 0;
   image = 0;
   imageSize = 0;
   imageWidth = 0;
   imageHeight = 0;
   cineBarsHor = 0;
   cineBarsVer = 0;
}

cSeduThread::~cSeduThread()
{
   sedu.close();
   delete[] pixAverage;
}

//***************************************************************************
// Stop Thread
//***************************************************************************

void cSeduThread::Stop()
{   
   loopActive = false;
   waitCondition.Broadcast();    // wakeup the thread

   Cancel(3);                    // wait up to 3 seconds for thread was stopping
}

//***************************************************************************
// Action
//***************************************************************************

void cSeduThread::Action()
{
   MsTime wait = 0;
   cMutexLock lock(&mutex);

   tell(0, "atmo Thread started (pid=%d)", getpid());

   loopActive = true;
   pixAverage = new PixQueue[cfg.ledCount];

   sedu.setMode(cfg.seduMode, cfg.ledCount);

   while (loopActive && Running())
   {
      MsTime start = msNow();

      // work ...

      if (cfg.viewMode == vmAtmo)
      {
         if (grabImage() == success)
         {
            detectCineBars();
            putData();

            MsTime elapsed = msNow() - start;
            wait = 1000 / cfg.frequence - elapsed;
            tell(2, "sleeping %ldms (%d Hz)", wait, cfg.frequence);
         }
         else
         {
            if (sedu.isOpen())
            {
               error("softhddevice grab failed");
               tell(0, "Closing sedu interface to enable others (XBMC/boblight/...) to connect");
               sedu.close();
            }

            wait = 10000;   // retry softhd grab every 10 seconds
         }
      }
      else
      {
         putData();

         if (cfg.viewMode != vmRainbow && cfg.viewMode != vmColorWheel)
            wait = 500;      // less load on fixed color or black 
         else
            wait = 100;      // for Rainbow sleep always 100ms
      }
      
      waitCondition.TimedWait(mutex, wait);  // wait time in ms
   } 

   sedu.close();
   loopActive = false;

   delete[] pixAverage;
   pixAverage = 0;

   tell(0, "atmo thread ended (pid=%d)", getpid());
}

//***************************************************************************
// Grab Image
//***************************************************************************

int cSeduThread::grabImage()
{
   SoftHDDevice_AtmoGrabService_v1_1_t req;

   free(image);
   image = 0;

   cPlugin* softHdPlugin = cPluginManager::GetPlugin("softhddevice");
   int softHdGrabService = (softHdPlugin && softHdPlugin->Service(ATMO1_GRAB_SERVICE, 0));

   if (!softHdGrabService)
      return error("Can't find softhddevice %s, aborting grab, retrying in 10 seconds!", 
                   softHdPlugin ? "service" : "plugin");
 
   // grab image at sofhddevice

   req.width = cfg.grabWidth;
   req.height = cfg.grabHeight;
   req.img = 0;

   if (!softHdPlugin->Service(ATMO1_GRAB_SERVICE, &req) || !req.img)
      return fail;
   
   tell(2, "Got image with %dx%d pixel; %d bytes", req.width, req.height, req.size);

   image = (Pixel*)req.img;
   imageSize = req.size;
   imageWidth = req.width;
   imageHeight = req.height;

   return success;
}

//***************************************************************************
// Detect Cine Bars
//***************************************************************************

int cSeduThread::detectCineBars()
{
   const int threshold = 3;    // threshold for black level of cine bars
   Pixel* p;
   int off;

   // check horizontal bars

   if (cfg.detectCineBars == cbHorizontal || cfg.detectCineBars == cbBoth)
   {
      for (off = 0; off < imageHeight/5; off++)  // cinebar height max 1/5 of the screen height
      {
         int above = 0;
         
         for (int x = 0; x < imageWidth; x++)
         {
            p = &image[off*imageWidth + x];
            
            if (p->r > threshold || p->g > threshold || p->b > threshold)
               above++;
            
            p = &image[((imageHeight-1)-off)*imageWidth + x];
            
            if (p->r > threshold || p->g > threshold || p->b > threshold)
               above++;
         }
         
         if (above > imageWidth/8)              // max 1/8 failed pixel
            break;
      }
      
      if (cineBarsHor != off)
      {
         static int last = 0;
         static int count = 0;

         if (off != last)
         {
            last = off;
            count = 0;
         }

         if (count++ >= cfg.frequence)
         {
            count = 0;
            cineBarsHor = off;
            tell(0, "Switch horizontal cine bars to %d", cineBarsHor);
         }
      }
   }

   // check vertical bars
   
   if (cfg.detectCineBars == cbVertical || cfg.detectCineBars == cbBoth)
   {
      for (off = 0; off < imageWidth/5; off++)    // cinebar height max 1/5 of the screen width
      {
         int above = 0;
         
         for (int y = 0; y < imageHeight; y++)
         {
            p = &image[y*imageWidth + off];
            
            if (p->r > threshold || p->g > threshold || p->b > threshold)
               above++;
            
            p = &image[y*imageWidth + ((imageWidth-1)-off)];
            
            if (p->r > threshold || p->g > threshold || p->b > threshold)
            above++;
         }
         
         if (above > imageHeight/6)         // max 1/6 failed pixel
            break;
      }
      
      if (cineBarsVer != off)
      {
         static int last = 0;
         static int count = 0;

         if (off != last)
         {
            last = off;
            count = 0;
         }

         if (count++ >= cfg.frequence)
         {
            count = 0;

            cineBarsVer = off;
            tell(0, "Switch vertical cine bars to %d", cineBarsVer);
         }
      }
   }

   return done;
}

//***************************************************************************
// Put Data
//***************************************************************************

int cSeduThread::putData()
{
   Pixel pFixedCol = {0,0,0,0};

   if (!sedu.isOpen())
   {
      if (sedu.open() != success)
         return fail;
   }

   switch (cfg.viewMode)
   {
      case vmBlack:
      case vmFixedCol:
      {
         pFixedCol.r = cfg.viewMode == vmFixedCol ? cfg.fixedR : 0;
         pFixedCol.g = cfg.viewMode == vmFixedCol ? cfg.fixedG : 0;
         pFixedCol.b = cfg.viewMode == vmFixedCol ? cfg.fixedB : 0;

         if (cfg.viewMode != vmBlack)
         {
            gammaAdj(&pFixedCol);
            whiteAdj(&pFixedCol);
         }

         break;
      }
      case vmRainbow:
      {
         pFixedCol = getRainbowColor();
         break;
      }

      default: break;
   }
   
   sedu.writeStartSeq();
   
   // loop over all LEDs

   for (int led = 0; led < cfg.ledCount; led++)
   {   
      Pixel pixel = {0,0,0,0};
      Pixel* p = &pixel;

      if (cfg.viewMode == vmAtmo)
      {
         getPixel(led, p);
         pixAverage[led].push(p);

         pixAverage[led].getPixel(p);

         threshold(p);
         gammaAdj(p);
         whiteAdj(p);
      }
      else
         if (cfg.viewMode == vmColorWheel)
         {
            pixel = getColorWheel(1, led);
            p = &pixel;
         }
         else if (cfg.viewMode == vmColorWheelStatic)
         {
            pixel = getColorWheel(0, led);
            p = &pixel;
         }
         else
         {
            p = &pFixedCol;
         }
      
      sedu.writePix(p);
   }
   
   sedu.writeEndSeq();
   sedu.read();
   
   return success;
}

//***************************************************************************
// Get Pixel
//***************************************************************************

int cSeduThread::getPixel(int ledIdx, Pixel* pixel)
{
   cLed* led;
   PixSum sum;

   // some checks ...

   if (ledIdx >= cfg.ledCount)
   {
      tell(0, "Invalid led index %d, ignoring", ledIdx);
      return 0;
   }

   // get led ...

   led = &cfg.leds[ledIdx];

   // valid ?

   if (led->x < 0 || led->y < 0)
   {
      tell(0, "Invalid position for (%d/%d) led %d, ignoring", 
           led->x, led->y, ledIdx);
      return 0;
   }

   // calc average over pixels in 'deep'

   sum.clear();

   switch (led->lp)
   {
      case lpLeft:
      {
         for (int x = led->x; x <= led->x + cfg.xDeep; x++)
            sum.add(&image[led->y*imageWidth + x + cineBarsVer]);

         break;
      }
      case lpRight:
      {
         for (int x = led->x; x >= led->x - cfg.xDeep; x--)
            sum.add(&image[led->y*imageWidth + x - cineBarsVer]);

         break;
      }
      case lpTop:
      {
         for (int y = led->y; y <= led->y + cfg.yDeep; y++)
            sum.add(&image[(y+cineBarsHor)*imageWidth + led->x]);

         break;
      }

      case lpBottom:
      {
         for (int y = led->y; y >= led->y - cfg.yDeep; y--)
            sum.add(&image[(y-cineBarsHor)*imageWidth + led->x]);

         break;
      }
   }
   
   if (!sum.getCount())
      return error("Fatal missing range for led %d", ledIdx);

   sum.getAvg(pixel);

   return success;
}

//***************************************************************************
// Merge Pixel
//  - merge pixel p2 onto p1 - result stored in p1
//***************************************************************************

void cSeduThread::merge(Pixel* p1, Pixel* p2, int level)
{
   double factor = level / 100.0 / 2.0;

   if (p1 && p2)
   {
      p1->r = p2->r * factor + p1->r * (1.0-factor);
      p1->g = p2->g * factor + p1->g * (1.0-factor);
      p1->b = p2->b * factor + p1->b * (1.0-factor);
   }
}

//***************************************************************************
// Threshold
//***************************************************************************

void cSeduThread::threshold(Pixel* p)
{
   if (p)
   {
      if (p->r < cfg.threshold && p->g < cfg.threshold && p->b < cfg.threshold)
         memset(p, cfg.black, sizeof(Pixel));
   }
}

//***************************************************************************
// White Adjust
//***************************************************************************

void cSeduThread::whiteAdj(Pixel* p)
{
   if (p && !isBlack(p))
   {
      p->r = (double)p->r * (0.01 * cfg.adjRed);
      p->b = (double)p->b * (0.01 * cfg.adjBlue);
      p->g = (double)p->g * (0.01 * cfg.adjGreen);   
   }
}

//***************************************************************************
// White Adjust
//***************************************************************************

void cSeduThread::gammaAdj(Pixel* p)
{
   if (p && !isBlack(p) && cfg.gamma > 10)  
   {
      double g = cfg.gamma / 10.0;

      p->r = (unsigned char)(pow(p->r / 255.0, g) * 255.0);  
      p->g = (unsigned char)(pow(p->g / 255.0, g) * 255.0);
      p->b = (unsigned char)(pow(p->b / 255.0, g) * 255.0);
   }
}

//***************************************************************************
// Get Rainbow Color
//***************************************************************************

Pixel cSeduThread::getRainbowColor()
{
   static int rainbowColorTone = 0;
   static int callCount = 0;

   Pixel p = hsv2rgb(rainbowColorTone, 1, 1);

   if (!(callCount++ % (cfg.effectSpeed / 100)))
   {
      if (++rainbowColorTone >= 360)
         rainbowColorTone = 0;
   }

   gammaAdj(&p);
   whiteAdj(&p);

   return p;
}

Pixel cSeduThread::getColorWheel(int moving, int led)
{
   static int degrees = 0;
   static int callCount = 0;
   int steps = 360 / cfg.ledCount;
   int color = 0;
   
   if (moving && !(callCount++ % (cfg.effectSpeed / 100)))
   {
      // calculate spinng wheel with given effect speed

      if (led == 0)
         degrees += steps;

      if (degrees >= 360)
         degrees = 0;
   }

   // calculate color degrees

   color = led * steps + degrees;

   if (color >= 360)
      color -= 360;
   
   // convert color from HSV to RGB

   Pixel p = hsv2rgb(color, 1, 1);
   
   // create a single Pixel
   
   gammaAdj(&p);
   whiteAdj(&p);
   
   return p;
}

//***************************************************************************
// Class cSeduLine
//***************************************************************************
//***************************************************************************
// Object
//***************************************************************************

cSeduLine::cSeduLine()
{
   dataBytesSend = 0;
   fd = na;  
   bzero(&oldtio, sizeof(oldtio)); 
   deviceName = 0;

   setMode(smMiniDMX);
}

//***************************************************************************
// SEDU Mode
//***************************************************************************

void cSeduLine::setMode(SeduMode aMode, int channels)
{
   mode = aMode;

   switch (mode)
   {
      case smMiniDMX:
      {
         byteStart = 0x5A;
         byteMode = 0xA2;
         byteEnd = 0xA5;
         
         dataBytes = 512;
         
         break;
      }
      case smMiniDMX_A1:
      {
         byteStart = 0x5A;
         byteMode = 0xA1;
         byteEnd = 0xA5;
         
         dataBytes = 192;
         
         break;
      }
      case smMiniDMX_B0:
      {
         byteStart = 0x5A;
         byteMode = 0xB0;
         byteEnd = 0xA5;
         
         dataBytes = 768;
         
         break;
      }
      case smTpm2:
      {
         byteStart = 0xC9;
         byteMode = 0xDA;
         byteEnd = 0x36;
         
         dataBytes = channels*3;
         
         break;
      }
   }
}

//***************************************************************************
// Detect
//***************************************************************************

int cSeduLine::detect()
{
   const char* pattern = "module:ftdi_sio"; // move to config?

   FILE* fd;
   regex_t reg;
   char line[200];
   
   free(deviceName);
   deviceName = 0;

   if (regcomp(&reg, pattern, REG_EXTENDED | REG_NOSUB))
   {
      tell(0, "Invalid regular expression '%s' for usb device", pattern);
      regfree(&reg);
      return fail;
   }
   
   if (!(fd = fopen("/proc/tty/driver/usbserial", "r")))
   {
      tell(0, "Could not open '/proc/tty/driver/usbserial' '%m'");
      
      regfree(&reg);
      return fail;
   }
   
   while (fgets(line, sizeof(line), fd)) 
   {
      char* p;
      
      if (!regexec(&reg, line, 0, 0, 0) && (p = index(line, ':')))
      {
         *p = 0;
         asprintf(&deviceName, "/dev/ttyUSB%s", line);
         break;
      }
   }
   
   fclose(fd);
   regfree(&reg);
   
   if (!deviceName) 
   {
      tell(0, "Could not auto detect a usb device like '%s' in '/proc/tty/driver/usbserial'",pattern );
      return fail;
   }
   
   return success;
}

//***************************************************************************
// Open/Close
//***************************************************************************

int cSeduLine::open()
{
   struct termios newtio;

   if (detect() != success)
   {
      free(deviceName);
      deviceName = strdup("/dev/ttySEDU");
      tell(0, "Falling back to '%s'", deviceName);
   }

   if (isOpen()) 
      close();

	// open serial line with 8 data bits, no parity, 1 stop bit

   if ((fd = ::open(deviceName, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
   {
      fd = na;
      tell(0, "Error: Opening '%s' failed", deviceName);
  
      return fail;
   }

   tell(0, "Opening '%s' succeeded!", deviceName);
   
   // configure serial line

   tcgetattr(fd, &oldtio);
   bzero(&newtio, sizeof(newtio));

   /* BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
      CRTSCTS : output hardware flow control (only used if the cable has
                all necessary lines. See sect. 7 of Serial-HOWTO)
      CS8     : 8n1 (8bit,no parity,1 stopbit)
      CLOCAL  : local connection, no modem control
      CREAD   : enable receiving characters  */
   
   newtio.c_cflag = B500000 | CS8 | CLOCAL | CREAD;
   newtio.c_iflag = IGNPAR;
   newtio.c_oflag = 0;
   newtio.c_lflag = 0;

   if (tcsetattr(fd, TCSANOW, &newtio) != 0)
      tell(0, "tcsetattr failed!");

   tcflush(fd, TCIFLUSH);

   return success;
}

int cSeduLine::close()
{
   if (isOpen())
   {
      tcsetattr(fd, TCSANOW, &oldtio);
      ::close(fd);
      fd = na;
   }

   free(deviceName);
   deviceName = 0;

   return success;
}

//***************************************************************************
// Read/Write
//***************************************************************************

int cSeduLine::read()
{
   fd_set readfs;
   timeval tv;
   unsigned char c;
   MsTime start = msNow();
   
   if (!isOpen())
      return fail;

   // check if something to read ...
   
   tv.tv_sec = 0;
   tv.tv_usec = 100000;
   
   FD_ZERO(&readfs);
   FD_SET(fd, &readfs);
   select(fd+1, &readfs, 0, 0, &tv);

   if (FD_ISSET(fd, &readfs))
   {
      tell(2, "Received (after %ldms): ", (msNow()-start));

      while (::read(fd, &c, 1) > 0)
         tell(2, "%02X ", c);
   }
   else
   {
      tell(3, ".. no data available");
   }

   return 0;
}

int cSeduLine::write(unsigned char b)
{
   if (!isOpen())
      return 0;

   if (checkLine() != success)
      return 0;
   
   tell(3, "send: 0x%02X", b);

   return ::write(fd, &b, 1);
}

int cSeduLine::writeStartSeq()
{
   write(byteStart);
   write(byteMode);

   if (mode == smTpm2)
   {
      write((dataBytes & 0xFF00) >> 8);
      write(dataBytes & 0x00FF);
   }

   dataBytesSend = 0;

   return success;
}

int cSeduLine::writeEndSeq()
{
   while (dataBytesSend < dataBytes)
      dataBytesSend += write(0);

   write(byteEnd);

   tell(2, "Wrote %d RGB Values", dataBytesSend);

   return success;
}

//***************************************************************************
// Write Pixel
//***************************************************************************

int cSeduLine::writePix(Pixel* p)
{
   writeColor(p, 0);
   writeColor(p, 1);
   writeColor(p, 2);

   return success;
}

int cSeduLine::writeColor(Pixel* p, int index)
{
   switch (cfg.seduRGBOrder[index])
   {
      case 'R': dataBytesSend += write(p ? p->r : 0); break;
      case 'B': dataBytesSend += write(p ? p->b : 0); break;
      case 'G': dataBytesSend += write(p ? p->g : 0); break;
   }

   return success;
}

//***************************************************************************
// Check Line
//***************************************************************************

int cSeduLine::checkLine()
{
   fd_set port;

   if (!isOpen())
      return fail;

   // check if space to write ...
   
   FD_ZERO(&port);
   FD_SET(fd, &port);
   
   if (select(fd+1, 0, &port, 0, 0) == -1)
   {
      tell(0, "Error: select() %m");
      return fail;
   }

   return success;
}
