using ElainaUI.Models;
using ElainaUI.Services;
using Microsoft.Win32;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;

namespace ElainaUI
{
    public partial class MainWindow : Window
    {
        private readonly PipeClient _pipeClient;
        private readonly List<ScriptFile> _scripts = new();
        private string? _currentFilePath;
        private bool _isConnected;

        public MainWindow()
        {
            InitializeComponent();

            _pipeClient = new PipeClient("ElainaExecutor");
            _pipeClient.OnConnected += (s, e) => UpdateConnectionStatus(true);
            _pipeClient.OnDisconnected += (s, e) => UpdateConnectionStatus(false);
            _pipeClient.OnResponseReceived += (s, e) => AppendToConsole(e, Brushes.Gray);

            TryConnect();
        }

        private async void TryConnect()
        {
            for (int i = 0; i < 30; i++)
            {
                if (await _pipeClient.ConnectAsync())
                {
                    UpdateConnectionStatus(true);
                    AppendToConsole("[+] Connected to executor engine", Brushes.Green);
                    return;
                }
                await Task.Delay(500);
            }
            AppendToConsole("[-] Failed to connect. Is Roblox running with Elaina injected?", Brushes.Red);
        }

        private void UpdateConnectionStatus(bool connected)
        {
            _isConnected = connected;
            Dispatcher.Invoke(() =>
            {
                StatusIndicator.Fill = connected
                    ? (SolidColorBrush)FindResource("BrushSuccess")
                    : (SolidColorBrush)FindResource("BrushError");
                ConnectionStatus.Text = connected ? "Connected" : "Disconnected";
            });
        }

        private void AppendToConsole(string text, Brush? color = null)
        {
            Dispatcher.Invoke(() =>
            {
                var paragraph = ConsoleOutput.Document.Blocks.LastBlock as Paragraph;
                if (paragraph == null)
                {
                    paragraph = new Paragraph();
                    ConsoleOutput.Document.Blocks.Add(paragraph);
                }

                var run = new Run($"[{DateTime.Now:HH:mm:ss}] {text}\n");
                if (color != null)
                    run.Foreground = color;
                paragraph.Inlines.Add(run);

                ConsoleOutput.ScrollToEnd();
            });
        }

        #region Title Bar
        private void TitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ClickCount == 2)
            {
                WindowState = WindowState == WindowState.Maximized
                    ? WindowState.Normal : WindowState.Maximized;
                return;
            }
            DragMove();
        }

        private void Minimize_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void Close_Click(object sender, RoutedEventArgs e)
        {
            _pipeClient.Disconnect();
            Application.Current.Shutdown();
        }
        #endregion

        #region Script Execution
        private async void Execute_Click(object sender, RoutedEventArgs e)
        {
            string code = ScriptEditor.Text.Trim();
            if (string.IsNullOrEmpty(code))
            {
                AppendToConsole("[!] No script to execute", Brushes.Yellow);
                return;
            }

            if (!_isConnected)
            {
                AppendToConsole("[-] Not connected to executor engine", Brushes.Red);
                return;
            }

            ExecuteBtn.IsEnabled = false;
            StatusText.Text = "Executing...";
            AppendToConsole("[>] Executing script...", Brushes.Cyan);

            bool success = await _pipeClient.ExecuteScriptAsync(code);

            StatusText.Text = success ? "Executed" : "Failed";
            ExecuteBtn.IsEnabled = true;

            if (success)
                AppendToConsole("[+] Script executed successfully", Brushes.Green);
            else
                AppendToConsole("[-] Script execution failed", Brushes.Red);
        }

        private async void ScriptEditor_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.F5 || (e.Key == Key.F6))
            {
                e.Handled = true;
                Execute_Click(sender, new RoutedEventArgs());
            }
        }
        #endregion

        #region Editor Controls
        private void Clear_Click(object sender, RoutedEventArgs e)
        {
            ScriptEditor.Clear();
            _currentFilePath = null;
            StatusText.Text = "Cleared";
        }

        private void ScriptEditor_TextChanged(object sender, TextChangedEventArgs e)
        {
            int line = ScriptEditor.GetLineIndexFromCharacterIndex(ScriptEditor.CaretIndex) + 1;
            int col = ScriptEditor.CaretIndex - ScriptEditor.GetCharacterIndexFromLineIndex(line - 1) + 1;
            LineInfo.Text = $"Ln: {line} | Col: {col} | Len: {ScriptEditor.Text.Length}";
        }

        private async void SaveScript_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new SaveFileDialog
            {
                Filter = "Lua Scripts (*.lua)|*.lua|All files (*.*)|*.*",
                DefaultExt = ".lua"
            };

            if (dialog.ShowDialog() == true)
            {
                await File.WriteAllTextAsync(dialog.FileName, ScriptEditor.Text);
                _currentFilePath = dialog.FileName;
                StatusText.Text = $"Saved: {Path.GetFileName(dialog.FileName)}";
                AppendToConsole($"[+] Saved to {dialog.FileName}", Brushes.Green);
            }
        }
        #endregion

        #region Script List
        private async void LoadScript_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "Lua Scripts (*.lua)|*.lua|All files (*.*)|*.*",
                Multiselect = false
            };

            if (dialog.ShowDialog() == true)
            {
                string content = await File.ReadAllTextAsync(dialog.FileName);
                ScriptEditor.Text = content;
                _currentFilePath = dialog.FileName;

                var script = new ScriptFile
                {
                    Name = Path.GetFileName(dialog.FileName),
                    Path = dialog.FileName
                };

                if (!_scripts.Any(s => s.Path == script.Path))
                {
                    _scripts.Add(script);
                    ScriptListBox.ItemsSource = null;
                    ScriptListBox.ItemsSource = _scripts;
                }

                StatusText.Text = $"Loaded: {script.Name}";
                AppendToConsole($"[+] Loaded {script.Name}", Brushes.Cyan);
            }
        }

        private void ScriptList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ScriptListBox.SelectedItem is ScriptFile script)
            {
                try
                {
                    ScriptEditor.Text = File.ReadAllText(script.Path);
                    _currentFilePath = script.Path;
                    StatusText.Text = $"Loaded: {script.Name}";
                }
                catch (Exception ex)
                {
                    AppendToConsole($"[-] Error loading script: {ex.Message}", Brushes.Red);
                }
            }
        }
        #endregion

        #region Console
        private void ClearConsole_Click(object sender, RoutedEventArgs e)
        {
            ConsoleOutput.Document.Blocks.Clear();
        }

        protected override async void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            await _pipeClient.DisconnectAsync();
            base.OnClosing(e);
        }
        #endregion
    }
}
