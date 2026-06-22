using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace ElainaUI
{
    static class ElainaCore
    {
        public const int IDLE = 0;
        public const int ATTACHED = 1;
        public const int INJECTED = 2;
        public const int ERROR = -1;

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
        public static extern int ElainaGetState();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int ElainaGetPid();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ElainaInject();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaStartServer(int port);

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaStopServer();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ElainaDiagnose();

        [DllImport("Elaina.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ElainaGetLastError();

        public static string Diagnose()
        {
            var ptr = ElainaDiagnose();
            return Marshal.PtrToStringAnsi(ptr) ?? "(null)";
        }

        public static string GetLastError()
        {
            var ptr = ElainaGetLastError();
            return Marshal.PtrToStringAnsi(ptr) ?? "(null)";
        }
    }
}
