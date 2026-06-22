using System;
using System.Text;
using System.Windows;
using System.Windows.Input;

namespace ElainaUI
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            ScriptBox.Focus();
        }

        private void UpdateStatus()
        {
            Dispatcher.Invoke(() =>
            {
                StatusText.Text = ElainaCore.GetStatus();
            });
        }

        private void OnAttach(object sender, RoutedEventArgs e)
        {
            if (ElainaCore.ElainaAttach())
            {
                AttachBtn.IsEnabled = false;
                ExecuteBtn.IsEnabled = true;
                InjectBtn.IsEnabled = true;
            }
            UpdateStatus();
        }

        private void OnDetach(object sender, RoutedEventArgs e)
        {
            ElainaCore.ElainaDetach();
            AttachBtn.IsEnabled = true;
            ExecuteBtn.IsEnabled = false;
            InjectBtn.IsEnabled = false;
            UpdateStatus();
        }

        private void OnInject(object sender, RoutedEventArgs e)
        {
            if (ElainaCore.ElainaInject())
                ScriptBox.Text = "-- UNC API injected successfully";
            UpdateStatus();
        }

        private void OnExecute(object sender, RoutedEventArgs e)
        {
            var script = ScriptBox.Text;
            if (string.IsNullOrWhiteSpace(script))
            {
                StatusText.Text = "Nothing to execute";
                return;
            }
            ElainaCore.ElainaExecute(script);
            UpdateStatus();
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Enter && (Keyboard.Modifiers & ModifierKeys.Control) == ModifierKeys.Control)
            {
                OnExecute(null, null);
                e.Handled = true;
            }
            base.OnKeyDown(e);
        }
    }
}
