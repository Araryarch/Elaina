using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;

namespace ElainaUI
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            Log("Elaina initialized");
            ScriptBox.Focus();
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
                    Foreground = color ?? new SolidColorBrush(Color.FromRgb(0xc0, 0xc0, 0xd0)),
                    FontSize = 12
                };
                p.Inlines.Add(run);
                ConsoleBox.Document.Blocks.Add(p);
                ConsoleBox.ScrollToEnd();
            });
        }

        private void LogError(string msg)
        {
            Log("✗ " + msg, new SolidColorBrush(Color.FromRgb(0xe9, 0x45, 0x60)));
        }

        private void LogOk(string msg)
        {
            Log("✓ " + msg, new SolidColorBrush(Color.FromRgb(0x00, 0xb8, 0x94)));
        }

        private void LogInfo(string msg)
        {
            Log("→ " + msg, new SolidColorBrush(Color.FromRgb(0x6c, 0x5c, 0xe7)));
        }

        private void UpdateStatus(string text, bool connected = false)
        {
            Dispatcher.Invoke(() =>
            {
                StatusText.Text = text;
                StatusDot.Fill = new SolidColorBrush(connected
                    ? Color.FromRgb(0x00, 0xb8, 0x94)
                    : Color.FromRgb(0x6c, 0x6c, 0x80));
                StatusText.Foreground = new SolidColorBrush(connected
                    ? Color.FromRgb(0x00, 0xb8, 0x94)
                    : Color.FromRgb(0x6c, 0x6c, 0x80));
            });
        }

        private void OnAttach(object sender, RoutedEventArgs e)
        {
            LogInfo("Attaching to Roblox...");
            if (ElainaCore.ElainaAttach())
            {
                AttachBtn.IsEnabled = false;
                ExecuteBtn.IsEnabled = true;
                InjectBtn.IsEnabled = true;
                PidText.Text = ElainaCore.GetStatus().Replace("Attached to PID ", "");
                LogOk("Attached successfully");
                UpdateStatus("Connected", true);
            }
            else
            {
                LogError("Failed to attach — Roblox not found");
                UpdateStatus(ElainaCore.GetStatus());
            }
        }

        private void OnDetach(object sender, RoutedEventArgs e)
        {
            ElainaCore.ElainaDetach();
            AttachBtn.IsEnabled = true;
            ExecuteBtn.IsEnabled = false;
            InjectBtn.IsEnabled = false;
            PidText.Text = "—";
            UpdateStatus("Detached");
            LogInfo("Detached from Roblox");
        }

        private void OnInject(object sender, RoutedEventArgs e)
        {
            LogInfo("Injecting UNC API...");
            if (ElainaCore.ElainaInject())
            {
                LogOk("UNC API injected");
                ScriptBox.Text = "-- UNC API ready";
            }
            else
            {
                LogError("Injection failed");
            }
            UpdateStatus(ElainaCore.GetStatus());
        }

        private void DoExecute()
        {
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
                LogError("Execution failed: " + ElainaCore.GetStatus());

            UpdateStatus(ElainaCore.GetStatus());
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
            Log("Console cleared", new SolidColorBrush(Color.FromRgb(0x6c, 0x6c, 0x80)));
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
