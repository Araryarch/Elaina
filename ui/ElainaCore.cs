using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace ElainaUI
{
    static class ElainaCore
    {
        static ElainaCore()
        {
            var assembly = typeof(ElainaCore).Assembly;
            var names = assembly.GetManifestResourceNames();
            foreach (var name in names)
            {
                if (!name.Contains("Elaina.dll")) continue;

                var dllPath = Path.Combine(Path.GetTempPath(), "Elaina-" + Guid.NewGuid() + ".dll");
                using (var stream = assembly.GetManifestResourceStream(name))
                {
                    if (stream == null) break;
                    using var fs = new FileStream(dllPath, FileMode.Create, FileAccess.Write);
                    stream.CopyTo(fs);
                }
                NativeLibrary.Load(dllPath);
                break;
            }
        }

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ElainaAttach();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaDetach();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ElainaExecute([MarshalAs(UnmanagedType.LPStr)] string script);

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ElainaGetStatus();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ElainaInject();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaStartServer(int port);

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaStopServer();

        public static string GetStatus()
        {
            return Marshal.PtrToStringAnsi(ElainaGetStatus()) ?? "";
        }
    }
}
