using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace WindowDebugger
{
    class Program
    {
        // Windows API declarations
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
        private static extern bool IsWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern IntPtr GetForegroundWindow();

        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        private static List<WindowInfo> windows = new List<WindowInfo>();

        private struct WindowInfo
        {
            public IntPtr Handle;
            public string Title;
            public string ProcessName;
            public uint ProcessId;
            public bool IsVisible;
            public bool IsValid;

            public override string ToString()
            {
                return $"[{Handle:X8}] {Title} ({ProcessName}, PID:{ProcessId}) - Visible:{IsVisible}, Valid:{IsValid}";
            }
        }

        static void Main(string[] args)
        {
            Console.WriteLine("=== Windows 枚举调试工具 ===");
            Console.WriteLine();

            while (true)
            {
                Console.WriteLine("选择操作:");
                Console.WriteLine("1. 枚举所有窗口");
                Console.WriteLine("2. 枚举可见窗口");
                Console.WriteLine("3. 枚举过滤后的窗口");
                Console.WriteLine("4. 获取当前前台窗口");
                Console.WriteLine("5. 测试特定窗口句柄");
                Console.WriteLine("0. 退出");
                Console.Write("请输入选择 (0-5): ");

                string? input = Console.ReadLine();
                Console.WriteLine();

                switch (input)
                {
                    case "1":
                        EnumerateAllWindows();
                        break;
                    case "2":
                        EnumerateVisibleWindows();
                        break;
                    case "3":
                        EnumerateFilteredWindows();
                        break;
                    case "4":
                        GetCurrentForegroundWindow();
                        break;
                    case "5":
                        TestSpecificWindow();
                        break;
                    case "0":
                        return;
                    default:
                        Console.WriteLine("无效选择，请重试。");
                        break;
                }

                Console.WriteLine();
                Console.WriteLine("按任意键继续...");
                Console.ReadKey();
                Console.Clear();
            }
        }

        static void EnumerateAllWindows()
        {
            Console.WriteLine("=== 枚举所有窗口 ===");
            windows.Clear();

            try
            {
                bool result = EnumWindows(EnumAllWindowsCallback, IntPtr.Zero);
                Console.WriteLine($"EnumWindows 返回: {result}");
                Console.WriteLine($"找到窗口总数: {windows.Count}");
                Console.WriteLine();

                for (int i = 0; i < windows.Count; i++)
                {
                    Console.WriteLine($"{i + 1:D3}. {windows[i]}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"错误: {ex.Message}");
            }
        }

        static void EnumerateVisibleWindows()
        {
            Console.WriteLine("=== 枚举可见窗口 ===");
            windows.Clear();

            try
            {
                bool result = EnumWindows(EnumVisibleWindowsCallback, IntPtr.Zero);
                Console.WriteLine($"EnumWindows 返回: {result}");
                Console.WriteLine($"找到可见窗口数: {windows.Count}");
                Console.WriteLine();

                for (int i = 0; i < windows.Count; i++)
                {
                    Console.WriteLine($"{i + 1:D3}. {windows[i]}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"错误: {ex.Message}");
            }
        }

        static void EnumerateFilteredWindows()
        {
            Console.WriteLine("=== 枚举过滤后的窗口 ===");
            windows.Clear();

            try
            {
                bool result = EnumWindows(EnumFilteredWindowsCallback, IntPtr.Zero);
                Console.WriteLine($"EnumWindows 返回: {result}");
                Console.WriteLine($"找到过滤后窗口数: {windows.Count}");
                Console.WriteLine();

                for (int i = 0; i < windows.Count; i++)
                {
                    Console.WriteLine($"{i + 1:D3}. {windows[i]}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"错误: {ex.Message}");
            }
        }

        static void GetCurrentForegroundWindow()
        {
            Console.WriteLine("=== 当前前台窗口 ===");
            
            try
            {
                IntPtr foregroundWindow = GetForegroundWindow();
                Console.WriteLine($"前台窗口句柄: {foregroundWindow:X8}");

                if (foregroundWindow != IntPtr.Zero)
                {
                    var windowInfo = GetWindowInfo(foregroundWindow);
                    Console.WriteLine($"窗口信息: {windowInfo}");
                }
                else
                {
                    Console.WriteLine("无法获取前台窗口");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"错误: {ex.Message}");
            }
        }

        static void TestSpecificWindow()
        {
            Console.Write("请输入窗口句柄 (十六进制，如 1A2B3C): ");
            string? input = Console.ReadLine();

            if (string.IsNullOrEmpty(input))
                return;

            try
            {
                IntPtr handle = new IntPtr(Convert.ToInt64(input, 16));
                Console.WriteLine($"测试窗口句柄: {handle:X8}");

                var windowInfo = GetWindowInfo(handle);
                Console.WriteLine($"窗口信息: {windowInfo}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"错误: {ex.Message}");
            }
        }

        static bool EnumAllWindowsCallback(IntPtr hWnd, IntPtr lParam)
        {
            try
            {
                var windowInfo = GetWindowInfo(hWnd);
                windows.Add(windowInfo);
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"EnumAllWindowsCallback 错误: {ex.Message}");
                return true;
            }
        }

        static bool EnumVisibleWindowsCallback(IntPtr hWnd, IntPtr lParam)
        {
            try
            {
                if (!IsWindowVisible(hWnd))
                    return true;

                var windowInfo = GetWindowInfo(hWnd);
                windows.Add(windowInfo);
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"EnumVisibleWindowsCallback 错误: {ex.Message}");
                return true;
            }
        }

        static bool EnumFilteredWindowsCallback(IntPtr hWnd, IntPtr lParam)
        {
            try
            {
                if (!IsWindowVisible(hWnd))
                    return true;

                var windowInfo = GetWindowInfo(hWnd);

                // 应用过滤规则
                if (ShouldExcludeWindow(windowInfo))
                    return true;

                windows.Add(windowInfo);
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"EnumFilteredWindowsCallback 错误: {ex.Message}");
                return true;
            }
        }

        static WindowInfo GetWindowInfo(IntPtr hWnd)
        {
            var info = new WindowInfo
            {
                Handle = hWnd,
                IsVisible = IsWindowVisible(hWnd),
                IsValid = IsWindow(hWnd)
            };

            // 获取窗口标题
            try
            {
                int length = GetWindowTextLength(hWnd);
                if (length > 0)
                {
                    var title = new StringBuilder(length + 1);
                    GetWindowText(hWnd, title, title.Capacity);
                    info.Title = title.ToString();
                }
                else
                {
                    info.Title = "<无标题>";
                }
            }
            catch
            {
                info.Title = "<获取标题失败>";
            }

            // 获取进程信息
            try
            {
                GetWindowThreadProcessId(hWnd, out uint processId);
                info.ProcessId = processId;
                info.ProcessName = GetProcessName(processId);
            }
            catch
            {
                info.ProcessId = 0;
                info.ProcessName = "<获取进程失败>";
            }

            return info;
        }

        static string GetProcessName(uint processId)
        {
            try
            {
                IntPtr hProcess = OpenProcess(0x0400 | 0x0010, false, processId);
                if (hProcess != IntPtr.Zero)
                {
                    try
                    {
                        var processName = new StringBuilder(1024);
                        uint result = GetModuleBaseName(hProcess, IntPtr.Zero, processName, (uint)processName.Capacity);
                        if (result > 0)
                        {
                            return processName.ToString();
                        }
                    }
                    finally
                    {
                        CloseHandle(hProcess);
                    }
                }
            }
            catch
            {
                // 忽略错误
            }

            return "Unknown";
        }

        static bool ShouldExcludeWindow(WindowInfo windowInfo)
        {
            string title = windowInfo.Title ?? "";

            // 排除空标题或很短的标题
            if (string.IsNullOrWhiteSpace(title) || title.Length < 2)
                return true;

            // 排除系统窗口
            if (title == "Program Manager" ||
                title == "Desktop" ||
                title.StartsWith("Microsoft Text Input Application") ||
                title.StartsWith("Windows Input Experience"))
                return true;

            // 排除本应用程序的窗口
            if (title.Contains("Input Assistant") ||
                title == "选择目标窗口" ||
                title.Contains("InputAssistant") ||
                title.Contains("WindowDebugger"))
                return true;

            // 排除一些常见的系统进程窗口
            string processName = windowInfo.ProcessName?.ToLower() ?? "";
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

            foreach (string sysProcess in systemProcesses)
            {
                if (processName == sysProcess)
                {
                    // explorer.exe 不排除
                    if (sysProcess == "explorer.exe")
                        return false;
                    return true;
                }
            }

            return false;
        }
    }
}
