using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Timers;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;

namespace ElainaUI
{
    public partial class MainWindow : Window
    {
        private Timer _pollTimer;
        private int _lastState = ElainaCore.IDLE;

        private Process? _termProc;
        private StreamWriter? _termInput;
        private readonly SolidColorBrush _colError = new(Color.FromRgb(0xFF, 0x47, 0x57));
        private readonly SolidColorBrush _colOk = new(Color.FromRgb(0x00, 0xC4, 0x8C));
        private readonly SolidColorBrush _colInfo = new(Color.FromRgb(0x3B, 0x82, 0xF6));
        private readonly SolidColorBrush _colWarn = new(Color.FromRgb(0xFF, 0xB5, 0x47));
        private readonly SolidColorBrush _colDim = new(Color.FromRgb(0x66, 0x66, 0x80));
        private readonly SolidColorBrush _colWhite = new(Color.FromRgb(0xEB, 0xEB, 0xF0));

        public MainWindow()
        {
            InitializeComponent();
            Log("Elaina v1.0.0 initialized");
            ScriptBox.Focus();
            RightTabs.SelectionChanged += OnRightTabChanged;

            _pollTimer = new Timer(1500);
            _pollTimer.Elapsed += (_, _) => PollState();
            _pollTimer.Start();
        }

        private void WriteRichTextBox(RichTextBox box, string msg, Brush color)
        {
            var p = new Paragraph { Margin = new Thickness(0), LineHeight = 1.2 };
            p.Inlines.Add(new Run(msg) { Foreground = color, FontSize = 10.5 });
            Dispatcher.Invoke(() =>
            {
                box.Document.Blocks.Add(p);
                box.ScrollToEnd();
            });
        }

        private void Log(string msg, Brush? color = null)
        {
            WriteRichTextBox(ConsoleBox, msg, color ?? _colDim);
        }

        private void LogError(string msg)
        {
            Log("ERROR: " + msg, _colError);
        }

        private void LogOk(string msg)
        {
            Log("OK: " + msg, _colOk);
        }

        private void LogInfo(string msg)
        {
            Log(">> " + msg, _colInfo);
        }

        private void LogWarn(string msg)
        {
            Log("WARN: " + msg, _colWarn);
        }

        private void SetConnected(bool connected, int pid = 0)
        {
            Dispatcher.Invoke(() =>
            {
                StatusDot.Background = connected
                    ? new SolidColorBrush(Color.FromRgb(0x00, 0xC4, 0x8C))
                    : new SolidColorBrush(Color.FromRgb(0x66, 0x66, 0x80));
                StatusText.Foreground = connected
                    ? new SolidColorBrush(Color.FromRgb(0xEB, 0xEB, 0xF0))
                    : new SolidColorBrush(Color.FromRgb(0x88, 0x88, 0xA3));
                StatusText.Text = connected ? "CONNECTED" : "IDLE";
                PidText.Text = pid > 0 ? pid.ToString() : "";
            });
        }

        private void SyncUi()
        {
            var state = ElainaCore.ElainaGetState();
            var pid = ElainaCore.ElainaGetPid();

            Dispatcher.Invoke(() =>
            {
                switch (state)
                {
                    case ElainaCore.IDLE:
                        InjectBtn.IsEnabled = true;
                        InjectBtn.Content = "INJECT";
                        ExecuteBtn.IsEnabled = false;
                        DetachBtn.IsEnabled = false;
                        SetConnected(false);
                        break;

                    case ElainaCore.ATTACHED:
                        InjectBtn.IsEnabled = true;
                        InjectBtn.Content = "INJECT";
                        ExecuteBtn.IsEnabled = false;
                        DetachBtn.IsEnabled = true;
                        SetConnected(true, pid);
                        break;

                    case ElainaCore.INJECTED:
                        InjectBtn.IsEnabled = false;
                        InjectBtn.Content = "INJECTED";
                        ExecuteBtn.IsEnabled = true;
                        DetachBtn.IsEnabled = true;
                        SetConnected(true, pid);
                        break;

                    case ElainaCore.ERROR:
                        InjectBtn.IsEnabled = true;
                        InjectBtn.Content = "INJECT";
                        ExecuteBtn.IsEnabled = false;
                        DetachBtn.IsEnabled = false;
                        SetConnected(false);
                        break;
                }
            });
        }

        private void PollState()
        {
            var state = ElainaCore.ElainaGetState();
            if (state != _lastState)
            {
                _lastState = state;
                SyncUi();
            }
        }

        private void OnInject(object sender, RoutedEventArgs e)
        {
            LogInfo("Attaching to Roblox...");
            if (!ElainaCore.ElainaAttach())
            {
                LogError("Attach failed: " + ElainaCore.GetLastError());
                SyncUi();
                return;
            }

            var pid = ElainaCore.ElainaGetPid();
            LogOk($"Attached (PID {pid})");

            LogInfo("Injecting UNC API...");
            if (!ElainaCore.ElainaInject())
            {
                LogError("Injection failed: " + ElainaCore.GetLastError());
                LogWarn("Running diagnostics...");
                LogWarn(ElainaCore.Diagnose());
                ElainaCore.ElainaDetach();
                _lastState = ElainaCore.IDLE;
                SyncUi();
                return;
            }

            LogOk("Injected — ready to execute");
            _lastState = ElainaCore.INJECTED;
            SyncUi();
        }

        private void OnDetach(object sender, RoutedEventArgs e)
        {
            ElainaCore.ElainaDetach();
            _lastState = ElainaCore.IDLE;
            SyncUi();
            LogInfo("Detached");
        }

        private void DoExecute()
        {
            var state = ElainaCore.ElainaGetState();
            if (state < ElainaCore.INJECTED)
            {
                LogError("Not injected");
                return;
            }

            var script = ScriptBox.Text;
            if (string.IsNullOrWhiteSpace(script))
            {
                LogError("Nothing to execute");
                return;
            }

            LogInfo($"Executing ({script.Length} chars)...");
            if (ElainaCore.ElainaExecute(script))
                LogOk("Script executed");
            else
            {
                LogError("Execution failed: " + ElainaCore.GetLastError());
                LogWarn("Running diagnostics...");
                LogWarn(ElainaCore.Diagnose());
            }
        }

        private void OnExecute(object sender, RoutedEventArgs e) => DoExecute();

        private void OnClear(object sender, RoutedEventArgs e)
        {
            ScriptBox.Clear();
            ScriptBox.Focus();
        }

        private void OnClearRight(object sender, RoutedEventArgs e)
        {
            if (RightTabs.SelectedIndex == 0)
            {
                ConsoleBox.Document.Blocks.Clear();
                Log("Console cleared");
            }
            else
            {
                TerminalBox.Document.Blocks.Clear();
                TerminalWrite("[TERMINAL] cleared\n");
            }
        }

        private void OnRightTabChanged(object sender, SelectionChangedEventArgs e)
        {
            var isTerminal = RightTabs.SelectedIndex == 1;
            ConsolePanel.Visibility = isTerminal ? Visibility.Collapsed : Visibility.Visible;
            TerminalPanel.Visibility = isTerminal ? Visibility.Visible : Visibility.Collapsed;
            if (isTerminal && _termProc == null)
                TerminalWrite("[TERMINAL] press Enter to start cmd.exe\n");
        }

        private void OnScriptTabChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ScriptTabs.SelectedIndex == 0)
            {
                EditorPanel.Visibility = Visibility.Visible;
                ScriptsPanel.Visibility = Visibility.Collapsed;
            }
            else
            {
                EditorPanel.Visibility = Visibility.Collapsed;
                ScriptsPanel.Visibility = Visibility.Visible;
            }
        }

        private void OnLoadScript(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is string script)
            {
                ScriptBox.Text = script;
                ScriptTabs.SelectedIndex = 0;
                LogInfo($"Loaded script: {(script.Length > 40 ? script[..40] + "..." : script)}");
            }
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Enter && (Keyboard.Modifiers & ModifierKeys.Control) == ModifierKeys.Control)
            {
                DoExecute();
                e.Handled = true;
            }
            else if (e.Key == Key.Escape)
            {
                TerminalInput.Focus();
                e.Handled = true;
            }
            base.OnKeyDown(e);
        }

        // ── Terminal ──────────────────────────────────────

        private void TerminalWrite(string text)
        {
            WriteRichTextBox(TerminalBox, text, _colWhite);
        }

        private void TerminalWriteErr(string text)
        {
            WriteRichTextBox(TerminalBox, text, _colError);
        }

        private void OnTerminalKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key != Key.Enter) return;

            var line = TerminalInput.Text;
            TerminalInput.Clear();

            if (_termProc == null || _termProc.HasExited)
            {
                if (line.Trim().Length == 0) return;
                StartTerminal(line);
                return;
            }

            _termInput?.WriteLine(line);
            _termInput?.Flush();
        }

        private void StartTerminal(string? initialCmd = null)
        {
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    UseShellExecute = false,
                    RedirectStandardInput = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true,
                    StandardOutputEncoding = Encoding.UTF8,
                    StandardErrorEncoding = Encoding.UTF8,
                };

                _termProc = new Process { StartInfo = psi };
                _termProc.Start();

                _termInput = _termProc.StandardInput;

                TerminalBox.Document.Blocks.Clear();
                TerminalWrite("Microsoft Windows [Version " + Environment.OSVersion.Version + "]\n");
                TerminalWrite("(c) Microsoft Corporation. All rights reserved.\n\n");

                _termProc.OutputDataReceived += (_, args) =>
                {
                    if (args.Data != null)
                        TerminalWrite(args.Data + "\n");
                };
                _termProc.ErrorDataReceived += (_, args) =>
                {
                    if (args.Data != null)
                        TerminalWriteErr(args.Data + "\n");
                };
                _termProc.BeginOutputReadLine();
                _termProc.BeginErrorReadLine();

                _termProc.Exited += (_, _) =>
                {
                    _termProc = null;
                    _termInput = null;
                    TerminalWriteErr("[TERMINAL] process exited\n");
                };

                if (initialCmd != null && _termInput != null)
                {
                    _termInput.WriteLine(initialCmd);
                    _termInput.Flush();
                }
            }
            catch (Exception ex)
            {
                TerminalWriteErr("[TERMINAL] failed to start: " + ex.Message + "\n");
            }
        }
    }
}
