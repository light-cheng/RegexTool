param([int]$Instance = 0)

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class OC {
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr p, int id);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EC cb, IntPtr lp);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessage")] public static extern IntPtr SendMsgW(IntPtr h, uint m, IntPtr w, StringBuilder l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessage")] public static extern IntPtr SendMsgP(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", EntryPoint="SendMessage")] public static extern IntPtr SendMsgS(IntPtr h, uint m, IntPtr w, string l);
  public delegate bool EC(IntPtr h, IntPtr lp);
  public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

function Get-ControlRects($hwnd) {
  $script:items = New-Object System.Collections.Generic.List[object]
  [OC]::EnumChildWindows($hwnd, {
    param($h, $lp)
    $cls = New-Object System.Text.StringBuilder 64
    [OC]::GetClassName($h, $cls, 64) | Out-Null
    $txt = New-Object System.Text.StringBuilder 256
    $len = [OC]::SendMsgP($h, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero)
    if ([int]$len -gt 0 -and [int]$len -lt 1000) { [OC]::SendMsgW($h, 0x000D, [IntPtr]1000, $txt) | Out-Null }
    $r = New-Object OC+RECT
    [OC]::GetWindowRect($h, [ref]$r) | Out-Null
    $script:items.Add([PSCustomObject]@{ Cls = $cls.ToString(); L = $r.Left; T = $r.Top; R = $r.Right; B = $r.Bottom; Txt = $txt.ToString() })
    return $true
  }, [IntPtr]::Zero) | Out-Null
  return $script:items
}

function Test-Overlaps($hwnd, $step) {
  $items = Get-ControlRects $hwnd
  $over = New-Object System.Collections.Generic.List[string]
  for ($i = 0; $i -lt $items.Count; $i++) {
    for ($j = $i + 1; $j -lt $items.Count; $j++) {
      $a = $items[$i]; $b = $items[$j]
      if ($a.Cls -eq "ComboBox" -or $b.Cls -eq "ComboBox") { continue }
      if ($a.L -lt $b.R -and $a.R -gt $b.L -and $a.T -lt $b.B -and $a.B -gt $b.T) {
        $over.Add(("{0}[{1}] vs {2}[{3}] at ({4},{5})-({6},{7})" -f $a.Cls, $a.Txt, $b.Cls, $b.Txt, $a.L, $a.T, $a.R, $a.B))
      }
    }
  }
  Write-Output ("STEP '{0}': controls={1} overlaps={2}" -f $step, $items.Count, $over.Count)
  foreach ($o in $over) { Write-Output ("  OVERLAP: " + $o) }
}

function Set-WindowSize($hwnd, $w, $h) {
  $rect = New-Object OC+RECT
  [OC]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
  $dw = $w - ($rect.Right - $rect.Left)
  $dh = $h - ($rect.Bottom - $rect.Top)
  [OC]::SendMsgP($hwnd, 0x0114, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null  # WM_SETREDRAW off
  $res = [OC]::SendMsgP($hwnd, 0x0112, [IntPtr]0xF000 + 5, [IntPtr]::Zero)  # WM_SYSCOMMAND SC_RESTORE
  $p = New-Object System.Runtime.InteropServices.HandleRef($hwnd, $hwnd)
  [System.Windows.Forms.SendKeys]::SendWait("") | Out-Null
  # Use SetWindowPos via P/Invoke
  return $res
}

$procs = @(Get-Process regex_tool -ErrorAction SilentlyContinue | Sort-Object Id)
if ($procs.Count -eq 0) { Write-Output "PROC_NOT_RUNNING"; exit 1 }
$idx = [Math]::Min($Instance, $procs.Count - 1)
$hwnd = $procs[$idx].MainWindowHandle
Write-Output ("HWND=" + $hwnd + " title=" + $procs[$idx].MainWindowTitle)

# 1) add rows one at a time, checking overlaps after each
for ($i = 0; $i -lt 10; $i++) {
  [OC]::SendMsgP([OC]::GetDlgItem($hwnd, 1004), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 60
}
Test-Overlaps $hwnd "after 10 adds (per-add)"
for ($i = 0; $i -lt 5; $i++) {
  [OC]::SendMsgP([OC]::GetDlgItem($hwnd, 1005), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 60
}
Test-Overlaps $hwnd "after 5 neg adds"

# 2) fill 5 real positives and generate
$samples = @("PN8821@BN9012@DT2026@LT3344@SQ0001@WK12","PN4503@BN7741@DT2026@LT5566@SQ0002@WK15","PN1192@BN3325@DT2026@LT1122@SQ0003@WK08","PN6678@BN5589@DT2026@LT9900@SQ0004@WK23","PN3345@BN1102@DT2026@LT4488@SQ0005@WK04")
for ($i = 0; $i -lt 5; $i++) {
  $e = [OC]::GetDlgItem($hwnd, 2000 + $i)
  $ptr = [System.Runtime.InteropServices.Marshal]::StringToHGlobalUni($samples[$i])
  [OC]::SendMsgP($e, 0x000C, [IntPtr]::Zero, $ptr) | Out-Null
  [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr) | Out-Null
}
[OC]::SendMsgP([OC]::GetDlgItem($hwnd, 1011), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 400
Test-Overlaps $hwnd "after fill + generate"

# 3) scroll down and up
for ($i = 0; $i -lt 15; $i++) { [OC]::SendMsgP($hwnd, 0x0115, [IntPtr]3, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 40 }
Start-Sleep -Milliseconds 200
Test-Overlaps $hwnd "after page-down to bottom"
for ($i = 0; $i -lt 15; $i++) { [OC]::SendMsgP($hwnd, 0x0115, [IntPtr]2, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 40 }
Start-Sleep -Milliseconds 200
Test-Overlaps $hwnd "after page-up to top"

# 4) delete 3 positive rows while scrolled
for ($i = 0; $i -lt 7; $i++) { [OC]::SendMsgP($hwnd, 0x0115, [IntPtr]3, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 40 }
Start-Sleep -Milliseconds 200
$script:delbtns = New-Object System.Collections.Generic.List[IntPtr]
[OC]::EnumChildWindows($hwnd, {
  param($h, $lp)
  $cls = New-Object System.Text.StringBuilder 64
  [OC]::GetClassName($h, $cls, 64) | Out-Null
  if ($cls.ToString() -eq "Button") {
    $t = New-Object System.Text.StringBuilder 8
    [OC]::SendMsgW($h, 0x000D, [IntPtr]8, $t) | Out-Null
    if ($t.ToString() -eq ([string][char]0x00D7)) { $script:delbtns.Add($h) }
  }
  return $true
}, [IntPtr]::Zero) | Out-Null
$n = [Math]::Min(3, $script:delbtns.Count)
for ($i = 0; $i -lt $n; $i++) { [OC]::SendMsgP($script:delbtns[$i], 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 100 }
Start-Sleep -Milliseconds 300
Test-Overlaps $hwnd "after delete 3 rows while scrolled"
