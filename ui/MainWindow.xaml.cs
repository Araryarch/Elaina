using System;
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

        public MainWindow()
        {
            InitializeComponent();
            Log("Elaina initialized");
            ScriptBox.Focus();

            _pollTimer = new Timer(1500);
            _pollTimer.Elapsed += (_, _) => PollState();
            _pollTimer.Start();
        }

        private void Log(string msg, Brush? color = null)
        {
            Dispatcher.Invoke(() =>
            {
                var p = new Paragraph();
                p.Margin = new Thickness(0);
                p.LineHeight = 1.2;
                var run = new Run(msg)
                {
                    Foreground = color ?? new SolidColorBrush(Color.FromRgb(0x88, 0x88, 0x88)),
                    FontSize = 10.5
                };
                p.Inlines.Add(run);
                ConsoleBox.Document.Blocks.Add(p);
                ConsoleBox.ScrollToEnd();
            });
        }

        private void LogError(string msg)
        {
            Log("Error: " + msg, new SolidColorBrush(Color.FromRgb(0x88, 0x88, 0x88)));
        }

        private void LogOk(string msg)
        {
            Log("OK: " + msg, new SolidColorBrush(Color.FromRgb(0xaa, 0xaa, 0xaa)));
        }

        private void LogInfo(string msg)
        {
            Log("-> " + msg, new SolidColorBrush(Color.FromRgb(0x66, 0x66, 0x66)));
        }

        private void SetConnected(bool connected, int pid = 0)
        {
            Dispatcher.Invoke(() =>
            {
                StatusDot.Background = connected
                    ? new SolidColorBrush(Color.FromRgb(0x88, 0x88, 0x88))
                    : new SolidColorBrush(Color.FromRgb(0x55, 0x55, 0x55));
                StatusText.Foreground = connected
                    ? new SolidColorBrush(Color.FromRgb(0xaa, 0xaa, 0xaa))
                    : new SolidColorBrush(Color.FromRgb(0x77, 0x77, 0x77));
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
                LogError("Roblox not running");
                SyncUi();
                return;
            }

            var pid = ElainaCore.ElainaGetPid();
            LogOk($"Attached (PID {pid})");

            LogInfo("Injecting UNC API...");
            if (!ElainaCore.ElainaInject())
            {
                LogError("Injection failed");
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
                LogError("Execution failed");
        }

        private void OnExecute(object sender, RoutedEventArgs e) => DoExecute();

        private void OnClear(object sender, RoutedEventArgs e)
        {
            ScriptBox.Clear();
            ScriptBox.Focus();
        }

        private void OnClearConsole(object sender, RoutedEventArgs e)
        {
            ConsoleBox.Document.Blocks.Clear();
            Log("Console cleared");
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
            base.OnKeyDown(e);
        }
    }
}
