using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace InputAssistant
{
    public partial class MainForm : Form
    {
        private TextBox textInput = null!;
        private NumericUpDown delayInput = null!;
        private Button startButton = null!;
        private Button stopButton = null!;
        private Label statusLabel = null!;
        private ProgressBar progressBar = null!;
        private Button pauseResumeButton = null!;
        private Button selectWindowButton = null!;
        private Label targetWindowLabel = null!;
        private TextBox debugConsole = null!;
        private Button clearDebugButton = null!;
        private Button toggleDebugButton = null!;
        private bool debugVisible = false;
        private CancellationTokenSource? cancellationTokenSource;
        private bool isRunning = false;
        private bool isPaused = false;
        private IntPtr targetWindowHandle = IntPtr.Zero;
        private string targetWindowTitle = string.Empty;
        private bool isWaitingForWindowSelection = false;
        private System.Windows.Forms.Timer windowCheckTimer = null!;

        // Windows API declarations for keyboard input simulation and window management
        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

        [DllImport("user32.dll")]
        private static extern IntPtr GetForegroundWindow();

        [DllImport("user32.dll")]
        private static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder text, int count);

        [DllImport("user32.dll")]
        private static extern bool IsWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern IntPtr SetFocus(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        [DllImport("user32.dll")]
        private static extern bool AttachThreadInput(uint idAttach, uint idAttachTo, bool fAttach);

        [DllImport("kernel32.dll")]
        private static extern uint GetCurrentThreadId();



        [StructLayout(LayoutKind.Sequential)]
        private struct INPUT
        {
            public uint type;
            public InputUnion u;
        }

        [StructLayout(LayoutKind.Explicit)]
        private struct InputUnion
        {
            [FieldOffset(0)]
            public MOUSEINPUT mi;
            [FieldOffset(0)]
            public KEYBDINPUT ki;
            [FieldOffset(0)]
            public HARDWAREINPUT hi;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct KEYBDINPUT
        {
            public ushort wVk;
            public ushort wScan;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MOUSEINPUT
        {
            public int dx;
            public int dy;
            public uint mouseData;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct HARDWAREINPUT
        {
            public uint uMsg;
            public ushort wParamL;
            public ushort wParamH;
        }

        private const uint INPUT_KEYBOARD = 1;
        private const uint KEYEVENTF_UNICODE = 0x0004;

        public MainForm()
        {
            InitializeComponent();
            InitializeForm();
        }

        private void InitializeForm()
        {
            this.Text = "Input Assistant v2.0 - 智能窗口版";
            this.Size = new Size(520, 450); // 默认大小，不显示调试控制台
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.TopMost = true; // 保持窗口置顶，方便随时操作

            // 初始化定时器
            windowCheckTimer = new System.Windows.Forms.Timer();
            windowCheckTimer.Interval = 500; // 每500ms检查一次
            windowCheckTimer.Tick += WindowCheckTimer_Tick;

            // Create controls
            CreateControls();
            UpdateButtonStates();
        }

        private void CreateControls()
        {
            // Text input area
            var textLabel = new Label
            {
                Text = "Text to input:",
                Location = new Point(10, 10),
                Size = new Size(100, 20)
            };
            this.Controls.Add(textLabel);

            textInput = new TextBox
            {
                Location = new Point(10, 35),
                Size = new Size(480, 120),
                Multiline = true,
                ScrollBars = ScrollBars.Vertical,
                AcceptsReturn = true,
                AcceptsTab = true
            };
            textInput.TextChanged += (s, e) => UpdateButtonStates();
            this.Controls.Add(textInput);

            // Target window selection
            var windowLabel = new Label
            {
                Text = "目标窗口:",
                Location = new Point(10, 165),
                Size = new Size(80, 20)
            };
            this.Controls.Add(windowLabel);

            selectWindowButton = new Button
            {
                Text = "🎯 拾取窗口",
                Location = new Point(100, 163),
                Size = new Size(100, 25)
            };
            selectWindowButton.Click += SelectWindowButton_Click;
            this.Controls.Add(selectWindowButton);

            targetWindowLabel = new Label
            {
                Text = "未选择窗口",
                Location = new Point(210, 165),
                Size = new Size(280, 20),
                ForeColor = Color.Gray,
                BorderStyle = BorderStyle.FixedSingle
            };
            this.Controls.Add(targetWindowLabel);

            // Delay setting
            var delayLabel = new Label
            {
                Text = "字符间延迟 (毫秒):",
                Location = new Point(10, 195),
                Size = new Size(120, 20)
            };
            this.Controls.Add(delayLabel);

            delayInput = new NumericUpDown
            {
                Location = new Point(140, 193),
                Size = new Size(80, 20),
                Minimum = 1,
                Maximum = 5000,
                Value = 50,
                Increment = 10
            };
            this.Controls.Add(delayInput);

            // Control buttons
            startButton = new Button
            {
                Text = "开始输入",
                Location = new Point(10, 230),
                Size = new Size(100, 30)
            };
            startButton.Click += StartButton_Click;
            this.Controls.Add(startButton);

            stopButton = new Button
            {
                Text = "停止",
                Location = new Point(120, 230),
                Size = new Size(80, 30),
                Enabled = false
            };
            stopButton.Click += StopButton_Click;
            this.Controls.Add(stopButton);

            // 暂停/恢复按钮
            pauseResumeButton = new Button
            {
                Text = "暂停",
                Location = new Point(210, 230),
                Size = new Size(80, 30),
                Enabled = false
            };
            pauseResumeButton.Click += PauseResumeButton_Click;
            this.Controls.Add(pauseResumeButton);

            // 最小化按钮
            var minimizeButton = new Button
            {
                Text = "最小化",
                Location = new Point(300, 230),
                Size = new Size(80, 30)
            };
            minimizeButton.Click += (s, e) => this.WindowState = FormWindowState.Minimized;
            this.Controls.Add(minimizeButton);

            // 刷新窗口按钮
            var refreshButton = new Button
            {
                Text = "刷新窗口",
                Location = new Point(390, 230),
                Size = new Size(80, 30)
            };
            refreshButton.Click += (s, e) => RefreshTargetWindow();
            this.Controls.Add(refreshButton);

            // 测试按钮（用于调试窗口选择器显示问题）
            var testButton = new Button
            {
                Text = "测试选择器",
                Location = new Point(390, 195),
                Size = new Size(80, 25),
                BackColor = Color.LightYellow
            };
            testButton.Click += TestWindowSelector_Click;
            this.Controls.Add(testButton);

            // Progress bar
            progressBar = new ProgressBar
            {
                Location = new Point(10, 270),
                Size = new Size(480, 20),
                Visible = false
            };
            this.Controls.Add(progressBar);

            // Status label
            statusLabel = new Label
            {
                Text = "就绪 - 请先选择目标窗口，然后输入文本并点击开始。",
                Location = new Point(10, 300),
                Size = new Size(480, 60),
                ForeColor = Color.DarkGreen
            };
            this.Controls.Add(statusLabel);

            // 调试控制台切换按钮
            toggleDebugButton = new Button
            {
                Text = "显示调试信息",
                Location = new Point(10, 370),
                Size = new Size(120, 30),
                UseVisualStyleBackColor = true
            };
            toggleDebugButton.Click += ToggleDebugButton_Click;
            this.Controls.Add(toggleDebugButton);

            // 清空调试控制台按钮 (默认隐藏)
            clearDebugButton = new Button
            {
                Text = "清空",
                Location = new Point(420, 368),
                Size = new Size(60, 25),
                Visible = false
            };
            clearDebugButton.Click += (s, e) => {
                debugConsole.Clear();
                LogDebug("调试控制台已清空");
            };
            this.Controls.Add(clearDebugButton);

            // 调试控制台 (默认隐藏)
            debugConsole = new TextBox
            {
                Location = new Point(10, 410),
                Size = new Size(480, 150),
                Multiline = true,
                ScrollBars = ScrollBars.Vertical,
                ReadOnly = true,
                BackColor = Color.Black,
                ForeColor = Color.LightGreen,
                Font = new Font("Consolas", 9),
                Text = "=== 调试控制台 ===\r\n程序启动完成\r\n",
                Visible = false
            };
            this.Controls.Add(debugConsole);
        }

        private void ToggleDebugButton_Click(object? sender, EventArgs e)
        {
            debugVisible = !debugVisible;

            if (debugVisible)
            {
                // 显示调试控制台
                debugConsole.Visible = true;
                clearDebugButton.Visible = true;
                toggleDebugButton.Text = "隐藏调试信息";
                this.Size = new Size(520, 600); // 扩大窗口
                this.Text = "Input Assistant v2.0 - 智能窗口版 (调试模式)";
            }
            else
            {
                // 隐藏调试控制台
                debugConsole.Visible = false;
                clearDebugButton.Visible = false;
                toggleDebugButton.Text = "显示调试信息";
                this.Size = new Size(520, 450); // 缩小窗口
                this.Text = "Input Assistant v2.0 - 智能窗口版";
            }
        }

        private void SelectWindowButton_Click(object? sender, EventArgs e)
        {
            try
            {
                StartSimpleWindowPicker();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"启动窗口选择器时发生错误: {ex.Message}", "错误",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }



        public void LogDebug(string message)
        {
            if (debugConsole != null)
            {
                string timestamp = DateTime.Now.ToString("HH:mm:ss.fff");
                string logEntry = $"[{timestamp}] {message}\r\n";

                // 在UI线程中更新
                if (debugConsole.InvokeRequired)
                {
                    debugConsole.Invoke(new Action(() => {
                        debugConsole.AppendText(logEntry);
                        debugConsole.SelectionStart = debugConsole.Text.Length;
                        debugConsole.ScrollToCaret();
                    }));
                }
                else
                {
                    debugConsole.AppendText(logEntry);
                    debugConsole.SelectionStart = debugConsole.Text.Length;
                    debugConsole.ScrollToCaret();
                }
            }
        }

        private void StartSimpleWindowPicker()
        {
            if (isWaitingForWindowSelection)
            {
                // 如果已经在等待选择，取消等待
                isWaitingForWindowSelection = false;
                windowCheckTimer.Stop();
                selectWindowButton.Text = "拾取窗口";
                statusLabel.Text = "窗口选择已取消";
                statusLabel.ForeColor = Color.Red;
                LogDebug("窗口选择已取消");
                return;
            }

            LogDebug("开始简化窗口拾取过程...");
            LogDebug("请激活您想要输入的目标窗口");

            isWaitingForWindowSelection = true;
            selectWindowButton.Text = "取消选择";
            statusLabel.Text = "请激活您想要输入的目标窗口...";
            statusLabel.ForeColor = Color.Blue;

            // 开始定时检查前台窗口
            windowCheckTimer.Start();
        }

        private void WindowCheckTimer_Tick(object? sender, EventArgs e)
        {
            if (!isWaitingForWindowSelection)
                return;

            IntPtr foregroundWindow = GetForegroundWindow();

            // 忽略自己的窗口
            if (foregroundWindow == this.Handle)
                return;

            // 检查是否是有效窗口
            if (foregroundWindow != IntPtr.Zero && IsWindow(foregroundWindow))
            {
                // 获取窗口标题
                var title = new System.Text.StringBuilder(256);
                GetWindowText(foregroundWindow, title, title.Capacity);
                string windowTitle = title.ToString();

                if (string.IsNullOrEmpty(windowTitle))
                    windowTitle = "无标题窗口";

                // 选择这个窗口
                targetWindowHandle = foregroundWindow;
                targetWindowTitle = windowTitle;
                targetWindowLabel.Text = windowTitle;
                targetWindowLabel.ForeColor = Color.DarkBlue;

                // 停止等待
                isWaitingForWindowSelection = false;
                windowCheckTimer.Stop();
                selectWindowButton.Text = "拾取窗口";

                statusLabel.Text = $"目标窗口已选择: {windowTitle}";
                statusLabel.ForeColor = Color.DarkGreen;

                LogDebug($"窗口选择完成: {windowTitle} (句柄: 0x{foregroundWindow:X8})");
            }
        }





        private void TestWindowSelector_Click(object? sender, EventArgs e)
        {
            try
            {
                MessageBox.Show("即将打开窗口选择器测试。\n如果看不到选择器窗口，请检查任务栏或按Alt+Tab查找。",
                    "测试提示", MessageBoxButtons.OK, MessageBoxIcon.Information);

                var selector = new WindowSelectorForm();
                selector.Show(); // 使用非模态显示进行测试

                MessageBox.Show("窗口选择器已打开（非模态）。\n请查看是否能看到选择器窗口。",
                    "测试结果", MessageBoxButtons.OK, MessageBoxIcon.Information);

                selector.Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"测试失败: {ex.Message}", "错误",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void StartButton_Click(object? sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(textInput.Text))
            {
                MessageBox.Show("请输入要自动输入的文本。", "无文本", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (targetWindowHandle == IntPtr.Zero)
            {
                MessageBox.Show("请先选择目标窗口。", "未选择窗口", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            // 检查目标窗口是否仍然有效
            if (!IsWindow(targetWindowHandle))
            {
                MessageBox.Show("目标窗口已关闭，请重新选择。", "窗口无效", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                targetWindowHandle = IntPtr.Zero;
                targetWindowLabel.Text = "未选择窗口";
                targetWindowLabel.ForeColor = Color.Gray;
                UpdateButtonStates();
                return;
            }

            var result = MessageBox.Show(
                $"将开始向指定窗口输入文本:\n\n" +
                $"目标窗口: {targetWindowTitle}\n" +
                $"字符数量: {textInput.Text.Length}\n" +
                $"字符延迟: {delayInput.Value}毫秒\n\n" +
                $"输入过程中您可以自由切换窗口。\n\n" +
                $"确认开始?",
                "确认输入",
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Question);

            if (result != DialogResult.Yes)
                return;

            await StartInputProcess();
        }

        private void StopButton_Click(object? sender, EventArgs e)
        {
            if (cancellationTokenSource != null && !cancellationTokenSource.Token.IsCancellationRequested)
            {
                LogDebug("用户点击停止按钮，取消输入过程");
                cancellationTokenSource.Cancel();
                statusLabel.Text = "正在停止输入过程...";
                statusLabel.ForeColor = Color.Red;
            }
        }

        private void PauseResumeButton_Click(object? sender, EventArgs e)
        {
            isPaused = !isPaused;
            pauseResumeButton.Text = isPaused ? "恢复" : "暂停";

            if (isPaused)
            {
                statusLabel.Text = "输入已暂停 - 点击恢复继续";
                statusLabel.ForeColor = Color.Orange;
                LogDebug("输入过程已暂停");
            }
            else
            {
                statusLabel.Text = "输入已恢复，继续处理...";
                statusLabel.ForeColor = Color.Blue;
                LogDebug("输入过程已恢复");
            }
        }

        private async Task StartInputProcess()
        {
            try
            {
                LogDebug("开始输入过程");
                isRunning = true;
                isPaused = false;
                UpdateButtonStates();
                cancellationTokenSource = new CancellationTokenSource();

                // Start input process immediately
                string text = textInput.Text;
                int delay = (int)delayInput.Value;

                progressBar.Minimum = 0;
                progressBar.Maximum = text.Length;
                progressBar.Value = 0;
                progressBar.Visible = true;

                statusLabel.Text = "正在输入文本... 可自由切换窗口";
                statusLabel.ForeColor = Color.Blue;

                for (int i = 0; i < text.Length; i++)
                {
                    if (cancellationTokenSource.Token.IsCancellationRequested)
                        break;

                    // 检查暂停状态
                    while (isPaused && !cancellationTokenSource.Token.IsCancellationRequested)
                    {
                        try
                        {
                            await Task.Delay(100, cancellationTokenSource.Token);
                        }
                        catch (OperationCanceledException)
                        {
                            // 在暂停期间被取消，直接退出
                            LogDebug("在暂停期间收到取消请求");
                            break;
                        }
                    }

                    if (cancellationTokenSource.Token.IsCancellationRequested)
                        break;

                    char c = text[i];

                    // 在UI线程上更新界面
                    this.Invoke(new Action(() =>
                    {
                        progressBar.Value = i + 1;
                        statusLabel.Text = $"输入中 {i + 1}/{text.Length}: '{c}' (目标: {targetWindowTitle})";
                        statusLabel.ForeColor = Color.Blue;
                    }));

                    // 发送字符输入到指定窗口
                    SendUnicodeCharToWindow(targetWindowHandle, c);

                    if (i < text.Length - 1) // Don't delay after the last character
                    {
                        try
                        {
                            await Task.Delay(delay, cancellationTokenSource.Token);
                        }
                        catch (OperationCanceledException)
                        {
                            LogDebug("在延迟期间收到取消请求");
                            break;
                        }
                    }
                }

                if (!cancellationTokenSource.Token.IsCancellationRequested)
                {
                    statusLabel.Text = $"输入完成! 已发送 {text.Length} 个字符到 {targetWindowTitle}";
                    statusLabel.ForeColor = Color.DarkGreen;
                }
                else
                {
                    statusLabel.Text = "输入已被用户取消";
                    statusLabel.ForeColor = Color.Red;
                }
            }
            catch (OperationCanceledException)
            {
                statusLabel.Text = "输入已被用户取消";
                statusLabel.ForeColor = Color.Red;
            }
            catch (Exception ex)
            {
                statusLabel.Text = $"错误: {ex.Message}";
                statusLabel.ForeColor = Color.Red;
                MessageBox.Show($"发生错误: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                isRunning = false;
                isPaused = false;
                pauseResumeButton.Text = "暂停";
                progressBar.Visible = false;
                UpdateButtonStates();
                cancellationTokenSource?.Dispose();
                cancellationTokenSource = null;
            }
        }

        private void SendUnicodeChar(char character)
        {
            INPUT[] inputs = new INPUT[1];
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].u.ki.wVk = 0;
            inputs[0].u.ki.wScan = character;
            inputs[0].u.ki.dwFlags = KEYEVENTF_UNICODE;
            inputs[0].u.ki.time = 0;
            inputs[0].u.ki.dwExtraInfo = IntPtr.Zero;

            SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT)));
        }

        private void SendUnicodeCharToWindow(IntPtr windowHandle, char character)
        {
            // 尝试激活目标窗口并发送输入
            try
            {
                // 获取当前线程ID和目标窗口线程ID
                uint currentThreadId = GetCurrentThreadId();
                GetWindowThreadProcessId(windowHandle, out uint processId);
                uint targetThreadId = GetWindowThreadProcessId(windowHandle, out _);

                // 临时附加到目标窗口的线程
                bool attached = false;
                if (targetThreadId != currentThreadId)
                {
                    attached = AttachThreadInput(currentThreadId, targetThreadId, true);
                }

                try
                {
                    // 设置焦点到目标窗口
                    SetForegroundWindow(windowHandle);
                    SetFocus(windowHandle);

                    // 短暂延迟确保窗口获得焦点
                    Thread.Sleep(10);

                    // 发送Unicode字符
                    INPUT[] inputs = new INPUT[1];
                    inputs[0].type = INPUT_KEYBOARD;
                    inputs[0].u.ki.wVk = 0;
                    inputs[0].u.ki.wScan = character;
                    inputs[0].u.ki.dwFlags = KEYEVENTF_UNICODE;
                    inputs[0].u.ki.time = 0;
                    inputs[0].u.ki.dwExtraInfo = IntPtr.Zero;

                    SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT)));
                }
                finally
                {
                    // 分离线程输入
                    if (attached)
                    {
                        AttachThreadInput(currentThreadId, targetThreadId, false);
                    }
                }
            }
            catch
            {
                // 如果专用方法失败，回退到通用方法
                SendUnicodeChar(character);
            }
        }

        private string GetForegroundWindowTitle()
        {
            try
            {
                IntPtr hwnd = GetForegroundWindow();
                if (hwnd == IntPtr.Zero)
                    return string.Empty;

                var sb = new System.Text.StringBuilder(256);
                GetWindowText(hwnd, sb, sb.Capacity);
                return sb.ToString();
            }
            catch
            {
                return string.Empty;
            }
        }

        private void RefreshTargetWindow()
        {
            if (targetWindowHandle != IntPtr.Zero)
            {
                if (!IsWindow(targetWindowHandle))
                {
                    targetWindowHandle = IntPtr.Zero;
                    targetWindowTitle = string.Empty;
                    targetWindowLabel.Text = "窗口已关闭";
                    targetWindowLabel.ForeColor = Color.Red;
                    statusLabel.Text = "目标窗口已关闭，请重新选择";
                    statusLabel.ForeColor = Color.Red;
                }
                else
                {
                    statusLabel.Text = $"目标窗口有效: {targetWindowTitle}";
                    statusLabel.ForeColor = Color.DarkGreen;
                }
                UpdateButtonStates();
            }
        }

        private void UpdateButtonStates()
        {
            bool hasValidTarget = targetWindowHandle != IntPtr.Zero && IsWindow(targetWindowHandle);
            bool hasText = !string.IsNullOrEmpty(textInput.Text);

            startButton.Enabled = !isRunning && hasValidTarget && hasText;
            stopButton.Enabled = isRunning;
            pauseResumeButton.Enabled = isRunning;
            selectWindowButton.Enabled = !isRunning;
            textInput.Enabled = !isRunning;
            delayInput.Enabled = !isRunning;

            LogDebug($"按钮状态更新: isRunning={isRunning}, isPaused={isPaused}, " +
                    $"startEnabled={startButton.Enabled}, stopEnabled={stopButton.Enabled}, " +
                    $"pauseEnabled={pauseResumeButton.Enabled}");
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (isRunning)
            {
                var result = MessageBox.Show(
                    "Input process is still running. Do you want to stop it and exit?",
                    "Confirm Exit",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Question);

                if (result == DialogResult.Yes)
                {
                    cancellationTokenSource?.Cancel();
                }
                else
                {
                    e.Cancel = true;
                    return;
                }
            }

            // 清理资源
            windowCheckTimer?.Stop();
            windowCheckTimer?.Dispose();

            base.OnFormClosing(e);
        }
    }


}
