using System;
using System.Collections.Generic;
using System.Text;
using Microsoft.Win32;

namespace MobileSRC.GScroll
{
    internal class GScrollRegistry
    {
        private static readonly int CValue = 47;
        private static readonly int Variant = 7714;

        public static readonly string SettingsPath = @"HKEY_CURRENT_USER\ControlPanel\GScroll";
        public static readonly string GScrollKey = "ControlPanel\\GScroll";
        public static readonly string Version = "Version";
        public static readonly string RegstrationCode = "RegCode";
        public static readonly string TiltMode = "Tilt";
        public static readonly string SwipeMode = "Swipe";
        public static readonly string Disabled = "Disabled";

        public static readonly string TapMinInterval = "TapMinInterval";
        public static readonly string TapMaxInterval = "TapMaxInterval";
        public static readonly string SwipeMinInterval = "SwipeMinInterval";
        public static readonly string DblTapMinInterval = "DblTapMinInterval";
        public static readonly string DblTapMaxInterval = "DblTapMaxInterval";
        public static readonly string PrecisionInterval = "PrecisionInterval";
        public static readonly string TiltStep = "TiltStep";
        public static readonly string TiltSleep = "TiltSleep";
        public static readonly string TiltThreshold = "TiltThreshold";

        public static readonly string DisableLaunchGestures = "DisableLaunchGestures";
        public static readonly string EnableScrollWheel = "EnableScrollWheel";
        public static readonly string EnableGameMode = "EnableGameMode";
        public static readonly string EnableSmoothScroll = "EnableSmoothScroll";
        public static readonly string EnablePrecisionMode = "EnablePrecisionMode";
        public static readonly string GameModeKeyRepeat = "GameModeKeyRepeat";
        public static readonly string PrecisionModeScrollSize = "PrecisionModeScrollSize";
        public static readonly string PrecisionModeNavigateSize = "PrecisionModeNavigateSize";

        public static readonly string Launch_Notify = "LaunchNotify";
        public static readonly string LaunchKey = "Launch";
        public static readonly string Launch_SequencesKey = "Sequences";

        public static readonly string PerProcessKey = "PerProcess";

        public static readonly string AppSwitchKey = "AppSwitch";
        public static readonly string AppSwitch_ProcessKey = "Processes";
        public static readonly string AppSwitch_Process_Ignore = "Ignore";
        public static readonly string AppSwitch_Process_CanClose = "CanClose";
        public static readonly string AppSwitch_Process_CanMinimize = "CanMinimize";
        public static readonly string AppSwitch_EnableShrinkAnimation = "EnableShrinkAnimation";
        public static readonly string AppSwitch_EnableScrollAnimation = "EnableScrollAnimation";
        public static readonly string AppSwitch_EnableThumbnails = "EnableThumbnails";
        public static readonly string AppSwitch_AutoCloseDelay = "AutoCloseDelay";

        public static string ReadFromRegistry(string rootKeyName, string name, string defaultValue)
        {
            using (RegistryKey rootKey = Registry.CurrentUser.CreateSubKey(rootKeyName))
            {
                return Convert.ToString(rootKey.GetValue(name, defaultValue));
            }
        }
        public static uint ReadFromRegistry(string rootKeyName, string name, uint defaultValue)
        {
            using (RegistryKey rootKey = Registry.CurrentUser.CreateSubKey(rootKeyName))
            {
                return Convert.ToUInt32(rootKey.GetValue(name, defaultValue));
            }
        }

        public static T ReadValue<T>(string valueName, T defaultValue) where T : new()
        {
            try
            {
                return (T)Convert.ChangeType(Registry.GetValue(SettingsPath, valueName, defaultValue), typeof(T), null);
            }
            catch
            {
                return defaultValue;
            }
        }

        public static void WriteString(string subKey, string valueName, string value)
        {
            string key = (string.IsNullOrEmpty(subKey)) ? SettingsPath : System.IO.Path.Combine(SettingsPath, subKey);
            Registry.SetValue(key, valueName, value, RegistryValueKind.String);
        }

        public static void WriteDword(string subKey, string valueName, int value)
        {
            string key = (string.IsNullOrEmpty(subKey)) ? SettingsPath : System.IO.Path.Combine(SettingsPath, subKey);
            Registry.SetValue(key, valueName, value, RegistryValueKind.DWord);
        }

        public static void WriteString(string valueName, string value)
        {
            Registry.SetValue(SettingsPath, valueName, value, RegistryValueKind.String);
        }

        public static void WriteDword(string valueName, int value)
        {
            Registry.SetValue(SettingsPath, valueName, value, RegistryValueKind.DWord);
        }

        public static string GetOwnerName()
        {
            return Convert.ToString(Registry.GetValue(@"HKEY_CURRENT_USER\ControlPanel\Owner", "Name", string.Empty));
        }

        public static bool CheckRegistration()
        {
            return true;
            return CheckRegistration(GetOwnerName(), Convert.ToInt32(Microsoft.Win32.Registry.GetValue(SettingsPath, RegstrationCode, 0)));
        }

        public static bool CheckRegistration(string ownerName)
        {
            return true;
            try
            {
                return CheckRegistration(ownerName, Convert.ToInt32(Microsoft.Win32.Registry.GetValue(SettingsPath, RegstrationCode, 0)));
            }
            catch
            {
                return false;
            }
        }

        public static bool CheckRegistration(int regCode)
        {
            return true;
            return CheckRegistration(GetOwnerName(), regCode);
        }

        public static bool CheckRegistration(string ownerName, int regCode)
        {
            return true;
            string customer = ownerName;
            if (customer.Length > 10)
            {
                customer = customer.Substring(0, 5) + customer.Substring(customer.Length - 5, 5);
            }

            int value = 0;
            for (int i = 0; i < customer.Length; ++i)
            {
                value += (int)customer[i];
            }

            value *= CValue;
            value += Variant;

            return (value == regCode);
        }

        public static void SetRegistration(int regCode)
        {
            Microsoft.Win32.Registry.SetValue(SettingsPath, RegstrationCode, regCode, Microsoft.Win32.RegistryValueKind.DWord);
        }
    }
}
