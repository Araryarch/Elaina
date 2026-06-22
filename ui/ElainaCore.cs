using System;
using System.Runtime.InteropServices;

namespace ElainaUI
{
    internal static class ElainaCore
    {
        private const string DllName = "Elaina.dll";

        [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
        public static extern bool ElainaAttach();

        [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaDetach();

        [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
        public static extern bool ElainaExecute([MarshalAs(UnmanagedType.LPStr)] string script);

        [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ElainaGetStatus();

        [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
        public static extern bool ElainaInject();

        [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaStartServer(int port);

        [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaStopServer();

        public static string GetStatus()
        {
            var ptr = ElainaGetStatus();
            return Marshal.PtrToStringAnsi(ptr) ?? "Unknown";
        }
    }
}
