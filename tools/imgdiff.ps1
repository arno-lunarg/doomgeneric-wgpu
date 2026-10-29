# Compares two PNGs of the same size. Prints the count and bounding box of differing pixels.
# -Out writes a diff image: differing pixels red, identical ones a dimmed copy of A.
# Exit code 0 identical, 1 different, 2 size mismatch.
#
# Usage: pwsh -File tools/imgdiff.ps1 -A gpu.png -B gdi.png [-Out diff.png]
param(
    [Parameter(Mandatory)][string]$A,
    [Parameter(Mandatory)][string]$B,
    [string]$Out = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# C# for the per-pixel loop: in PowerShell it takes seconds on a window capture.
# Plain int[] in and out, so no System.Drawing reference to resolve at compile time.
Add-Type @"
public static class ImgDiff {
    public static int Count, X0, Y0, X1, Y1;

    // Alpha ignored: DG_ScreenBuffer alpha is 0, window captures are opaque.
    public static int[] Compare(int[] a, int[] b, int w) {
        int n = a.Length;
        var diff = new int[n];
        Count = 0; X0 = int.MaxValue; Y0 = int.MaxValue; X1 = -1; Y1 = -1;
        for (int i = 0; i < n; i++) {
            int ca = a[i] & 0xFFFFFF;
            if (ca != (b[i] & 0xFFFFFF)) {
                int x = i % w, y = i / w;
                Count++;
                if (x < X0) X0 = x; if (x > X1) X1 = x;
                if (y < Y0) Y0 = y; if (y > Y1) Y1 = y;
                diff[i] = unchecked((int)0xFFFF0000);
            } else {
                diff[i] = unchecked((int)0xFF000000) | ((ca >> 2) & 0x3F3F3F);
            }
        }
        return diff;
    }
}
"@

function Get-FullPath([string]$p) { [IO.Path]::GetFullPath([IO.Path]::Combine((Get-Location).Path, $p)) }

# 32bpp ARGB pixels, row-major, stride padding removed.
function Read-Pixels([string]$path) {
    $bmp  = [System.Drawing.Bitmap]::new((Get-FullPath $path))
    $data = $bmp.LockBits([System.Drawing.Rectangle]::new(0, 0, $bmp.Width, $bmp.Height), 'ReadOnly', 'Format32bppArgb')
    $px   = [int[]]::new($bmp.Width * $bmp.Height)
    for ($y = 0; $y -lt $bmp.Height; $y++) {
        [Runtime.InteropServices.Marshal]::Copy([IntPtr]::Add($data.Scan0, $y * $data.Stride), $px, $y * $bmp.Width, $bmp.Width)
    }
    $bmp.UnlockBits($data)
    $img = [pscustomobject]@{ W = $bmp.Width; H = $bmp.Height; Px = $px }
    $bmp.Dispose()
    $img
}

# $A, $B are [string] params and PowerShell names are case-insensitive: $a would convert the image to a string.
$imgA = Read-Pixels $A
$imgB = Read-Pixels $B
if ($imgA.W -ne $imgB.W -or $imgA.H -ne $imgB.H) {
    Write-Host "Size differs: $($imgA.W)x$($imgA.H) vs $($imgB.W)x$($imgB.H)"
    exit 2
}

$diff = [ImgDiff]::Compare($imgA.Px, $imgB.Px, $imgA.W)
if ([ImgDiff]::Count -eq 0) {
    Write-Host "Identical ($($imgA.W)x$($imgA.H))"
} else {
    Write-Host "$([ImgDiff]::Count) pixels differ, bbox ($([ImgDiff]::X0),$([ImgDiff]::Y0))-($([ImgDiff]::X1),$([ImgDiff]::Y1))"
}

if ($Out) {
    $bmp  = [System.Drawing.Bitmap]::new($imgA.W, $imgA.H, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $data = $bmp.LockBits([System.Drawing.Rectangle]::new(0, 0, $imgA.W, $imgA.H), 'WriteOnly', 'Format32bppArgb')
    for ($y = 0; $y -lt $imgA.H; $y++) {
        [Runtime.InteropServices.Marshal]::Copy($diff, $y * $imgA.W, [IntPtr]::Add($data.Scan0, $y * $data.Stride), $imgA.W)
    }
    $bmp.UnlockBits($data)
    $bmp.Save((Get-FullPath $Out), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Saved $Out"
}

exit ([int]([ImgDiff]::Count -ne 0))
