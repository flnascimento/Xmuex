using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading;
using System.Threading.Tasks;
using QRCoder;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;

namespace ShenmueLauncher
{
    public sealed partial class MainPage : Page
    {
        // Pasta, executável (tamanho exato da build 3291213) e espaço aproximado de cada jogo.
        private static readonly string[] Folder = { null, "sm1", "sm2" };
        private static readonly string[] ExeName = { null, "Shenmue.exe", "Shenmue2.exe" };
        private static readonly long[] ExeSize = { 0, 11651072, 10890752 };
        private static readonly long[] NeededBytes = { 0, 5_200_000_000, 9_500_000_000 };
        private const string CompleteMarker = ".shenmue_ok";

        private readonly CancellationTokenSource _cts = new CancellationTokenSource();
        private SteamSession _steam;
        private bool _loginLoop;
        private int _downloading; // 0 = nenhum

        public MainPage()
        {
            InitializeComponent();
            Loaded += (s, e) =>
            {
                InitMods();
                RefreshGames();
                ShowLastRun();
                StartLoginLoop();
                Window.Current.Activated += (w, a) =>
                {
                    if (a.WindowActivationState == CoreWindowActivationState.Deactivated) return;
                    RefreshGames();
                    ShowLastRun();
                };
                (IsInstalled(1) ? Play1 : Download1).Focus(FocusState.Programmatic);
            };
        }

        private static string LocalPath => ApplicationData.Current.LocalFolder.Path;
        private static string GameRoot => Path.Combine(LocalPath, "game");
        private static string GameDir(int g) => Path.Combine(GameRoot, Folder[g]);

        private Task UiAsync(Action a) => Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => a()).AsTask();

        private void SetStatus(string s) => Status.Text = s;

        // ---------------------------------------------------------------- estado dos jogos
        private static bool IsInstalled(int g)
        {
            try
            {
                if (File.Exists(Path.Combine(GameDir(g), CompleteMarker))) return true;
                var exe = new FileInfo(Path.Combine(GameDir(g), ExeName[g]));
                return exe.Exists && exe.Length == ExeSize[g] && Directory.Exists(Path.Combine(GameDir(g), "archives"));
            }
            catch (Exception) { return false; }
        }

        private static long FolderBytes(string dir)
        {
            try { return Directory.Exists(dir) ? new DirectoryInfo(dir).EnumerateFiles("*", SearchOption.AllDirectories).Sum(f => f.Length) : 0; }
            catch (Exception) { return 0; }
        }

        private static string Gb(long bytes) => (bytes / 1e9).ToString("0.0") + " GB";

        private async void RefreshGames()
        {
            bool logged = _steam != null && _steam.IsLoggedOn;
            for (int g = 1; g <= 2; g++)
            {
                bool installed = IsInstalled(g), busy = _downloading == g, anyBusy = _downloading != 0;
                bool partial = !installed && Directory.Exists(GameDir(g));
                Button play = g == 1 ? Play1 : Play2, dl = g == 1 ? Download1 : Download2, rep = g == 1 ? Repair1 : Repair2, del = g == 1 ? Delete1 : Delete2;
                TextBlock state = g == 1 ? State1 : State2;

                play.Visibility = installed && !busy ? Visibility.Visible : Visibility.Collapsed;
                dl.Visibility = !installed || busy ? Visibility.Visible : Visibility.Collapsed;
                dl.IsEnabled = logged && !anyBusy;
                dl.Content = busy ? "Baixando..." : partial ? "Continuar download" : "Baixar (~" + Gb(NeededBytes[g]) + ")";
                rep.Visibility = installed && !busy ? Visibility.Visible : Visibility.Collapsed;
                rep.IsEnabled = logged && !anyBusy;
                del.Visibility = (installed || partial) && !busy ? Visibility.Visible : Visibility.Collapsed;
                del.IsEnabled = !anyBusy;
                state.Text = busy ? "Baixando" : installed ? "Instalado" : partial ? "Download incompleto" : logged ? "Não instalado" : "Não instalado · entre pelo QR para baixar";
            }
            for (int g = 1; g <= 2; g++)
            {
                if (_downloading == g) continue;
                int game = g;
                bool installed = IsInstalled(game);
                if (!installed && !Directory.Exists(GameDir(game))) continue;
                long size = await Task.Run(() => FolderBytes(GameDir(game)));
                if (_downloading == game) continue;
                (game == 1 ? State1 : State2).Text = (installed ? "Instalado · " : "Download incompleto · ") + Gb(size);
            }
            long free = await FreeBytesAsync();
            if (_downloading == 0) SetStatus(free < 0 ? "Pronto." : "Espaço livre no console: " + Gb(free));
        }

        private static async Task<long> FreeBytesAsync()
        {
            try
            {
                var p = await ApplicationData.Current.LocalFolder.Properties.RetrievePropertiesAsync(new[] { "System.FreeSpace" });
                return p.TryGetValue("System.FreeSpace", out var v) && v is ulong u ? (long)u : -1;
            }
            catch (Exception) { return -1; }
        }

        // ---------------------------------------------------------------- login (só QR)
        private async void StartLoginLoop()
        {
            if (_loginLoop) return;
            _loginLoop = true;
            while (!_cts.IsCancellationRequested)
            {
                try
                {
                    _steam?.Close();
                    _steam = new SteamSession();
                    _steam.Disconnected += () => _ = UiAsync(OnSteamDisconnected);
                    SetQrOverlay("...", "Conectando à Steam");
                    LoginStatus.Text = "Conectando à Steam...";
                    await _steam.LoginQrAsync(url => _ = UiAsync(async () => await ShowQrAsync(url)), _cts.Token);
                    SetQrOverlay("✓", "Conectado como " + _steam.AccountName);
                    LoginStatus.Text = "Conectado como " + _steam.AccountName + ". Já pode baixar ou verificar os jogos.";
                    break;
                }
                catch (Exception ex)
                {
                    SetQrOverlay("!", "Gerando outro QR...");
                    LoginStatus.Text = "Não foi possível entrar: " + ex.Message;
                    try { await Task.Delay(4000, _cts.Token); } catch (Exception) { break; }
                }
            }
            _loginLoop = false;
            RefreshGames();
        }

        private void OnSteamDisconnected()
        {
            LoginStatus.Text = "A conexão com a Steam caiu. Escaneie o QR de novo.";
            RefreshGames();
            StartLoginLoop();
        }

        private void SetQrOverlay(string icon, string text)
        {
            QrOverlay.Visibility = Visibility.Visible;
            QrOverlayIcon.Text = icon;
            QrOverlayText.Text = text;
        }

        private async Task ShowQrAsync(string url)
        {
            var png = new PngByteQRCode(new QRCodeGenerator().CreateQrCode(url, QRCodeGenerator.ECCLevel.Q)).GetGraphic(10);
            var bmp = new BitmapImage();
            using (var ms = new InMemoryRandomAccessStream())
            {
                await ms.WriteAsync(png.AsBuffer());
                ms.Seek(0);
                await bmp.SetSourceAsync(ms);
            }
            QrImage.Source = bmp;
            QrOverlay.Visibility = Visibility.Collapsed;
            LoginStatus.Text = "Escaneie o QR com o app Steam e confirme no celular.";
        }

        // ---------------------------------------------------------------- jogar
        private async void Play_Click(object sender, RoutedEventArgs e)
        {
            int game = int.Parse((string)((Button)sender).Tag);
            try
            {
                File.WriteAllText(Path.Combine(LocalPath, "lastgame.txt"), game.ToString());
                File.Delete(Path.Combine(LocalPath, "boot.log"));
                File.Delete(Path.Combine(LocalPath, "host.log"));
            }
            catch (Exception) { }
            bool ok = await Windows.System.Launcher.LaunchUriAsync(new Uri("shenmue-uwp-run:" + game));
            if (!ok) SetStatus("Não foi possível abrir o jogo.");
        }

        // ---------------------------------------------------------------- baixar / verificar
        private async void Download_Click(object sender, RoutedEventArgs e)
        {
            int game = int.Parse((string)((Button)sender).Tag);
            if (_steam == null || !_steam.IsLoggedOn) { SetStatus("Entre pelo QR code para baixar."); return; }
            long have = FolderBytes(GameDir(game)), free = await FreeBytesAsync();
            if (free >= 0 && free + have < NeededBytes[game])
            {
                SetStatus("Espaço insuficiente: " + Gb(free) + " livres; precisa de ~" + Gb(NeededBytes[game] - have) + ".");
                return;
            }
            _downloading = game;
            RefreshGames();
            var panel = game == 1 ? Progress1 : Progress2;
            var bar = game == 1 ? Bar1 : Bar2;
            var text = game == 1 ? ProgressText1 : ProgressText2;
            panel.Visibility = Visibility.Visible;
            var sw = Stopwatch.StartNew();
            long first = -1;
            var progress = new Progress<DownloadProgress>(p =>
            {
                if (p.Total <= 0) { text.Text = p.File; return; }
                if (first < 0) first = p.Done; // o que já estava baixado não entra na velocidade
                double frac = (double)p.Done / p.Total, secs = Math.Max(sw.Elapsed.TotalSeconds, 1), rate = (p.Done - first) / secs;
                bar.Value = frac;
                string eta = rate > 100_000 && p.Done < p.Total ? " · restam ~" + TimeSpan.FromSeconds((p.Total - p.Done) / rate).ToString(@"h\:mm\:ss") : "";
                text.Text = (frac * 100).ToString("0.0") + "% · " + Gb(p.Done) + " de " + Gb(p.Total) + " · " + (rate / 1e6).ToString("0.0") + " MB/s" + eta;
                SetStatus("Baixando " + (game == 1 ? "Shenmue" : "Shenmue II") + ": " + p.File);
            });
            try
            {
                var steam = _steam;
                await Task.Run(() => steam.DownloadAsync(GameRoot, Folder[game], progress, _cts.Token));
                File.WriteAllText(Path.Combine(GameDir(game), CompleteMarker), SteamSession.ManifestId.ToString());
                SetStatus((game == 1 ? "Shenmue" : "Shenmue II") + ": concluído e verificado.");
                panel.Visibility = Visibility.Collapsed;
            }
            catch (Exception ex)
            {
                text.Text = "Parou: " + ex.Message + " (aperte Continuar para retomar)";
                SetStatus("Download interrompido.");
            }
            _downloading = 0;
            RefreshGames();
        }

        // ---------------------------------------------------------------- apagar
        private async void Delete_Click(object sender, RoutedEventArgs e)
        {
            int game = int.Parse((string)((Button)sender).Tag);
            string name = game == 1 ? "Shenmue" : "Shenmue II";
            var dlg = new ContentDialog
            {
                Title = "Apagar " + name + "?",
                Content = "Os arquivos de " + name + " serão removidos deste console. Os saves e as opções do jogo ficam guardados. Você pode baixar de novo depois.",
                PrimaryButtonText = "Apagar",
                CloseButtonText = "Cancelar",
                DefaultButton = ContentDialogButton.Close,
            };
            if (await dlg.ShowAsync() != ContentDialogResult.Primary) return;
            SetStatus("Apagando " + name + "...");
            try
            {
                string dir = GameDir(game);
                await Task.Run(() => { if (Directory.Exists(dir)) Directory.Delete(dir, true); });
                SetStatus(name + " apagado.");
            }
            catch (Exception ex) { SetStatus("Não foi possível apagar tudo: " + ex.Message); }
            RefreshGames();
        }

        // ---------------------------------------------------------------- mods embutidos (opções lidas pelo runtime em mods.ini)
        private static string ModsIni => Path.Combine(LocalPath, "mods.ini");
        private static string PackageMods => Path.Combine(Windows.ApplicationModel.Package.Current.InstalledLocation.Path, "mods");
        private bool _modsLoaded;

        private void InitMods()
        {
            var ini = new Dictionary<string, bool> { ["widescreen_sm1"] = true, ["widescreen_sm2"] = true, ["legendas_ptbr_sm1"] = true, ["sm2_sem_bordas"] = false, ["sm2_sem_icone_camera"] = false, ["texturas_sm1"] = false };
            try
            {
                if (File.Exists(ModsIni))
                    foreach (var line in File.ReadAllLines(ModsIni))
                    {
                        var kv = line.Split('=');
                        if (kv.Length == 2 && ini.ContainsKey(kv[0].Trim())) ini[kv[0].Trim()] = kv[1].Trim() == "1";
                    }
            }
            catch (Exception) { }
            bool ws1 = File.Exists(Path.Combine(PackageMods, "patches", "widescreen_sm1.txt"));
            bool ws2 = File.Exists(Path.Combine(PackageMods, "patches", "widescreen_sm2.txt"));
            bool pt1 = Directory.Exists(Path.Combine(PackageMods, "overlay", "sm1"));
            bool tex1 = Directory.Exists(Path.Combine(PackageMods, "data", "sm1")) && Directory.Exists(Path.Combine(PackageMods, "overlay_tex", "sm1"));
            Ws1.Visibility = ws1 ? Visibility.Visible : Visibility.Collapsed;
            Ptbr1.Visibility = pt1 ? Visibility.Visible : Visibility.Collapsed;
            Tex1.Visibility = tex1 ? Visibility.Visible : Visibility.Collapsed;
            Ws2.Visibility = Borders2.Visibility = Camera2.Visibility = ws2 ? Visibility.Visible : Visibility.Collapsed;
            Credits.Visibility = ws1 || ws2 || pt1 || tex1 ? Visibility.Visible : Visibility.Collapsed;
            Ws1.IsChecked = ini["widescreen_sm1"];
            Ptbr1.IsChecked = ini["legendas_ptbr_sm1"];
            Tex1.IsChecked = ini["texturas_sm1"];
            Ws2.IsChecked = ini["widescreen_sm2"];
            Borders2.IsChecked = ini["sm2_sem_bordas"];
            Camera2.IsChecked = ini["sm2_sem_icone_camera"];
            Borders2.IsEnabled = Ws2.IsChecked == true;
            _modsLoaded = true;
            SaveMods();
        }

        private void Mod_Click(object sender, RoutedEventArgs e)
        {
            Borders2.IsEnabled = Ws2.IsChecked == true;
            SaveMods();
        }

        private void SaveMods()
        {
            if (!_modsLoaded) return;
            string B(CheckBox c) => c.IsChecked == true ? "1" : "0";
            try
            {
                File.WriteAllLines(ModsIni, new[]
                {
                    "widescreen_sm1=" + B(Ws1), "legendas_ptbr_sm1=" + B(Ptbr1), "texturas_sm1=" + B(Tex1), "widescreen_sm2=" + B(Ws2),
                    "sm2_sem_bordas=" + B(Borders2), "sm2_sem_icone_camera=" + B(Camera2),
                });
            }
            catch (Exception ex) { SetStatus("Não foi possível salvar as opções: " + ex.Message); }
        }

        // ---------------------------------------------------------------- diagnóstico
        // Depois de JOGAR, mostra o que aconteceu, sem precisar abrir o log.
        private void ShowLastRun()
        {
            if (!File.Exists(Path.Combine(LocalPath, "lastgame.txt"))) { LastRun.Text = "Nenhum jogo aberto ainda."; return; }
            string host = ReadShared(Path.Combine(LocalPath, "host.log")), boot = ReadShared(Path.Combine(LocalPath, "boot.log"));
            if (boot == null && host == null) { LastRun.Text = "Última execução: o jogo não chegou a iniciar (o programa não carregou)."; return; }
            if (host == null) { LastRun.Text = "Última execução: parou antes do jogo carregar. " + boot.Replace("\r\n", " · "); return; }
            var lines = host.Split('\n').Select(l => l.Trim()).Where(l => l.Length > 0).ToList();
            var picked = lines.Where(l => l.Contains("ERRO") || l.Contains("EXCECAO") || l.Contains("TRAP") || l.Contains("falhou") || l.Contains("exit(")).ToList();
            if (picked.Count > 5) picked = picked.GetRange(picked.Count - 5, 5);
            string head = host.Contains("primeiro Present") ? "Última execução: o jogo abriu normalmente." : "Última execução: o jogo NÃO chegou a desenhar na tela.";
            LastRun.Text = head + (picked.Count > 0 ? "\n" + string.Join("\n", picked) : "");
        }

        private static string ReadShared(string path)
        {
            try
            {
                if (!File.Exists(path)) return null;
                using (var f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                using (var r = new StreamReader(f)) return r.ReadToEnd();
            }
            catch (Exception) { return null; }
        }

        private async void About_Click(object sender, RoutedEventArgs e)
        {
            const string pt =
                "Este é um projeto pessoal, de fã, feito para rodar no meu próprio Xbox a minha própria cópia de Shenmue I & II, comprada legalmente na Steam. Não há nenhuma pretensão de pirataria ou uso fraudulento.\n\n" +
                "O jogo é baixado exclusivamente pela sua conta Steam, por login oficial (QR code), sem nenhum bypass dessa verificação. Nenhum arquivo do jogo, da SEGA ou do Shenmue Production Committee está incluído neste software.\n\n" +
                "A única intenção é compartilhar, de graça, com outros fãs que também já compraram o jogo, uma forma de jogá-lo com a melhor qualidade possível no Xbox com Dev Mode.\n\n" +
                "Compromisso: se a SEGA, o Shenmue Production Committee ou a Valve entenderem que este projeto infringe algum direito, mesmo que de forma não intencional, peço que entrem em contato — ele será removido ou tornado privado imediatamente, sem discussão.\n\n" +
                "Sou só um fã que quer jogar Shenmue no Xbox. E estender isso a outros que, assim como eu, têm o jogo original e querem fazer o mesmo. Nada além disso.";
            const string en =
                "This is a personal fan project, built to run my own legally purchased Steam copy of Shenmue I & II on my own Xbox. There is no intent of piracy or fraudulent use whatsoever.\n\n" +
                "The game is downloaded exclusively through your own Steam account, via official login (QR code), with no bypass of that verification. No SEGA / Shenmue Production Committee game file is included in this software.\n\n" +
                "The sole intent is to share, for free, with other fans who also already own the game, a way to play it at the best possible quality on an Xbox with Dev Mode.\n\n" +
                "Commitment: if SEGA, the Shenmue Production Committee, or Valve believe this project infringes any right, even unintentionally, please get in touch — it will be taken down or made private immediately, no questions asked.\n\n" +
                "I'm just a fan who wants to play Shenmue on Xbox. And to extend that to others who, like me, own the original game and want to do the same. Nothing more than that.";
            var body = new StackPanel { Spacing = 16 };
            body.Children.Add(new TextBlock { Text = pt, TextWrapping = TextWrapping.Wrap, FontSize = 13, IsTextSelectionEnabled = true });
            body.Children.Add(new Windows.UI.Xaml.Shapes.Rectangle { Height = 1, Fill = (Brush)Resources["Muted"] });
            body.Children.Add(new TextBlock { Text = en, TextWrapping = TextWrapping.Wrap, FontSize = 13, IsTextSelectionEnabled = true });
            var dlg = new ContentDialog { Title = "Sobre este projeto / About this project", Content = new ScrollViewer { Content = body, MaxHeight = 420, Width = 560 }, CloseButtonText = "Fechar / Close" };
            await dlg.ShowAsync();
        }

        private async void Log_Click(object sender, RoutedEventArgs e)
        {
            string log = ReadShared(Path.Combine(LocalPath, "host.log"));
            string text = "Ainda não há log: o jogo não foi aberto.";
            if (log != null)
            {
                var lines = log.Split('\n');
                int first = Math.Max(0, lines.Length - 60);
                text = string.Join("\n", lines, first, lines.Length - first);
            }
            var body = new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap, FontSize = 12, IsTextSelectionEnabled = true };
            var dlg = new ContentDialog { Title = "Log do jogo (últimas 60 linhas)", Content = new ScrollViewer { Content = body, MaxHeight = 420 }, CloseButtonText = "Fechar" };
            await dlg.ShowAsync();
        }
    }
}
