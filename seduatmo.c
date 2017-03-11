/*
 * seduatmo.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: seduatmo.c,v 1.64 2012/11/28 06:29:24 wendel Exp $
 */

#include <vdr/plugin.h>

#include "seduthread.h"
#include "config.h"
#include "ledsconf.h"
#include "setup.h"

//***************************************************************************
// 
//***************************************************************************

static const char *VERSION        = "0.0.4";
static const char *DESCRIPTION    = "sedu ambi light control with data from softhddevice";
static const char *MAINMENUENTRY  = "Seduatmo";

//***************************************************************************
// Plugin
//***************************************************************************

class cPluginSeduatmo : public cPlugin 
{
   public:
      
      cPluginSeduatmo(void);
      virtual ~cPluginSeduatmo();
      virtual const char* Version(void)          { return VERSION; }
      virtual const char* Description(void)      { return DESCRIPTION; }
      virtual const char* CommandLineHelp(void)  { return 0; }
      virtual bool ProcessArgs(int argc, char* argv[]);
      virtual bool Initialize(void);
      virtual bool Start(void);
      virtual void Stop(void);
      virtual void Housekeeping(void)   { };
      virtual void MainThreadHook(void) { };
      virtual cString Active(void);
      virtual time_t WakeupTime(void);
      virtual const char* MainMenuEntry(void) { return cfg.showMainmenu ?  MAINMENUENTRY : 0; }
      virtual cOsdObject* MainMenuAction(void);
      virtual cMenuSetupPage* SetupMenu(void) { return new cSeduSetup; }
      virtual bool SetupParse(const char* Name, const char* Value);
      virtual bool Service(const char* Id, void* Data = NULL);
      virtual const char** SVDRPHelpPages(void);
      virtual cString SVDRPCommand(const char* Command, const char* Option, int &ReplyCode);

      int startAtmo();
      int stopAtmo();
      cSeduThread* update;

      int isRunning()
      {
         if (!update)
            return no;

         return update->isRunning();
      }

   private:

      cLedConfs ledConfs;
};

//***************************************************************************
// Plugins Main Menu
//***************************************************************************

class cSeduPluginMenu : public cMenuSetupPage
{
   public:

      cSeduPluginMenu(const char* title, cPluginSeduatmo* aPlugin);
      virtual ~cSeduPluginMenu() { };
      
      virtual eOSState ProcessKey(eKeys key);

   protected:

      void Store();
      cPluginSeduatmo* plugin;
      int effectSpeed;
};

cSeduPluginMenu::cSeduPluginMenu(const char* title, cPluginSeduatmo* aPlugin)
{
   SetTitle(title ? title : "");
   plugin = aPlugin;
   effectSpeed = cfg.effectSpeed;
   
   Clear();

   cOsdMenu::Add(new cMenuEditStraItem(tr("View Mode"), 
                                       (int*)&cfg.viewMode, 
                                       (int)cSeduService::vmCount,
                                       cSeduService::viewModes));

   Add(new cMenuEditIntItem(tr("Fixed Color Red"), &cfg.fixedR, 0, 255));
   Add(new cMenuEditIntItem(tr("Fixed Color Green"), &cfg.fixedG, 0, 255));
   Add(new cMenuEditIntItem(tr("Fixed Color Blue"), &cfg.fixedB, 0, 255));

   Add(new cMenuEditIntItem(tr("Effect Speed [ms]"), &effectSpeed, 100, 5000));

   SetHelp(0, 0, 0, 0);

   Display();
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cSeduPluginMenu::ProcessKey(eKeys key)
{
   eOSState state = cOsdMenu::ProcessKey(key);
   
   if (key == kLeft || key == kRight)
   {
      if (cfg.viewMode == cSeduService::vmDetached && plugin->isRunning())
         plugin->stopAtmo();
      else if (cfg.viewMode != cSeduService::vmDetached && !plugin->isRunning())
         plugin->startAtmo();

      Display();
   }

   if (key == kOk)
   {
      cfg.effectSpeed = effectSpeed;
      Store();

      return osEnd;
   }

   return state;
}

void cSeduPluginMenu::Store()
{  
   plugin->SetupStore("FixedColorRed", cfg.fixedR);
   plugin->SetupStore("FixedColorGreen", cfg.fixedG);
   plugin->SetupStore("FixedColorBlue", cfg.fixedB);
   plugin->SetupStore("ViewMode", (int)cfg.viewMode);
   plugin->SetupStore("EffectSpeed", cfg.effectSpeed);

   Setup.Save();
}

//***************************************************************************
// Plugin 
//***************************************************************************

cPluginSeduatmo::cPluginSeduatmo(void)
{
   update = 0;
}

cPluginSeduatmo::~cPluginSeduatmo()
{
   stopAtmo();
}

bool cPluginSeduatmo::ProcessArgs(int argc, char* argv[])
{
   return true;
}

bool cPluginSeduatmo::Initialize(void)
{
   char* tmp;

   asprintf(&tmp, "%s/%s", cPlugin::ConfigDirectory("seduatmo"), "seduatmo.conf");

   if (!ledConfs.Load(tmp, true))
   {
      free(tmp);
      return fail;
   }

   free(tmp);

   cfg.createLeds(&ledConfs);

   return true;
}

bool cPluginSeduatmo::Start(void)
{
   startAtmo();

   return true;
}

int cPluginSeduatmo::startAtmo()
{
   if (update)
   {
      update->Stop();
      delete update;
   }

   update = new cSeduThread();

   if (cfg.grabWidth <= 0 || cfg.grabHeight <= 0 || !cfg.ledCount)
      tell(0, "Error: Invalid configuration in seduatmo.conf, aborting");
   else
      update->Start();

   return done;
}

int cPluginSeduatmo::stopAtmo()
{
   if (update)
      update->Stop();

   delete update;
   update = 0;

   return done;
}

void cPluginSeduatmo::Stop(void)
{
   stopAtmo();
}

cString cPluginSeduatmo::Active(void)
{
  return 0;
}

time_t cPluginSeduatmo::WakeupTime(void)
{
  return 0;
}

cOsdObject* cPluginSeduatmo::MainMenuAction(void)
{
   return new cSeduPluginMenu(MAINMENUENTRY, this);
}

bool cPluginSeduatmo::SetupParse(const char* Name, const char* Value)
{
   if      (!strcasecmp(Name, "LogLevel"))         cfg.loglevel = atoi(Value);
   else if (!strcasecmp(Name, "ShowMainmenu"))     cfg.showMainmenu = atoi(Value);
   else if (!strcasecmp(Name, "ViewMode"))         cfg.viewMode = (cSeduService::ViewMode)atoi(Value);

   else if (!strcasecmp(Name, "DetectCineBars"))   cfg.detectCineBars = (cSeduService::Cinebars)atoi(Value);

   else if (!strcasecmp(Name, "Threshold"))        cfg.threshold = atoi(Value);
   else if (!strcasecmp(Name, "Gamma"))            cfg.gamma = atoi(Value);
   else if (!strcasecmp(Name, "Frequence"))        cfg.frequence = atoi(Value);
   else if (!strcasecmp(Name, "Blacklevel"))       cfg.black = atoi(Value);
   else if (!strcasecmp(Name, "Smoothness"))       cfg.average = atoi(Value);

   else if (!strcasecmp(Name, "HorizontalDepth"))  cfg.xDeep = atoi(Value);
   else if (!strcasecmp(Name, "VerticalDepth"))    cfg.yDeep = atoi(Value);

   else if (!strcasecmp(Name, "LevelRed"))         cfg.adjRed = atoi(Value);
   else if (!strcasecmp(Name, "LevelGreen"))       cfg.adjGreen = atoi(Value);
   else if (!strcasecmp(Name, "LevelBlue"))        cfg.adjBlue = atoi(Value);

   else if (!strcasecmp(Name, "FixedColorRed"))    cfg.fixedR = atoi(Value);
   else if (!strcasecmp(Name, "FixedColorGreen"))  cfg.fixedG = atoi(Value);
   else if (!strcasecmp(Name, "FixedColorBlue"))   cfg.fixedB = atoi(Value);
   else if (!strcasecmp(Name, "EffectSpeed"))      cfg.effectSpeed = atoi(Value);

   else if (!strcasecmp(Name, "SeduMode"))         cfg.seduMode = (cSeduService::SeduMode)atoi(Value);
   else if (!strcasecmp(Name, "SeduRGBOrder"))     strcpy(cfg.seduRGBOrder, Value);

   else
      return false;

   return true;
}

bool cPluginSeduatmo::Service(const char* Id, void* Data)
{
   return false;
}

cString cPluginSeduatmo::SVDRPCommand(const char* Command, const char* Option, int &ReplyCode)
{
   if (!strcasecmp(Command, "MODE")) 
   {
      if (Option && strcasecmp(Option, "atmo") == 0) 
      {
         cfg.viewMode = cSeduService::vmAtmo;
         startAtmo();
         ReplyCode = 550;
         return "atmo mode activated";
      }
      else if (Option && strcasecmp(Option, "fixed") == 0) 
      {
         cfg.viewMode = cSeduService::vmFixedCol;
         startAtmo();
         ReplyCode = 550;
         return "fixed color activated";
      }
      else if (Option && strcasecmp(Option, "rainbow") == 0) 
      {
         cfg.viewMode = cSeduService::vmRainbow;
         startAtmo();
         ReplyCode = 550;
         return "rainbow effect activated";
      }
      else if (Option && strcasecmp(Option, "wheel") == 0) 
      {
         cfg.viewMode = cSeduService::vmColorWheel;
         startAtmo();
         ReplyCode = 550;
         return "color wheel effect activated";
      }
      else if (Option && strcasecmp(Option, "wheelstatic") == 0) 
      {
         cfg.viewMode = cSeduService::vmColorWheelStatic;
         startAtmo();
         ReplyCode = 550;
         return "static color wheel activated";
      }
      else if (Option && strcasecmp(Option, "black") == 0) 
      {
         cfg.viewMode = cSeduService::vmBlack;
         startAtmo();
         
         ReplyCode = 550;
         return "stripes black";
      }
      else if (Option && strcasecmp(Option, "detach") == 0) 
      {
         cfg.viewMode = cSeduService::vmDetached;
         stopAtmo();
         
         ReplyCode = 550;
         return "stripes detached";
      }
      else
      {
         ReplyCode = 901;
         return "Error: Unexpected option";
      }
   }

   return 0;
}

const char** cPluginSeduatmo::SVDRPHelpPages(void) 
{ 
   static const char* HelpPages[] = 
   {
      "MODE <mode>\n"
      "    Set mode {atmo|fixed|rainbow|wheel|wheelstatic|black|detach}\n",
      0
   };

   return HelpPages;
}

//***************************************************************************
// Class Setup
//***************************************************************************

const char* cSeduSetup::seduRGBOrders[] = 
{
   "RGB",
   "RBG",
   "BGR",
   "BRG",
   "GBR",
   "GRB",

   0
};

int cSeduSetup::toOrderIndex(const char* order)
{
   for (int i = 0; seduRGBOrders[i];i++)
      if (strcasecmp(seduRGBOrders[i], order) == 0)
         return i;

   return na;
}

//***************************************************************************
// Object
//***************************************************************************

cSeduSetup::cSeduSetup()
{
   data = cfg;
   data.copyLeds(&cfg);

   // sedu mode

   seduModes[smMiniDMX] 	= "miniDMX_512";
   seduModes[smTpm2] 		= "tpm2";
   seduModes[smMiniDMX_A1] 	= "miniDMX_192";
   seduModes[smMiniDMX_B0] 	= "miniDMX_768";

   // rgb order

   rgbOrderIndex = toOrderIndex(data.seduRGBOrder);

   if (rgbOrderIndex = na)
      rgbOrderIndex = 0;

   // Cinema Bars

   cineBars[0] = "Horizontal";
   cineBars[1] = "Vertical";
   cineBars[2] = "Both";

   Setup();
}

//***************************************************************************
// Setup
//***************************************************************************

void cSeduSetup::Setup() 
{
   Clear();

   Add(new cMenuEditIntItem(tr("Log level"), &data.loglevel, 0, 3));
   Add(new cMenuEditBoolItem(tr("Show mainmenu"), &data.showMainmenu));

   Add(new cMenuEditIntItem(tr("Frequence [Hz]"), &data.frequence, 1, 100));

   Add(new cMenuEditStraItem(tr("Detect cinema bars"), (int*)&data.detectCineBars, 3, cineBars));

   Add(new cMenuEditIntItem(tr("Threshold"), &data.threshold, 0, 255));
   Add(new cMenuEditIntItem(tr("Gamma (10-30)"), &data.gamma, 10, 30));
   Add(new cMenuEditIntItem(tr("Horizontal depth"), &data.xDeep, 0, 5));
   Add(new cMenuEditIntItem(tr("Vertical depth"), &data.yDeep, 0, 5));
   Add(new cMenuEditIntItem(tr("Smoothness"), (int*)&data.average, 1, 100));
   Add(new cMenuEditIntItem(tr("Blacklevel"), &data.black, 0, 255));

   Add(new cMenuEditIntItem(tr("Level red [%]"), &data.adjRed, 0, 100));
   Add(new cMenuEditIntItem(tr("Level green [%]"), &data.adjGreen, 0, 100));
   Add(new cMenuEditIntItem(tr("Level blue [%]"), &data.adjBlue, 0, 100));

   Add(new cMenuEditStraItem(tr("SEDU mode"), (int*)&data.seduMode, 4, seduModes));
   Add(new cMenuEditStraItem(tr("SEDU RGB order"), &rgbOrderIndex, 6, seduRGBOrders));
}

eOSState cSeduSetup::ProcessKey(eKeys Key) 
{
   eOSState state = cMenuSetupPage::ProcessKey(Key);

   // nothing yet ..

   return state;
}

void cSeduSetup::Store()
{
   strcpy(data.seduRGBOrder, seduRGBOrders[rgbOrderIndex]);

   cfg = data;
   cfg.copyLeds(&data);

   SetupStore("LogLevel", data.loglevel);
   SetupStore("ShowMainmenu", data.showMainmenu);
   SetupStore("ViewMode", (int)data.viewMode);

   SetupStore("DetectCineBars", data.detectCineBars);

   SetupStore("Frequence", data.frequence);
   SetupStore("Threshold", data.threshold);
   SetupStore("Gamma", data.gamma);
   SetupStore("Blacklevel", data.black);
   SetupStore("Smoothness", data.average);

   SetupStore("HorizontalDepth", data.xDeep);
   SetupStore("VerticalDepth", data.yDeep);

   SetupStore("LevelRed", data.adjRed);
   SetupStore("LevelGreen", data.adjGreen);
   SetupStore("LevelBlue", data.adjBlue);

   SetupStore("FixedColorRed", data.fixedR);
   SetupStore("FixedColorGreen", data.fixedG);
   SetupStore("FixedColorBlue", data.fixedB);
   SetupStore("EffectSpeed", data.effectSpeed);

   SetupStore("SeduMode", data.seduMode);
   SetupStore("SeduRgbOrder", data.seduRGBOrder);
}

//***************************************************************************
// VDR Internal
//***************************************************************************

VDRPLUGINCREATOR(cPluginSeduatmo);
