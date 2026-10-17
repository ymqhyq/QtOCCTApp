<#
.SYNOPSIS
  在独立的 Windows 桌面上运行自动化任务，不干扰当前交互桌面。

.DESCRIPTION
  用 CreateDesktop 创建一个隔离桌面，并把 PowerShell 子进程放到该桌面执行
  -CommandText。子进程及其启动的程序（Qt 测试、应用实例）都不会出现在
  当前桌面，也不会抢占焦点。

  典型用途：
    - 自动化调试/回归时启动 GUI 程序或 QtTest（它们会创建真实窗口）
    - 批量构建后运行测试，避免弹窗打断正在进行的其它工作

  注意：DWM 只为"当前交互桌面"合成窗口，因此在隔离桌面上用 PrintWindow
  截图会得到空白图像。需要视觉检查时请改用进程内截图（如 QWidget::grab），
  或显式在交互桌面上运行。

  另：当验证步骤需要用户亲自查看/点击/输入时，请直接在用户交互桌面上启动
  程序并提前告知，不要使用本脚本。

.EXAMPLE
  .\run_in_background_desktop.ps1 -CommandText "ctest -C Release" `
      -WorkingDirectory D:\QtOCCTApp\build_v142

.EXAMPLE
  .\run_in_background_desktop.ps1 -CommandText `
      "Start-Process D:\QtOCCTApp\build_v142\Release\QtOCCTApp.exe" `
      -WorkingDirectory D:\QtOCCTApp\build_v142\Release
#>
param(
  [Parameter(Mandatory = $true)][string]$CommandText,
  [string]$WorkingDirectory = (Get-Location).Path,
  [string]$DesktopName = "CodexAutomationDesktop",
  [int]$TimeoutSeconds = 900,
  [string]$LogPath = (Join-Path $env:TEMP "background_desktop_log.txt")
)

$psExe = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public class BackgroundDesktopHost
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr CreateDesktop(string lpszDesktop, IntPtr lpszDevice,
                                              IntPtr pDevmode, int dwFlags,
                                              uint dwDesiredAccess, IntPtr lpsa);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool CloseDesktop(IntPtr hDesktop);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool CreateProcess(string lpApplicationName, string lpCommandLine,
                                            IntPtr lpProcessAttributes, IntPtr lpThreadAttributes,
                                            bool bInheritHandles, uint dwCreationFlags,
                                            IntPtr lpEnvironment, string lpCurrentDirectory,
                                            ref STARTUPINFO si, out PROCESS_INFORMATION pi);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetExitCodeProcess(IntPtr hProcess, out uint lpExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct STARTUPINFO
    {
        public int cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars;
        public int dwFillAttribute, dwFlags;
        public short wShowWindow, cbReserved2;
        public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PROCESS_INFORMATION
    {
        public IntPtr hProcess, hThread;
        public int dwProcessId, dwThreadId;
    }
}
'@

$wrapped = @"
`$ErrorActionPreference = 'Continue'
`$ProgressPreference = 'SilentlyContinue'
Set-Location -LiteralPath '$WorkingDirectory'
`$global:LASTEXITCODE = 0
$CommandText
`$code = `$LASTEXITCODE
"exit code: `$code" | Out-File -FilePath '$LogPath' -Append -Encoding utf8
"@

$encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($wrapped))

$desktopHandle = [BackgroundDesktopHost]::CreateDesktop(
    $DesktopName, [IntPtr]::Zero, [IntPtr]::Zero, 0, 0x10000000, [IntPtr]::Zero)
if ($desktopHandle -eq [IntPtr]::Zero) {
  $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
  throw "CreateDesktop 失败（Win32 错误 $err）"
}

try {
  $si = New-Object BackgroundDesktopHost+STARTUPINFO
  $si.cb = [Runtime.InteropServices.Marshal]::SizeOf($si)
  $si.lpDesktop = $DesktopName

  $pi = New-Object BackgroundDesktopHost+PROCESS_INFORMATION
  $commandLine = "`"$psExe`" -NoLogo -NoProfile -EncodedCommand $encoded"
  $created = [BackgroundDesktopHost]::CreateProcess(
      $psExe, $commandLine, [IntPtr]::Zero, [IntPtr]::Zero, $false, 0,
      [IntPtr]::Zero, $WorkingDirectory, [ref]$si, [ref]$pi)
  if (-not $created) {
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "CreateProcess 失败（Win32 错误 $err）"
  }

  $waitMs = [uint32][Math]::Min($TimeoutSeconds * 1000, [int]::MaxValue)
  $waitResult = [BackgroundDesktopHost]::WaitForSingleObject($pi.hProcess, $waitMs)
  if ($waitResult -ne 0) {
    Write-Warning "后台任务超时（${TimeoutSeconds}s），已请求结束进程"
    [void][BackgroundDesktopHost]::CloseHandle($pi.hProcess)
    [void][BackgroundDesktopHost]::CloseHandle($pi.hThread)
    return 124
  }

  $exitCode = 0
  [void][BackgroundDesktopHost]::GetExitCodeProcess($pi.hProcess, [ref]$exitCode)
  [void][BackgroundDesktopHost]::CloseHandle($pi.hProcess)
  [void][BackgroundDesktopHost]::CloseHandle($pi.hThread)
  return [int]$exitCode
}
finally {
  [void][BackgroundDesktopHost]::CloseDesktop($desktopHandle)
}
