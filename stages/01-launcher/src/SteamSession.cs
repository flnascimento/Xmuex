using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using SteamKit2;
using SteamKit2.Authentication;
using SteamKit2.CDN;

namespace ShenmueLauncher
{
    /// <summary>Progresso do download de um jogo.</summary>
    sealed class DownloadProgress
    {
        public string File;
        public long Done, Total;
    }

    /// <summary>Login Steam por QR code + download do depot pinado (somente a conta do próprio usuário). Nada é salvo em disco.</summary>
    sealed class SteamSession
    {
        // Shenmue I & II (Steam). Build 3291213, depot 758332.
        public const uint AppId = 758330;
        public const uint DepotId = 758332;
        public const ulong ManifestId = 2953351546760814442;

        private readonly SteamClient _client = new SteamClient();
        private readonly CallbackManager _mgr;
        private readonly SteamUser _user;
        private readonly SteamApps _apps;
        private readonly SteamContent _content;
        private readonly CancellationTokenSource _pump = new CancellationTokenSource();
        private TaskCompletionSource<bool> _connected = new TaskCompletionSource<bool>();
        private TaskCompletionSource<EResult> _loggedOn = new TaskCompletionSource<EResult>();

        public bool IsLoggedOn { get; private set; }
        public string AccountName { get; private set; }

        /// <summary>Disparado quando a conexão cai depois do login (a UI volta a mostrar o QR).</summary>
        public event Action Disconnected;

        public SteamSession()
        {
            _mgr = new CallbackManager(_client);
            _user = _client.GetHandler<SteamUser>();
            _apps = _client.GetHandler<SteamApps>();
            _content = _client.GetHandler<SteamContent>();
            _mgr.Subscribe<SteamClient.ConnectedCallback>(_ => _connected.TrySetResult(true));
            _mgr.Subscribe<SteamClient.DisconnectedCallback>(_ =>
            {
                bool was = IsLoggedOn;
                IsLoggedOn = false;
                _connected.TrySetResult(false);
                _loggedOn.TrySetResult(EResult.NoConnection);
                if (was) Disconnected?.Invoke();
            });
            _mgr.Subscribe<SteamUser.LoggedOnCallback>(c => _loggedOn.TrySetResult(c.Result));
            var thread = new Thread(() =>
            {
                while (!_pump.IsCancellationRequested)
                {
                    try { _mgr.RunWaitCallbacks(TimeSpan.FromMilliseconds(250)); }
                    catch (Exception) { Thread.Sleep(250); } // erro de rede não pode derrubar o launcher
                }
            }) { IsBackground = true, Name = "SteamCallbacks" };
            thread.Start();
        }

        private async Task ConnectAsync()
        {
            _connected = new TaskCompletionSource<bool>();
            _client.Connect();
            if (!await _connected.Task) throw new InvalidOperationException("Falha ao conectar à Steam.");
        }

        /// <summary>Mostra o QR (showQr é chamado de novo quando a Steam troca o código) e espera o app Steam confirmar.</summary>
        public async Task LoginQrAsync(Action<string> showQr, CancellationToken ct)
        {
            await ConnectAsync();
            var qr = await _client.Authentication.BeginAuthSessionViaQRAsync(new AuthSessionDetails { DeviceFriendlyName = "Shenmue UWP (Xbox)" });
            qr.ChallengeURLChanged = () => showQr(qr.ChallengeURL);
            showQr(qr.ChallengeURL);
            AuthPollResult result = await qr.PollingWaitForResultAsync(ct);
            _loggedOn = new TaskCompletionSource<EResult>();
            _user.LogOn(new SteamUser.LogOnDetails
            {
                Username = result.AccountName,
                AccessToken = result.RefreshToken, // só em memória; descartado ao fechar
                ShouldRememberPassword = false,
            });
            var r = await _loggedOn.Task;
            if (r != EResult.OK) throw new InvalidOperationException("Login recusado: " + r);
            AccountName = result.AccountName;
            IsLoggedOn = true;
        }

        public void Close()
        {
            _pump.Cancel();
            try { if (IsLoggedOn) _user.LogOff(); _client.Disconnect(); } catch (Exception) { }
            IsLoggedOn = false;
        }

        /// <summary>Baixa e verifica os arquivos do depot que começam com <paramref name="folder"/> (sm1 ou sm2) em <paramref name="root"/>.</summary>
        public async Task DownloadAsync(string root, string folder, IProgress<DownloadProgress> progress, CancellationToken ct)
        {
            progress.Report(new DownloadProgress { File = "Verificando licença..." });
            var keyRes = await _apps.GetDepotDecryptionKey(DepotId, AppId);
            if (keyRes.Result != EResult.OK)
                throw new InvalidOperationException("Esta conta não possui o Shenmue I & II (" + keyRes.Result + ").");
            var depotKey = keyRes.DepotKey;

            progress.Report(new DownloadProgress { File = "Obtendo servidores..." });
            var servers = (await _content.GetServersForSteamPipe())
                .Where(s => s.Type == "SteamCache" || s.Type == "CDN").Take(6).ToList();
            if (servers.Count == 0) throw new InvalidOperationException("Nenhum servidor de conteúdo disponível.");

            using (var cdn = new Client(_client))
            {
                progress.Report(new DownloadProgress { File = "Baixando lista de arquivos..." });
                var code = await _content.GetManifestRequestCode(DepotId, AppId, ManifestId, "public");
                var manifest = await cdn.DownloadManifestAsync(DepotId, ManifestId, code, servers[0], depotKey);
                if (manifest.FilenamesEncrypted) manifest.DecryptFilenames(depotKey);

                string prefix = folder.TrimEnd('\\') + "\\";
                var files = manifest.Files
                    .Where(f => (f.Flags & EDepotFileFlag.Directory) == 0)
                    .Where(f => f.FileName.Replace('/', '\\').StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                    .ToList();
                if (files.Count == 0) throw new InvalidOperationException("Nenhum arquivo de " + folder + " no manifest.");
                long total = files.Sum(f => (long)f.TotalSize), done = 0;
                var rootFull = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
                Directory.CreateDirectory(root);

                foreach (var f in files)
                {
                    ct.ThrowIfCancellationRequested();
                    var path = Path.GetFullPath(Path.Combine(root, f.FileName.Replace('/', '\\').Replace('\\', Path.DirectorySeparatorChar)));
                    if (!path.StartsWith(rootFull, StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("Caminho inválido no manifest: " + f.FileName);
                    Directory.CreateDirectory(Path.GetDirectoryName(path));

                    if (File.Exists(path) && new FileInfo(path).Length == (long)f.TotalSize && Sha1Matches(path, f.FileHash))
                    {
                        done += (long)f.TotalSize;
                        progress.Report(new DownloadProgress { File = f.FileName, Done = done, Total = total });
                        continue;
                    }

                    using (var fs = new FileStream(path, FileMode.Create, FileAccess.ReadWrite, FileShare.None, 1 << 16))
                    {
                        fs.SetLength((long)f.TotalSize);
                        var gate = new SemaphoreSlim(4);
                        var wlock = new object();
                        var tasks = f.Chunks.Select(async chunk =>
                        {
                            await gate.WaitAsync(ct);
                            try
                            {
                                var buf = await DownloadChunkAsync(cdn, servers, chunk, depotKey, ct);
                                lock (wlock) { fs.Position = (long)chunk.Offset; fs.Write(buf, 0, buf.Length); }
                                var d = Interlocked.Add(ref done, (long)buf.Length);
                                progress.Report(new DownloadProgress { File = f.FileName, Done = d, Total = total });
                            }
                            finally { gate.Release(); }
                        }).ToList();
                        await Task.WhenAll(tasks);
                    }

                    if (f.TotalSize > 0 && !Sha1Matches(path, f.FileHash))
                        throw new InvalidDataException("Hash inválido: " + f.FileName);
                }
                progress.Report(new DownloadProgress { File = "Concluído e verificado.", Done = total, Total = total });
            }
        }

        private static async Task<byte[]> DownloadChunkAsync(Client cdn, List<Server> servers, DepotManifest.ChunkData chunk, byte[] key, CancellationToken ct)
        {
            Exception last = null;
            for (int attempt = 0; attempt < 6; attempt++)
            {
                ct.ThrowIfCancellationRequested();
                try
                {
                    var dc = await cdn.DownloadDepotChunkAsync(DepotId, chunk, servers[attempt % servers.Count], key);
                    return dc.Data;
                }
                catch (Exception e) { last = e; await Task.Delay(500 * (attempt + 1), ct); }
            }
            throw new InvalidOperationException("Falha ao baixar chunk após tentativas.", last);
        }

        private static bool Sha1Matches(string path, byte[] expected)
        {
            if (expected == null || expected.Length == 0) return true;
            using (var s = File.OpenRead(path))
            using (var sha = SHA1.Create())
                return sha.ComputeHash(s).SequenceEqual(expected);
        }
    }
}
