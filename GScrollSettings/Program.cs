using System;
using System.IO;
using System.Reflection;
using System.Collections.Generic;
using System.Windows.Forms;

namespace MobileSRC.GScroll
{
    static class Program
    {
        public static Documentation Documentation;
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [MTAThread]
        static void Main()
        {
            string workingDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().ManifestModule.FullyQualifiedName);
            System.Xml.Serialization.XmlSerializer xs = new System.Xml.Serialization.XmlSerializer(typeof(Documentation));
            using (StringReader fs = new StringReader(Properties.Resources.documentation))
            {
                Program.Documentation = (Documentation)xs.Deserialize(fs);
            }
            Cursor.Current = Cursors.WaitCursor;
            UpgradeSettings();
            GScrollSettings settings = new GScrollSettings();
            if (!GScrollRegistry.CheckRegistration())
            {
                using (Register register = new Register())
                {
                    register.ShowDialog();
                }
            }
            Cursor.Current = Cursors.WaitCursor;
            Application.Run(settings);
        }
/*

static DWORD GSCROLL_TAP_MININTERVAL = 30;
static DWORD GSCROLL_TAP_MAXINTERVAL = 400;
static DWORD GSCROLL_SWIPE_MININTERVAL = 15;
static DWORD GSCROLL_DOUBLETAP_MAXINTERVAL = 1100;
static DWORD GSCROLL_DOUBLETAP_MININTERVAL = 400;
static DWORD GSCROLL_PRECISON_INTERVAL = 200;
static DWORD GSCROLL_TILT_STEP = 200;
static DWORD GSCROLL_TILT_SLEEP_INTERVAL = 400;
static DWORD GSCROLL_TILT_THRESHOLD = 200;
*/
        static void SetAppSwitchProcessOptions(string processName, int canClose, int canMinimize, int ignore)
        {
            GScrollRegistry.WriteDword(string.Format(@"{0}\{1}\{2}", GScrollRegistry.AppSwitchKey, GScrollRegistry.AppSwitch_ProcessKey, processName), GScrollRegistry.AppSwitch_Process_CanClose, canClose);
            GScrollRegistry.WriteDword(string.Format(@"{0}\{1}\{2}", GScrollRegistry.AppSwitchKey, GScrollRegistry.AppSwitch_ProcessKey, processName), GScrollRegistry.AppSwitch_Process_CanMinimize, canMinimize);
            GScrollRegistry.WriteDword(string.Format(@"{0}\{1}\{2}", GScrollRegistry.AppSwitchKey, GScrollRegistry.AppSwitch_ProcessKey, processName), GScrollRegistry.AppSwitch_Process_Ignore, ignore);
        }

        static void WriteGeneralSettings(string process, int disable, int enableGame, int enablePrecision, int enableScroll, int enableSmooth, int gameModeRepeat, int precisionNav, int precisionScroll, int swipe, int tilt)
        {
            if (!string.IsNullOrEmpty(process))
            {
                process = System.IO.Path.Combine(GScrollRegistry.PerProcessKey, process);
            }
            GScrollRegistry.WriteDword(process, GScrollRegistry.Disabled, disable);
            GScrollRegistry.WriteDword(process, GScrollRegistry.EnableGameMode, enableGame);
            GScrollRegistry.WriteDword(process, GScrollRegistry.EnablePrecisionMode, enablePrecision);
            GScrollRegistry.WriteDword(process, GScrollRegistry.EnableScrollWheel, enableScroll);
            GScrollRegistry.WriteDword(process, GScrollRegistry.EnableSmoothScroll, enableSmooth);
            GScrollRegistry.WriteDword(process, GScrollRegistry.GameModeKeyRepeat, gameModeRepeat);
            GScrollRegistry.WriteDword(process, GScrollRegistry.PrecisionModeNavigateSize, precisionNav);
            GScrollRegistry.WriteDword(process, GScrollRegistry.PrecisionModeScrollSize, precisionScroll);
            GScrollRegistry.WriteDword(process, GScrollRegistry.SwipeMode, swipe);
            GScrollRegistry.WriteDword(process, GScrollRegistry.TiltMode, tilt);
        }

        static void UpgradeSettings()
        {
            double version = GScrollRegistry.ReadValue<double>(GScrollRegistry.Version, 1.9);
            int enableGameMode = GScrollRegistry.ReadValue<int>(GScrollRegistry.EnableGameMode, -1);

            if (version < 2.0 && enableGameMode == -1)
            {
                // write all the default settings
                GScrollRegistry.WriteDword(GScrollRegistry.AppSwitchKey, GScrollRegistry.AppSwitch_AutoCloseDelay, 0);
                GScrollRegistry.WriteDword(GScrollRegistry.AppSwitchKey, GScrollRegistry.AppSwitch_EnableScrollAnimation, 1);
                GScrollRegistry.WriteDword(GScrollRegistry.AppSwitchKey, GScrollRegistry.AppSwitch_EnableShrinkAnimation, 0);
                GScrollRegistry.WriteDword(GScrollRegistry.AppSwitchKey, GScrollRegistry.AppSwitch_EnableThumbnails, 1);

                SetAppSwitchProcessOptions("cprog.exe", 0, 1, 0);
                SetAppSwitchProcessOptions("manila.exe", 0, 0, 0);
                SetAppSwitchProcessOptions("repllog.exe", 0, 1, 1);
                SetAppSwitchProcessOptions("services.exe", 0, 1, 0);
                SetAppSwitchProcessOptions("shell32.exe", 0, 1, 0);

                GScrollRegistry.WriteDword(GScrollRegistry.LaunchKey, GScrollRegistry.Launch_Notify, 1);

                WriteGeneralSettings("", 0, 0, 0, 0, 0, 500, 20, 10, 5, 3);
                WriteGeneralSettings("Opera9.exe", 0, 0, 0, 1, 1, 500, 20, 10, 5, 3);
                WriteGeneralSettings("services.exe", 1, 0, 0, 0, 0, 500, 20, 10, 5, 3);

                GScrollRegistry.WriteString(string.Format("{0}\\{1}", GScrollRegistry.LaunchKey, GScrollRegistry.Launch_SequencesKey), "00", @"\Windows\Start Menu\Programs\Internet Explorer.lnk");
                GScrollRegistry.WriteString(string.Format("{0}\\{1}", GScrollRegistry.LaunchKey, GScrollRegistry.Launch_SequencesKey), "02", @"::appswitchnext");
                GScrollRegistry.WriteString(string.Format("{0}\\{1}", GScrollRegistry.LaunchKey, GScrollRegistry.Launch_SequencesKey), "11", @"\Windows\Start Menu\Programs\Messaging.lnk");
                GScrollRegistry.WriteString(string.Format("{0}\\{1}", GScrollRegistry.LaunchKey, GScrollRegistry.Launch_SequencesKey), "20", @"::appswitchprev");
                GScrollRegistry.WriteString(string.Format("{0}\\{1}", GScrollRegistry.LaunchKey, GScrollRegistry.Launch_SequencesKey), "22", @"::close");
                GScrollRegistry.WriteString(string.Format("{0}\\{1}", GScrollRegistry.LaunchKey, GScrollRegistry.Launch_SequencesKey), "33", @"::lock");
                GScrollRegistry.WriteString(string.Format("{0}\\{1}", GScrollRegistry.LaunchKey, GScrollRegistry.Launch_SequencesKey), "44", @"::taptilt");
            }
            if (version < 2.1)
            {
                // we are upgrading from 2.0 to 2.1
                GScrollRegistry.WriteDword(GScrollRegistry.TapMaxInterval, 400);
                GScrollRegistry.WriteDword(GScrollRegistry.TapMinInterval, 30);
                GScrollRegistry.WriteDword(GScrollRegistry.SwipeMinInterval, 15);
                GScrollRegistry.WriteDword(GScrollRegistry.DblTapMaxInterval, 1100);
                GScrollRegistry.WriteDword(GScrollRegistry.DblTapMinInterval, 400);
                GScrollRegistry.WriteDword(GScrollRegistry.PrecisionInterval, 200);
                GScrollRegistry.WriteDword(GScrollRegistry.TiltStep, 200);
                GScrollRegistry.WriteDword(GScrollRegistry.TiltSleep, 400);
                GScrollRegistry.WriteDword(GScrollRegistry.TiltThreshold, 200);
            }

            GScrollRegistry.WriteString(GScrollRegistry.Version, "2.1");
        }
    }
}