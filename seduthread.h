/*
 * seduthread.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: seduthread.h,v 1.64 2012/11/28 06:29:24 wendel Exp $
 */

#include <termios.h>
#include <queue>

#include <vdr/thread.h>

#include "common.h"
#include "config.h"

class PixQueue
{
   public:

      PixQueue()            { clear(); }
      virtual ~PixQueue()   { clear(); }
      void clear()          { while (!pQueue.empty()) pQueue.pop(); r=g=b=0; }
      int getCount()        { return pQueue.size(); }

      void push(Pixel* p) 
      { 
         pQueue.push(*p);
 
         r += p->r;
         g += p->g;
         b += p->b;        

         // remove old entrys from queue and sum

         while (pQueue.size() > cfg.average)
         {
            r -= pQueue.front().r;
            g -= pQueue.front().g;
            b -= pQueue.front().b;
            
            pQueue.pop();
         }
      }

      void getPixel(Pixel* p)
      {
         p->r = r / pQueue.size();
         p->g = g / pQueue.size();
         p->b = b / pQueue.size();
      }
      
   protected:

      std::queue <Pixel> pQueue;
      unsigned int r;
      unsigned int g;
      unsigned int b;
};

class PixSum
{
   public:

      PixSum()       { clear(); }

      void clear()   { r=g=b=count=0;  }
      int getCount() { return count; }

      void add(Pixel* p)
      {
         r += p->r;
         g += p->g;
         b += p->b;
         count++;
      }

      void getAvg(Pixel* p)
      {
         p->r = r / count;
         p->g = g / count;
         p->b = b / count;
      }

   protected:
      
      unsigned int r;
      unsigned int g;
      unsigned int b;
      int count;
};

//***************************************************************************
// cSeduLine - Serial Communication
//***************************************************************************

class cSeduLine : public cSeduService
{
   public:

      cSeduLine();
      ~cSeduLine()  { close(); }

      int open();
      int close();
      int isOpen()  { return fd != na; }

      int checkLine();
      int read();

      int write(unsigned char b);

      int writeStartSeq();
      int writeEndSeq();
      int writePix(int ledIdx, Pixel* p);
      int writeColor(int ledIdx, Pixel* p, int index);

      // set ..

      void setMode(SeduMode aMode, int channels = 0);

   private:

      int detect();
   
      // data

      int dataBytesSend;

      int mode;
      int dataBytes;
      int byteMode;
      int byteStart;
      int byteEnd;

      char* deviceName;
      int fd;
      struct termios oldtio;
};

//***************************************************************************
// SEDU Thread
//***************************************************************************

class cSeduThread : public cThread, public cSeduService
{
   public:

      cSeduThread();
      ~cSeduThread();

      int isRunning() { return Running(); }

      // interface

      void Stop();
     
   private:

      void Action(void);

      int grabImage();
      int detectCineBars();
      int putData();

      void threshold(Pixel* p);
      void whiteAdj(Pixel* p);
      void gammaAdj(Pixel* p);
      void merge(Pixel* p1, Pixel* p2, int level = 100);

      int getPixel(int ledIdx, Pixel* pixel);
      int isBlack(Pixel* p) { return p->r < cfg.threshold && p->g < cfg.threshold &&  p->b < cfg.threshold; }

      // data

      cSeduLine sedu;

      cMutex mutex;
      cCondVar waitCondition;
      int loopActive;

      Pixel* image;
      PixQueue* pixAverage;
      int cineBarsHor;
      int cineBarsVer;
      int imageSize;
      int imageWidth;
      int imageHeight;

      Pixel getRainbowColor();
      Pixel getColorWheel(int moving = 0, int led = 0);
};
