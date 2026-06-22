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
                    Foreground = color ?? new SolidColorBrush(Color.FromRgb(0xe0, 0xe0, 0xf0)),
                    FontSize = 11.5
                };
                p.Inlines.Add(run);
                ConsoleBox.Document.Blocks.Add(p);
                ConsoleBox.ScrollToEnd();
            });
        }

        private void LogError(string msg)
        {
            Log("Error: " + msg, new SolidColorBrush(Color.FromRgb(0xff, 0x47, 0x57)));
        }

        private void LogOk(string msg)
        {
            Log("OK: " + msg, new SolidColorBrush(Color.FromRgb(0x2e, 0xd5, 0x73)));
        }

        private void LogInfo(string msg)
        {
            Log("-> " + msg, new SolidColorBrush(Color.FromRgb(0x7c, 0x5c, 0xfc)));
        }

        private void UpdateStatus(string text, bool connected = false)
        {
            Dispatcher.Invoke(() =>
            {
                StatusText.Text = text;
                StatusText.Foreground = connected
                    ? new SolidColorBrush(Color.FromRgb(0x2e, 0xd5, 0x73))
                    : new SolidColorBrush(Color.FromRgb(0x1a, 0x1a, 0x2e));
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
                LogError("Failed to attach - Roblox not found");
                UpdateStatus(ElainaCore.GetStatus());
            }
        }

        private void OnDetach(object sender, RoutedEventArgs e)
        {
            ElainaCore.ElainaDetach();
            AttachBtn.IsEnabled = true;
            ExecuteBtn.IsEnabled = false;
            InjectBtn.IsEnabled = false;
            PidText.Text = "";
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
            Log("Console cleared");
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
