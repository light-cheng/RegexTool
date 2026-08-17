param([int]$Rows = 8)

Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public class PX {
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr p, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessage")] public static extern IntPtr SendMsgP(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("gdi32.dll")] public static extern IntPtr CreateCompatibleDC(IntPtr hdc);
  [DllImport("gdi32.dll")] public static extern IntPtr CreateCompatibleBitmap(IntPtr hdc, int w, int h);
  [DllImport("gdi32.dll")] public static extern IntPtr SelectObject(IntPtr hdc, IntPtr obj);
  [DllImport("gdi32.dll")] public static extern bool DeleteObject(IntPtr obj);
  [DllImport("gdi32.dll")] public static extern bool DeleteDC(IntPtr dc);
  public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

function Capture-Window($hwnd) {
  $r = New-Object PX+RECT
  [PX]::GetWindowRect($hwnd, [ref]$r) | Out-Null
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  $wdc = [PX]::GetDC([IntPtr]::Zero)
  $mdc = [PX]::CreateCompatibleDC($wdc)
  $bmp = [PX]::CreateCompatibleBitmap($wdc, $w, $h)
  $old = [PX]::SelectObject($mdc, $bmp)
  [PX]::PrintWindow($hwnd, $mdc, 2) | Out-Null   # PW_CLIENTONLY
  [PX]::SelectObject($mdc, $old) | Out-Null
  $b = [System.Drawing.Bitmap]::FromHbitmap($bmp)
  [PX]::DeleteObject($bmp) | Out-Null
  [PX]::DeleteDC($mdc) | Out-Null
  [PX]::ReleaseDC([IntPtr]::Zero, $wdc) | Out-Null
  return $b
}

$procs = @(Get-Process regex_tool -ErrorAction SilentlyContinue | Sort-Object Id)
if ($procs.Count -eq 0) { Write-Output "PROC_NOT_RUNNING"; exit 1 }
$hwnd = $procs[0].MainWindowHandle

# Add rows
for ($i = 0; $i -lt $Rows; $i++) { [PX]::SendMsgP([PX]::GetDlgItem($hwnd, 1004), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 50 }
# Fill 5 real positives (WM_SETTEXT via HGlobal) and generate
$samples = @("PN8821@BN9012@DT2026@LT3344@SQ0001@WK12","PN4503@BN7741@DT2026@LT5566@SQ0002@WK15","PN1192@BN3325@DT2026@LT1122@SQ0003@WK08","PN6678@BN5589@DT2026@LT9900@SQ0004@WK23","PN3345@BN1102@DT2026@LT4488@SQ0005@WK04")
for ($i = 0; $i -lt 5; $i++) {
  $e = [PX]::GetDlgItem($hwnd, 2000 + $i)
  $ptr = [System.Runtime.InteropServices.Marshal]::StringToHGlobalUni($samples[$i])
  [PX]::SendMsgP($e, 0x000C, [IntPtr]::Zero, $ptr) | Out-Null
  [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr) | Out-Null
}
[PX]::SendMsgP([PX]::GetDlgItem($hwnd, 1011), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 500

$b = Capture-Window $hwnd
$out = "capture_" + $Rows + "rows.png"
$b.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output ("SAVED " + $out + " " + $b.Width + "x" + $b.Height)

# Scan the LEFT column only (x < 460; the right result pane is legitimately white).
# In a healthy UI, each edit row shows a ~330px-wide white band (x 8..338).
# Ghosts of moved edits appear as white bands at the OLD y positions too,
# so the band count exceeds the row count, or bands appear where labels/buttons
# should be.
$threshold = 200  # >200 white px across x<460 marks this y as an "edit row"
$whiteBands = New-Object System.Collections.Generic.List[int]
for ($y = 0; $y -lt $b.Height; $y++) {
  $white = 0
  for ($x = 0; $x -lt 460; $x++) {
    $c = $b.GetPixel($x, $y)
    if ($c.R -gt 245 -and $c.G -gt 245 -and $c.B -gt 245) { $white++ }
  }
  if ($white -gt $threshold) { $whiteBands.Add($y) }
}
# Report contiguous white bands (each is a row of edit boxes)
$groups = New-Object System.Collections.Generic.List[string]
$start = -1; $prev = -2
foreach ($y in $whiteBands) {
  if ($y -ne $prev + 1) { if ($start -ge 0) { $groups.Add(("band " + $start + ".." + $prev)) }; $start = $y }
  $prev = $y
}
if ($start -ge 0) { $groups.Add(("band " + $start + ".." + $prev)) }
Write-Output ("WHITE_BANDS (" + $groups.Count + "): " + ($groups -join "; "))

# Diagnostic: sample a few pixels where a ghost WOULD appear if repaint failed.
# y = 8+32+20 = 60 is the first edit row; with 8 rows the neg label moved down
# 8*28 px, so the old neg-label area (y=94..112 originally) must be gray now.
$samples = @(70, 100, 130, 160, 190, 220, 250, 280, 310, 340)
foreach ($sy in $samples) {
  if ($sy -lt $b.Height) {
    $c = $b.GetPixel(100, $sy)
    Write-Output ("pixel(100," + $sy + ") = (" + $c.R + "," + $c.G + "," + $c.B + ")")
  }
}
$b.Dispose()
