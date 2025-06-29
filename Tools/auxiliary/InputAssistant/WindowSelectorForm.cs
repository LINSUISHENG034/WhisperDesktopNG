using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace InputAssistant
{
    public partial class WindowSelectorForm : Form
    {
        public IntPtr SelectedWindowHandle { get; private set; } = IntPtr.Zero;
        public string SelectedWindowTitle { get; private set; } = string.Empty;

        private ListBox windowListBox = null!;
        private Button selectButton = null!;
        private Button cancelButton = null!;
        private Button refreshButton = null!;
        private List<WindowInfo> windows = new List<WindowInfo>();

        [DllImport("user32.dll")]
        private static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);

        [DllImport("user32.dll")]
        private static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

        [DllImport("user32.dll")]
        private static extern int GetWindowTextLength(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        [DllImport("kernel32.dll")]
        private static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, uint dwProcessId);

        [DllImport("kernel32.dll")]
        private static extern bool CloseHandle(IntPtr hObject);

        [DllImport("psapi.dll")]
        private static extern uint GetModuleBaseName(IntPtr hProcess, IntPtr hModule, StringBuilder lpBaseName, uint nSize);

        [DllImport("user32.dll")]
        private static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        [DllImport("user32.dll")]
        private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

        [DllImport("user32.dll")]
        private static extern bool FlashWindow(IntPtr hWnd, bool bInvert);

        [DllImport("user32.dll")]
        private static extern bool FlashWindowEx(ref FLASHWINFO pwfi);

        [StructLayout(LayoutKind.Sequential)]
        private struct FLASHWINFO
        {
            public uint cbSize;
            public IntPtr hwnd;
            public uint dwFlags;
            public uint uCount;
            public uint dwTimeout;
        }

        private const int SW_SHOW = 5;
        private const int SW_RESTORE = 9;
        private static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);
        private static readonly IntPtr HWND_NOTOPMOST = new IntPtr(-2);
        private const uint SWP_NOMOVE = 0x0002;
        private const uint SWP_NOSIZE = 0x0001;
        private const uint SWP_SHOWWINDOW = 0x0040;
        private const uint FLASHW_STOP = 0;
        private const uint FLASHW_CAPTION = 1;
        private const uint FLASHW_TRAY = 2;
        private const uint FLASHW_ALL = 3;
        private const uint FLASHW_TIMER = 4;

        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        private struct WindowInfo
        {
            public IntPtr Handle;
            public string Title;
            public string ProcessName;

            public override string ToString()
            {
                return $"{Title} ({ProcessName})";
            }
        }

        public WindowSelectorForm()
        {
            InitializeComponent();
            InitializeForm();
        }

        protected override void SetVisibleCore(bool value)
        {
            base.SetVisibleCore(value);
            if (value)
            {
                ForceToFront();
            }
        }

        private void ForceToFront()
        {
            try
            {
                // 强制窗口显示在最前面
                this.WindowState = FormWindowState.Normal;
                this.Show();
                this.BringToFront();
                this.Activate();
                this.Focus();

                // 使用Windows API确保窗口在最前面
                SetWindowPos(this.Handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                SetForegroundWindow(this.Handle);

                // 额外的激活尝试
                ShowWindow(this.Handle, SW_RESTORE);
                ShowWindow(this.Handle, SW_SHOW);

                // 闪烁窗口吸引注意
                FlashWindowToAttention();
            }
            catch
            {
                // 如果API调用失败，至少确保窗口可见
                this.Show();
                this.BringToFront();
            }
        }

        private void FlashWindowToAttention()
        {
            try
            {
                FLASHWINFO fInfo = new FLASHWINFO();
                fInfo.cbSize = Convert.ToUInt32(Marshal.SizeOf(fInfo));
                fInfo.hwnd = this.Handle;
                fInfo.dwFlags = FLASHW_ALL | FLASHW_TIMER;
                fInfo.uCount = 3; // 闪烁3次
                fInfo.dwTimeout = 0;

                FlashWindowEx(ref fInfo);
            }
            catch
            {
                // 如果FlashWindowEx失败，使用简单的FlashWindow
                try
                {
                    FlashWindow(this.Handle, true);
                }
                catch
                {
                    // 忽略闪烁错误
                }
            }
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            // 窗口显示后再次确保在前面
            ForceToFront();

            // 立即刷新窗口列表
            RefreshWindowList();

            // 调试信息
            System.Diagnostics.Debug.WriteLine($"WindowSelector OnShown: Handle={this.Handle}, Visible={this.Visible}, TopMost={this.TopMost}");
        }

        protected override void OnLoad(EventArgs e)
        {
            base.OnLoad(e);
            // 加载时确保窗口在前面
            ForceToFront();

            // 调试信息
            System.Diagnostics.Debug.WriteLine($"WindowSelector OnLoad: Handle={this.Handle}");
        }

        protected override void OnActivated(EventArgs e)
        {
            base.OnActivated(e);
            System.Diagnostics.Debug.WriteLine("WindowSelector Activated");
        }

        private void InitializeForm()
        {
            this.Text = "选择目标窗口";
            this.Size = new Size(500, 400);
            this.StartPosition = FormStartPosition.CenterParent;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.TopMost = true; // 确保窗口在最前面
            this.ShowInTaskbar = true; // 在任务栏显示
            this.BringToFront(); // 强制置前

            CreateControls();
        }

        private void CreateControls()
        {
            // Instructions label
            var instructionLabel = new Label
            {
                Text = "请选择要接收文本输入的目标窗口:",
                Location = new Point(10, 10),
                Size = new Size(350, 20)
            };
            this.Controls.Add(instructionLabel);

            // Window count label
            var countLabel = new Label
            {
                Name = "countLabel",
                Text = "扫描中...",
                Location = new Point(370, 10),
                Size = new Size(100, 20),
                ForeColor = Color.Gray
            };
            this.Controls.Add(countLabel);

            // Window list
            windowListBox = new ListBox
            {
                Location = new Point(10, 35),
                Size = new Size(460, 280),
                DisplayMember = "ToString"
            };
            windowListBox.SelectedIndexChanged += WindowListBox_SelectedIndexChanged;
            windowListBox.DoubleClick += (s, e) => SelectWindow();
            this.Controls.Add(windowListBox);

            // Buttons
            refreshButton = new Button
            {
                Text = "刷新列表",
                Location = new Point(10, 325),
                Size = new Size(100, 30)
            };
            refreshButton.Click += RefreshButton_Click;
            this.Controls.Add(refreshButton);

            selectButton = new Button
            {
                Text = "选择",
                Location = new Point(290, 325),
                Size = new Size(80, 30),
                Enabled = false
            };
            selectButton.Click += (s, e) => SelectWindow();
            this.Controls.Add(selectButton);

            cancelButton = new Button
            {
                Text = "取消",
                Location = new Point(380, 325),
                Size = new Size(80, 30)
            };
            cancelButton.Click += (s, e) => this.DialogResult = DialogResult.Cancel;
            this.Controls.Add(cancelButton);
        }

        private void RefreshButton_Click(object? sender, EventArgs e)
        {
            try
            {
                refreshButton.Enabled = false;
                refreshButton.Text = "刷新中...";
                Application.DoEvents();

                RefreshWindowList();
            }
            finally
            {
                refreshButton.Enabled = true;
                refreshButton.Text = "刷新列表";
            }
        }

        private void WindowListBox_SelectedIndexChanged(object? sender, EventArgs e)
        {
            bool hasValidSelection = windowListBox.SelectedIndex >= 0 &&
                                   windowListBox.SelectedIndex < windows.Count &&
                                   windows.Count > 0;
            selectButton.Enabled = hasValidSelection;
        }

        private void SelectWindow()
        {
            try
            {
                if (windowListBox.SelectedIndex >= 0 && windowListBox.SelectedIndex < windows.Count)
                {
                    var selectedWindow = windows[windowListBox.SelectedIndex];

                    // 验证窗口仍然有效
                    if (IsWindow(selectedWindow.Handle))
                    {
                        SelectedWindowHandle = selectedWindow.Handle;
                        SelectedWindowTitle = selectedWindow.Title;
                        this.DialogResult = DialogResult.OK;
                    }
                    else
                    {
                        MessageBox.Show("选择的窗口已不存在，请刷新列表后重新选择。", "窗口无效",
                            MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        RefreshWindowList();
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"选择窗口时发生错误: {ex.Message}", "错误",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        [DllImport("user32.dll")]
        private static extern bool IsWindow(IntPtr hWnd);

        private async void RefreshWindowList()
        {
            try
            {
                System.Diagnostics.Debug.WriteLine("=== RefreshWindowList 开始 ===");

                // 禁用刷新按钮，防止重复点击
                refreshButton.Enabled = false;
                refreshButton.Text = "Scanning...";

                windows.Clear();
                windowListBox.Items.Clear();

                // 添加状态提示
                windowListBox.Items.Add("Scanning windows...");

                // 更新计数标签
                var countLabel = this.Controls.Find("countLabel", false).FirstOrDefault() as Label;
                if (countLabel != null)
                {
                    countLabel.Text = "Scanning...";
                    countLabel.ForeColor = Color.Orange;
                }

                Application.DoEvents(); // 强制更新UI

                System.Diagnostics.Debug.WriteLine("开始后台枚举窗口...");

                // 在后台线程中枚举窗口，避免阻塞UI
                var foundWindows = await Task.Run(() => {
                    var tempWindows = new List<WindowInfo>();
                    try
                    {
                        bool enumResult = EnumWindows((hWnd, lParam) => {
                            try
                            {
                                if (!IsWindowVisible(hWnd))
                                    return true;

                                int length = GetWindowTextLength(hWnd);
                                if (length == 0)
                                    return true;

                                var title = new StringBuilder(length + 1);
                                int actualLength = GetWindowText(hWnd, title, title.Capacity);

                                if (actualLength == 0)
                                    return true;

                                string windowTitle = title.ToString();
                                if (string.IsNullOrEmpty(windowTitle))
                                    return true;

                                // 过滤掉不需要的窗口
                                if (ShouldExcludeWindow(hWnd, windowTitle))
                                    return true;

                                // Get process name
                                string processName = GetProcessName(hWnd);

                                tempWindows.Add(new WindowInfo
                                {
                                    Handle = hWnd,
                                    Title = windowTitle,
                                    ProcessName = processName
                                });

                                return true;
                            }
                            catch (Exception ex)
                            {
                                System.Diagnostics.Debug.WriteLine($"枚举回调错误: {ex.Message}");
                                return true; // 继续枚举
                            }
                        }, IntPtr.Zero);

                        System.Diagnostics.Debug.WriteLine($"后台枚举完成，结果: {enumResult}, 找到窗口数: {tempWindows.Count}");
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.Debug.WriteLine($"后台枚举异常: {ex}");
                    }

                    return tempWindows;
                });

                // 回到UI线程更新界面
                windows.AddRange(foundWindows);

                System.Diagnostics.Debug.WriteLine($"枚举窗口完成，找到窗口数: {windows.Count}");

                // 清除状态提示
                windowListBox.Items.Clear();

                // 更新窗口计数
                if (countLabel != null)
                {
                    countLabel.Text = $"Found {windows.Count} windows";
                    countLabel.ForeColor = windows.Count > 0 ? Color.DarkGreen : Color.Red;
                    System.Diagnostics.Debug.WriteLine($"更新计数标签: {windows.Count}");
                }

                if (windows.Count == 0)
                {
                    windowListBox.Items.Add("No available windows - Click Refresh to retry");
                    System.Diagnostics.Debug.WriteLine("警告: 未找到任何可用窗口");
                }
                else
                {
                    // 按窗口标题排序
                    windows.Sort((w1, w2) => string.Compare(w1.Title, w2.Title, StringComparison.OrdinalIgnoreCase));

                    System.Diagnostics.Debug.WriteLine($"开始添加 {windows.Count} 个窗口到列表...");

                    foreach (var window in windows)
                    {
                        windowListBox.Items.Add(window);
                    }

                    System.Diagnostics.Debug.WriteLine($"成功添加所有窗口到列表");
                }

                System.Diagnostics.Debug.WriteLine("=== RefreshWindowList 完成 ===");
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"RefreshWindowList 严重错误: {ex}");

                try
                {
                    windowListBox.Items.Clear();
                    windowListBox.Items.Add($"Error: {ex.Message}");

                    var countLabel = this.Controls.Find("countLabel", false).FirstOrDefault() as Label;
                    if (countLabel != null)
                    {
                        countLabel.Text = "Error occurred";
                        countLabel.ForeColor = Color.Red;
                    }
                }
                catch (Exception ex2)
                {
                    System.Diagnostics.Debug.WriteLine($"UI更新也失败: {ex2}");
                }

                MessageBox.Show($"Window list refresh error: {ex.Message}\n\nFull error: {ex}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                // 恢复刷新按钮
                refreshButton.Enabled = true;
                refreshButton.Text = "Refresh List";
            }
        }



        private bool ShouldExcludeWindow(IntPtr hWnd, string windowTitle)
        {
            // 排除空标题或很短的标题
            if (string.IsNullOrWhiteSpace(windowTitle) || windowTitle.Length < 2)
                return true;

            // 排除系统窗口
            if (windowTitle == "Program Manager" ||
                windowTitle == "Desktop" ||
                windowTitle.StartsWith("Microsoft Text Input Application") ||
                windowTitle.StartsWith("Windows Input Experience"))
                return true;

            // 排除本应用程序的窗口
            if (windowTitle.Contains("Input Assistant") ||
                windowTitle == "选择目标窗口" ||
                windowTitle.Contains("InputAssistant") ||
                windowTitle.Contains("WindowDebugger"))
                return true;

            // 排除当前窗口选择器自身
            if (hWnd == this.Handle)
                return true;

            // 排除父窗口（主窗口）
            if (this.Owner != null && hWnd == this.Owner.Handle)
                return true;

            // 排除一些常见的系统进程窗口
            string processName = GetProcessName(hWnd);
            if (IsSystemProcess(processName))
                return true;

            return false;
        }

        private bool IsSystemProcess(string processName)
        {
            if (string.IsNullOrEmpty(processName))
                return true;

            string[] systemProcesses = {
                "dwm.exe",
                "winlogon.exe",
                "csrss.exe",
                "wininit.exe",
                "services.exe",
                "lsass.exe",
                "smss.exe",
                "taskmgr.exe",
                "inputassistant.exe",
                "windowdebugger.exe"
            };

            string lowerProcessName = processName.ToLower();
            foreach (string sysProcess in systemProcesses)
            {
                if (lowerProcessName == sysProcess)
                {
                    // explorer.exe 不排除
                    if (sysProcess == "explorer.exe")
                        return false;
                    return true;
                }
            }

            return false;
        }

        private string GetProcessName(IntPtr hWnd)
        {
            try
            {
                uint processId;
                GetWindowThreadProcessId(hWnd, out processId);

                if (processId == 0)
                    return "Unknown";

                IntPtr hProcess = OpenProcess(0x0400 | 0x0010, false, processId); // PROCESS_QUERY_INFORMATION | PROCESS_VM_READ

                if (hProcess != IntPtr.Zero)
                {
                    try
                    {
                        var processName = new StringBuilder(1024);
                        uint result = GetModuleBaseName(hProcess, IntPtr.Zero, processName, (uint)processName.Capacity);

                        if (result > 0)
                        {
                            string name = processName.ToString();
                            return string.IsNullOrEmpty(name) ? "Unknown" : name;
                        }
                    }
                    finally
                    {
                        CloseHandle(hProcess);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"GetProcessName 错误: {ex.Message}");
            }

            return "Unknown";
        }
    }
}
