param([int]$Action = 0, [int]$Instance = 0)

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class W32 {
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr lp);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder sb, int n);
  [DllImport("user32.dll")] public static extern bool SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  public delegate bool EnumProc(IntPtr h, IntPtr lp);
  public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

$procs = @(Get-Process regex_tool -ErrorAction SilentlyContinue | Sort-Object Id)
if ($procs.Count -eq 0) { Write-Output "PROC_NOT_RUNNING"; exit 1 }
$idx = [Math]::Min($Instance, $procs.Count - 1)
$hwnd = $procs[$idx].MainWindowHandle
Write-Output ("USING hwnd=" + $hwnd + " action=" + $Action)

if ($Action -eq 1) { for ($i = 0; $i -lt 5; $i++) { [W32]::SendMessage([W32]::GetDlgItem($hwnd, 1004), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 100 } }
if ($Action -eq 2) { for ($i = 0; $i -lt 5; $i++) { [W32]::SendMessage([W32]::GetDlgItem($hwnd, 1005), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 100 } }
if ($Action -eq 3) { [W32]::SendMessage([W32]::GetDlgItem($hwnd, 1011), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
if ($Action -eq 4) {
  # add 15 positives then page down 12 times to reach the bottom
  for ($i = 0; $i -lt 15; $i++) { [W32]::SendMessage([W32]::GetDlgItem($hwnd, 1004), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 50 }
  for ($i = 0; $i -lt 12; $i++) { [W32]::SendMessage($hwnd, 0x0115, [IntPtr]3, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 50 }
  Write-Output "ADD_POS15+PAGEDOWN12"
}
if ($Action -eq 6) {
  $samples = @("PN8821@BN9012@DT2026@LT3344@SQ0001@WK12", "PN4503@BN7741@DT2026@LT5566@SQ0002@WK15", "PN1192@BN3325@DT2026@LT1122@SQ0003@WK08", "PN6678@BN5589@DT2026@LT9900@SQ0004@WK23", "PN3345@BN1102@DT2026@LT4488@SQ0005@WK04")
  for ($i = 0; $i -lt 5; $i++) {
    $h = [W32]::GetDlgItem($hwnd, 2000 + $i)
    $ptr = [System.Runtime.InteropServices.Marshal]::StringToHGlobalUni($samples[$i])
    [W32]::SendMessage($h, 0x000C, [IntPtr]::Zero, $ptr) | Out-Null
  }
  [W32]::SendMessage([W32]::GetDlgItem($hwnd, 1011), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
  Write-Output "FILLED_5_AND_GENERATED"
}
if ($Action -eq 7) {
  [W32]::SetWindowPos($hwnd, [IntPtr]::Zero, 100, 100, 700, 400, 0x0004) | Out-Null  # SWP_NOZORDER
  Start-Sleep -Milliseconds 300
  Write-Output "RESIZED_700x400"
}
if ($Action -eq 8) {
  [W32]::SetWindowPos($hwnd, [IntPtr]::Zero, 100, 100, 1200, 800, 0x0004) | Out-Null
  Start-Sleep -Milliseconds 300
  Write-Output "RESIZED_1200x800"
}
if ($Action -eq 5) {
  # click the first three delete (x) buttons of the positive column
  $script:delbtns = New-Object System.Collections.Generic.List[IntPtr]
  [W32]::EnumChildWindows($hwnd, {
    param($h, $lp)
    $cls = New-Object System.Text.StringBuilder 64
    [W32]::GetClassName($h, $cls, 64) | Out-Null
    if ($cls.ToString() -eq "Button") {
      $txt = New-Object System.Text.StringBuilder 16
      [W32]::GetWindowText($h, $txt, 16) | Out-Null
      if ($txt.ToString() -eq ([string][char]0x00D7)) { $script:delbtns.Add($h) }
    }
    return $true
  }, [IntPtr]::Zero) | Out-Null
  $n = [Math]::Min(3, $script:delbtns.Count)
  for ($i = 0; $i -lt $n; $i++) { [W32]::SendMessage($script:delbtns[$i], 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 120 }
  Write-Output ("DELETED x" + $n)
}
Start-Sleep -Milliseconds 400

# dump the result pane text (ID 1014) when requested
if ($Action -eq 6) {
  $res = [W32]::GetDlgItem($hwnd, 1014)
  $len = [W32]::SendMessage($res, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero)  # WM_GETTEXTLENGTH
  $sb = New-Object System.Text.StringBuilder ([int]$len + 1)
  [W32]::SendMessage($res, 0x000D, $sb, [IntPtr]::Zero) | Out-Null  # WM_GETTEXT
  Write-Output ("RESULT_TEXT: " + $sb.ToString())
}

$script:list = New-Object System.Collections.Generic.List[string]
[W32]::EnumChildWindows($hwnd, {
  param($h, $lp)
  $cls = New-Object System.Text.StringBuilder 64
  [W32]::GetClassName($h, $cls, 64) | Out-Null
  # GetWindowText cannot read another process's edit control; use WM_GETTEXT.
  $txt = New-Object System.Text.StringBuilder 300
  $len = [W32]::SendMessage($h, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero)  # WM_GETTEXTLENGTH
  if ([int]$len -gt 0) {
    [W32]::SendMessage($h, 0x000D, [IntPtr]($len + 1), $txt) | Out-Null  # WM_GETTEXT
  }
  $r = New-Object W32+RECT
  [W32]::GetWindowRect($h, [ref]$r) | Out-Null
  $script:list.Add(("{0} top={1,-4} bot={2,-4} L={3,-4} R={4,-4} [{5}]" -f $cls.ToString(), $r.Top, $r.Bottom, $r.Left, $r.Right, $txt.ToString()))
  return $true
}, [IntPtr]::Zero) | Out-Null
foreach ($e in $script:list) { Write-Output $e }
