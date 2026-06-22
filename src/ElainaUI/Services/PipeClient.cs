using System;
using System.IO.Pipes;
using System.Text;

namespace ElainaUI.Services
{
    public class PipeClient : IDisposable
    {
        private readonly string _pipeName;
        private NamedPipeClientStream? _pipe;
        private bool _disposed;

        public event EventHandler? OnConnected;
        public event EventHandler? OnDisconnected;
        public event EventHandler<string>? OnResponseReceived;

        public bool IsConnected => _pipe?.IsConnected ?? false;

        public PipeClient(string pipeName)
        {
            _pipeName = pipeName;
        }

        public async Task<bool> ConnectAsync(int timeoutMs = 5000)
        {
            try
            {
                _pipe = new NamedPipeClientStream(".", _pipeName,
                    PipeDirection.InOut, PipeOptions.Asynchronous);

                await _pipe.ConnectAsync(timeoutMs);
                OnConnected?.Invoke(this, EventArgs.Empty);
                return true;
            }
            catch
            {
                return false;
            }
        }

        public async Task<bool> ExecuteScriptAsync(string script)
        {
            if (_pipe == null || !_pipe.IsConnected)
                return false;

            try
            {
                byte[] scriptBytes = Encoding.UTF8.GetBytes(script);
                uint type = 0x01; // MessageType::Execute
                uint length = (uint)scriptBytes.Length;

                // Write header
                var header = new byte[12];
                BitConverter.GetBytes(type).CopyTo(header, 0);
                BitConverter.GetBytes(length).CopyTo(header, 4);
                BitConverter.GetBytes(0u).CopyTo(header, 8); // checksum

                await _pipe.WriteAsync(header, 0, 12);
                await _pipe.WriteAsync(scriptBytes, 0, scriptBytes.Length);
                await _pipe.FlushAsync();

                // Read response
                var respHeader = new byte[5]; // [success:1][data_len:4]
                int read = _pipe.Read(respHeader, 0, 5);
                if (read < 5) return false;

                bool success = respHeader[0] == 1;
                uint dataLen = BitConverter.ToUInt32(respHeader, 1);

                if (dataLen > 0)
                {
                    var data = new byte[dataLen];
                    _pipe.Read(data, 0, (int)dataLen);
                    OnResponseReceived?.Invoke(this, Encoding.UTF8.GetString(data));
                }

                return success;
            }
            catch
            {
                OnDisconnected?.Invoke(this, EventArgs.Empty);
                return false;
            }
        }

        public void Disconnect()
        {
            if (_pipe != null)
            {
                _pipe.Close();
                _pipe.Dispose();
                _pipe = null;
                OnDisconnected?.Invoke(this, EventArgs.Empty);
            }
        }

        public async Task DisconnectAsync()
        {
            await Task.Run(() => Disconnect());
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                Disconnect();
                _disposed = true;
            }
        }
    }
}
