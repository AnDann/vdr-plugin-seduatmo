/*
 * stup.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: seduatmo.c,v 1.64 2012/11/28 06:29:24 wendel Exp $
 */

//***************************************************************************
// Setup
//***************************************************************************

class cSeduSetup : public cMenuSetupPage, public cSeduService
{
   public:

      cSeduSetup();

      static const char* seduRGBOrders[];
      static int toOrderIndex(const char* order);

   protected:

      virtual void Setup();
      virtual eOSState ProcessKey(eKeys Key);
      virtual void Store();

      const char* seduModes[smCount];
      const char* cineBars[cbCount];
      int rgbOrderIndex;

      cSeduConfig data;
};
