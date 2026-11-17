using System;
using System.IO;
using System.Threading.Tasks;
using Windows.ApplicationModel.Activation;
using Windows.Storage;
using Windows.UI.ViewManagement;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

namespace ShenmueLauncher
{
    sealed partial class App : Application
    {
        public App()
        {
            InitializeComponent();
            UnhandledException += (s, e) => { LogError(e.Exception); e.Handled = true; };
            AppDomain.CurrentDomain.UnhandledException += (s, e) => LogError(e.ExceptionObject as Exception);
            TaskScheduler.UnobservedTaskException += (s, e) => { LogError(e.Exception); e.SetObserved(); };
        }

        private static void LogError(Exception ex)
        {
            try { File.AppendAllText(Path.Combine(ApplicationData.Current.LocalFolder.Path, "launcher.log"), DateTime.Now + " " + ex + "\r\n"); } catch (Exception) { }
        }

        protected override void OnLaunched(LaunchActivatedEventArgs e)
        {
            ApplicationView.GetForCurrentView().SetDesiredBoundsMode(ApplicationViewBoundsMode.UseCoreWindow);
            if (!(Window.Current.Content is Frame)) Window.Current.Content = new Frame();
            ((Frame)Window.Current.Content).Navigate(typeof(MainPage));
            Window.Current.Activate();
        }
    }
}
