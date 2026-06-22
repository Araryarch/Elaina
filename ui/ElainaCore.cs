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
                if (!name.Contains("SyntaxAPI.dll")) continue;

                var dllPath = Path.Combine(Path.GetTempPath(), "SyntaxAPI-" + Guid.NewGuid() + ".dll");
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

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ElainaAttach();

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaDetach();

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ElainaExecute([MarshalAs(UnmanagedType.LPStr)] string script);

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int ElainaGetState();

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int ElainaGetPid();

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ElainaInject();

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaStartServer(int port);

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void ElainaStopServer();

        [DllImport("SyntaxAPI.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr ElainaDiagnose();

        public static string Diagnose()
        {
            var ptr = ElainaDiagnose();
            return Marshal.PtrToStringAnsi(ptr) ?? "(null)";
        }
    }
}
