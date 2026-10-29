# Runs an exe, waits, saves a PNG of its window, kills it, prints its stdout/stderr.
# Finds the window by class (platform_win32.c registers "WebGpuWindow"). PrintWindow captures
# DXGI flip-model content even when the window is covered.
#
# Usage: pwsh -File tools/capture.ps1 -Exe build\Debug\step1_triangle.exe -Out shot.png [-Args -vulkan] [-WorkDir wad] [-Wait 4]
param(
    [Parameter(Mandatory)][string]$Exe,
    [Parameter(Mandatory)][string]$Out,
    [string]$Args = '',
    [string]$WorkDir = '',
    [int]$Wait = 4,
    [string]$Class = 'WebGpuWindow'
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll", CharSet = CharSet.Ansi)] public static extern IntPtr FindWindowA(string c, string t);
}
"@
[Win]::SetProcessDPIAware() | Out-Null

$ErrorActionPreference = 'Stop'

# Combine keeps $Out as-is when it is already absolute.
$Exe = (Resolve-Path $Exe).Path
$Out = [IO.Path]::GetFullPath([IO.Path]::Combine((Get-Location).Path, $Out))
if (-not $WorkDir) { $WorkDir = Split-Path $Exe }

# Redirected stdout is fully buffered: it may be empty after the kill. stderr is not.
$stdout = New-TemporaryFile
$stderr = New-TemporaryFile
$start  = @{ FilePath = $Exe; WorkingDirectory = $WorkDir; PassThru = $true; RedirectStandardOutput = $stdout; RedirectStandardError = $stderr }
if ($Args) { $start.ArgumentList = $Args }

$p = Start-Process @start
Start-Sleep -Seconds $Wait
$p.Refresh()

if ($p.HasExited) {
    Write-Host "Exited early, code $($p.ExitCode)"
} else {
    try {
        $h = [Win]::FindWindowA($Class, [NullString]::Value)
        if ($h -eq [IntPtr]::Zero) {
            Write-Host "No window of class $Class"
        } else {
            $r = New-Object Win+RECT
            [Win]::GetWindowRect($h, [ref]$r) | Out-Null
            $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
            $g   = [System.Drawing.Graphics]::FromImage($bmp)
            $hdc = $g.GetHdc()
            [Win]::PrintWindow($h, $hdc, 2) | Out-Null  # 2 = PW_RENDERFULLCONTENT
            $g.ReleaseHdc($hdc)
            $bmp.Save($Out)
            Write-Host "Saved $Out ($($bmp.Width)x$($bmp.Height))"
        }
    } finally {
        Stop-Process -Id $p.Id -Force
    }
}

Write-Host '--- stdout'; Get-Content $stdout
Write-Host '--- stderr'; Get-Content $stderr
Remove-Item $stdout, $stderr
