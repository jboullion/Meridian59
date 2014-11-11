using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using Newtonsoft.Json;
using PatchListGenerator;
using System.IO;
using System.ComponentModel;
using System.Threading;

namespace ClientPatcher
{
    #region Event Delegates and Args
    //Event when we Scan a File, used to notify UI.
    public delegate void ScanFileEventHandler(object sender, ScanEventArgs e);
    public class ScanEventArgs : EventArgs
    {
        private readonly string _filename;
        public string Filename
        {
            get
            {
                return _filename;
            }
        }

        public ScanEventArgs(string filename)
        {
            _filename = filename;
        }
    }
    //Event when we Start a Download, used to notify UI.
    public delegate void StartDownloadEventHandler(object sender, StartDownloadEventArgs e);
    public class StartDownloadEventArgs : EventArgs
    {
        private readonly long _filesize;
        public long Filesize
        {
            get
            {
                return _filesize;
            }
        }

        private readonly string _filename;
        public string Filename
        {
            get
            {
                return _filename;
            }
        }

        public StartDownloadEventArgs(string filename, long filesize)
        {
            _filename = filename;
            _filesize = filesize;
        }
    }
    //Event when we Make progress in a Download, used to notify UI.
    public delegate void ProgressDownloadEventHandler(object sender, DownloadProgressChangedEventArgs e);
    //Event when we Complete a Download, used to notify UI.
    public delegate void EndDownloadEventHandler(object sender, AsyncCompletedEventArgs e);

    #endregion

    class ClientPatcher
    {
        private string _patchInfoJason = "";

        public List<ManagedFile> PatchFiles; //Loaded from the web server at PatchInfoURL
        public List<ManagedFile> ChangedFiles; //Loaded with files that do NOT match
        public List<ManagedFile> LocalCachedFiles; //Loaded with hashes from cache.txt to prevent the need to scan each time
 
        public WebClient MyWebClient;

        bool _continueAsync;

        public PatcherSettings CurrentProfile { get; set; }

        #region Events
        //Event when we Scan a File, used to notify UI.
        public event ScanFileEventHandler FileScanned;
        protected virtual void OnFileScan(ScanEventArgs e)
        {
            if (FileScanned != null)
                FileScanned(this, e);
        }
        //Event when we Start a Download, used to notify UI.
        public event StartDownloadEventHandler StartedDownload;
        protected virtual void OnStartDownload(StartDownloadEventArgs e)
        {
            if (StartedDownload != null)
                StartedDownload(this, e);
        }
        //Event when we Make progress in a Download, used to notify UI.
        public event ProgressDownloadEventHandler ProgressedDownload;
        protected virtual void OnProgressedDownload(DownloadProgressChangedEventArgs e)
        {
            if (ProgressedDownload != null)
                ProgressedDownload(this, e);
        }
        //Event when we Complete a Download, used to notify UI.
        public event EndDownloadEventHandler EndedDownload;
        protected virtual void OnEndDownload(AsyncCompletedEventArgs e)
        {
            if (EndedDownload != null)
                EndedDownload(this, e);
        }
        #endregion

        public ClientPatcher()
        {
            ChangedFiles = new List<ManagedFile>();
            MyWebClient = new WebClient();
        }

        public ClientPatcher(PatcherSettings settings)
        {
            ChangedFiles = new List<ManagedFile>();
            MyWebClient = new WebClient();
            CurrentProfile = settings;
        }

        public int DownloadJson()
        {
            var wc = new WebClient();
            try
            {
                _patchInfoJason = wc.DownloadString(CurrentProfile.PatchInfoUrl);
                PatchFiles = JsonConvert.DeserializeObject<List<ManagedFile>>(_patchInfoJason);
                return 1;
            }
            catch (WebException e)
            {
                Console.WriteLine("WebException Handler: {0}", e);
                return 0;
            }
        }

        private void FillCacheFromFile()
        {
            using (var sr = new StreamReader(CurrentProfile.ClientFolder + "\\cache.txt"))
            {
                LocalCachedFiles = JsonConvert.DeserializeObject<List<ManagedFile>>(sr.ReadToEnd());
            }
        }

        private bool IsNewClient()
        {
            return !File.Exists(CurrentProfile.ClientFolder + "\\meridian.ini");
        }

        private bool HasCache()
        {
            return File.Exists(CurrentProfile.ClientFolder + "\\cache.txt");
        }

        private void CreateFolderStructure()
        {
            try
            {
                Directory.CreateDirectory(CurrentProfile.ClientFolder);
                Directory.CreateDirectory(CurrentProfile.ClientFolder + "\\resource\\");
                Directory.CreateDirectory(CurrentProfile.ClientFolder + "\\download\\");
                Directory.CreateDirectory(CurrentProfile.ClientFolder + "\\help\\");
                Directory.CreateDirectory(CurrentProfile.ClientFolder + "\\mail\\");
                Directory.CreateDirectory(CurrentProfile.ClientFolder + "\\ads\\");
            }
            catch (Exception e)
            {
                
                throw new IOException("Unable to CreateFolderStructure()" + e);
            }
            
        }

        private void CreateDefaultIni()
        {
            try
            {
                const string defaultIni = 
@"[Comm]
ServerNumber=103
[Miscellaneous]
UserName=username
Download=10016
";
                using (var sw = new StreamWriter(CurrentProfile.ClientFolder + "\\meridian.ini"))
                {
                    sw.Write(defaultIni);
                }
            }
            catch (Exception e)
            {

                throw new IOException("Unable to CreateDefaultIni()" + e);
            }
            
        }

        private void CreateNewClient()
        {
            CreateFolderStructure();
            CreateDefaultIni();
            //TODO: Replace this with code to download an initial .zip of the client.
        }

        public void ScanClient()
        {
            if (IsNewClient())
            {
                CreateNewClient();
            }
            if (HasCache())
            {
                FillCacheFromFile();
                CheckCachedFiles();
            }
            else
            {
                GenerateCacheFromScan();
                CheckPatchListFiles();
            }
        }

        public void GenerateCacheFromScan()
        {
            var start = DateTime.Now;
            var scanner = new ClientScanner(CurrentProfile.ClientFolder);
            scanner.ScanSource();
            LocalCachedFiles = scanner.Files;
            WriteCacheToFile();
            Debug.Print((DateTime.Now - start).ToString());
        }

        public void UpdateCacheFromPatch()
        {
            foreach (ManagedFile file in PatchFiles)
            {
                UpdateSingleFileCache(file);
            }
        }

        public void UpdateSingleFileCache(ManagedFile file)
        {
            var localFile = LocalCachedFiles.FirstOrDefault(x => x.Filepath == file.Filepath);
            if (localFile != null)
            {
                localFile.ComputeHash();
            }
            else
            {
                LocalCachedFiles.Add(file);
                file.ComputeHash();
            }
        }

        public void WriteCacheToFile()
        {
            using (var sw = new StreamWriter(CurrentProfile.ClientFolder + "\\cache.txt"))
            {
                sw.Write(JsonConvert.SerializeObject(LocalCachedFiles, Formatting.Indented));
            }
        }

        private void CheckPatchListFiles()
        {
            foreach (ManagedFile patchFile in PatchFiles)
            {
                string fullpath = CurrentProfile.ClientFolder + patchFile.Basepath + patchFile.Filename;
                var localFile = new ManagedFile(fullpath);
                localFile.ComputeHash();
                if (patchFile.MyHash != localFile.MyHash)
                {
                    localFile.Length = patchFile.Length;
                    localFile.Filepath = CurrentProfile.ClientFolder + patchFile.Basepath + patchFile.Filename;
                    ChangedFiles.Add(localFile);
                }
                //Tells the form to update the progress bar
                FileScanned(this, new ScanEventArgs(patchFile.Filename));
            }
            UpdateCacheFromPatch();
        }

        private void CheckCachedFiles()
        {
            foreach (ManagedFile patchFile in PatchFiles)
            {
                var localFile = LocalCachedFiles.FirstOrDefault(x => x.Filename == patchFile.Filename);
                if (localFile.MyHash != patchFile.MyHash)
                {
                    localFile.Length = patchFile.Length;
                    localFile.Filepath = CurrentProfile.ClientFolder + patchFile.Basepath + patchFile.Filename;
                    ChangedFiles.Add(localFile);
                }
                //Tells the form to update the progress bar
                FileScanned(this, new ScanEventArgs(patchFile.Filename));
            }
            UpdateCacheFromPatch();
        }

        public void DownloadFiles()
        {
            foreach (ManagedFile file in ChangedFiles)
            {
                string temp = file.Basepath.Replace("\\", "/");
                try
                {
                    StartedDownload(this, new StartDownloadEventArgs(file.Filename, file.Length));
                    MyWebClient.DownloadFile(CurrentProfile.PatchBaseUrl + temp + file.Filename, CurrentProfile.ClientFolder + file.Basepath + file.Filename);
                }
                catch (WebException e)
                {
                    Console.WriteLine("WebException Handler: {0}", e);
                    return;
                }
            }
        }

        public void DownloadFilesAsync()
        {
            foreach (ManagedFile file in ChangedFiles)
            {
                string temp = file.Basepath.Replace("\\", "/");
                StartedDownload(this, new StartDownloadEventArgs(file.Filename, file.Length));
                DownloadFileAsync(CurrentProfile.PatchBaseUrl + temp + file.Filename, CurrentProfile.ClientFolder + file.Basepath + file.Filename);
                while (!_continueAsync)
                {
                    //Wait for the previous file to finish
                    Thread.Sleep(10);
                }
                UpdateSingleFileCache(file);
            }
            WriteCacheToFile();
        }

        public void DownloadFileAsync(string url, string path)
        {
            using (var client = new WebClient())
            {
                try
                {
                    client.DownloadProgressChanged += client_DownloadProgressChanged;
                    client.DownloadFileCompleted += client_DownloadFileCompleted;
                    client.DownloadFileAsync(new Uri(url), path);
                }
                catch (WebException e)
                {
                    Console.WriteLine("Exception: {0}", e);
                }
                _continueAsync = false;
            }
        }

        private void client_DownloadFileCompleted(object sender, AsyncCompletedEventArgs e)
        {
            OnEndDownload(e);
            _continueAsync = true;
        }

        private void client_DownloadProgressChanged(object sender, DownloadProgressChangedEventArgs e)
        {
            OnProgressedDownload(e);
        }

    }
}
