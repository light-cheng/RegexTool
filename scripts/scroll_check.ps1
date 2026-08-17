param()

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class SC {
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr p, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessage")] public static extern IntPtr SendMsgP(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EC cb, IntPtr lp);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
  public delegate bool EC(IntPtr h, IntPtr lp);
  public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

function Test-Overlaps($hwnd, $step) {
  $script:items = New-Object System.Collections.Generic.List[object]
  [SC]::EnumChildWindows($hwnd, {
    param($h, $lp)
    $cls = New-Object System.Text.StringBuilder 64
    [SC]::GetClassName($h, $cls, 64) | Out-Null
    $r = New-Object SC+RECT
    [SC]::GetWindowRect($h, [ref]$r) | Out-Null
    $script:items.Add([PSCustomObject]@{ Cls = $cls.ToString(); L = $r.Left; T = $r.Top; R = $r.Right; B = $r.Bottom })
    return $true
  }, [IntPtr]::Zero) | Out-Null
  $over = New-Object System.Collections.Generic.List[string]
  for ($i = 0; $i -lt $script:items.Count; $i++) {
    for ($j = $i + 1; $j -lt $script:items.Count; $j++) {
      $a = $script:items[$i]; $b = $script:items[$j]
      if ($a.Cls -eq "ComboBox" -or $b.Cls -eq "ComboBox") { continue }
      if ($a.L -lt $b.R -and $a.R -gt $b.L -and $a.T -lt $b.B -and $a.B -gt $b.T) {
        $over.Add(("{0} vs {1} at ({2},{3})-({4},{5})" -f $a.Cls, $b.Cls, $a.L, $a.T, $a.R, $a.B))
      }
    }
  }
  Write-Output ("STEP '{0}': controls={1} overlaps={2}" -f $step, $script:items.Count, $over.Count)
  foreach ($o in $over) { Write-Output ("  OVERLAP: " + $o) }
}

$procs = @(Get-Process regex_tool -ErrorAction SilentlyContinue | Sort-Object Id)
if ($procs.Count -eq 0) { Write-Output "PROC_NOT_RUNNING"; exit 1 }
$hwnd = $procs[0].MainWindowHandle

# Add 15 positives + 5 negatives
for ($i = 0; $i -lt 15; $i++) { [SC]::SendMsgP([SC]::GetDlgItem($hwnd, 1004), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 30 }
for ($i = 0; $i -lt 5; $i++) { [SC]::SendMsgP([SC]::GetDlgItem($hwnd, 1005), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 30 }
Start-Sleep -Milliseconds 300
Test-Overlaps $hwnd "15 pos + 5 neg added"

# Page-down 12 times (bottom), then page-up 12 (top)
for ($i = 0; $i -lt 12; $i++) { [SC]::SendMsgP($hwnd, 0x0115, [IntPtr]3, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 30 }
Start-Sleep -Milliseconds 200
Test-Overlaps $hwnd "scrolled to bottom"
for ($i = 0; $i -lt 12; $i++) { [SC]::SendMsgP($hwnd, 0x0115, [IntPtr]2, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 30 }
Start-Sleep -Milliseconds 200
Test-Overlaps $hwnd "scrolled back to top"

# Scroll down a bit, then delete 3 positive rows (scrolled state)
for ($i = 0; $i -lt 5; $i++) { [SC]::SendMsgP($hwnd, 0x0115, [IntPtr]3, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 30 }
Start-Sleep -Milliseconds 200
$script:delbtns = New-Object System.Collections.Generic.List[IntPtr]
[SC]::EnumChildWindows($hwnd, {
  param($h, $lp)
  $cls = New-Object System.Text.StringBuilder 64
  [SC]::GetClassName($h, $cls, 64) | Out-Null
  if ($cls.ToString() -eq "Button") { $script:delbtns.Add($h) }
  return $true
}, [IntPtr]::Zero) | Out-Null
# Delete buttons have the lowest ids (3000/5000 range) — grab by id range instead
$delIds = @()
[SC]::EnumChildWindows($hwnd, {
  param($h, $lp)
  $id = [SC]::SendMsgP($h, 0x00F4, [IntPtr]::Zero, [IntPtr]::Zero)  # WM_GETDLGCODE? no - use GetDlgCtrlID
  return $true
}, [IntPtr]::Zero) | Out-Null
# Simpler: click the first 3 buttons whose rect is a small 28x22 button (del buttons)
$script:btns = New-Object System.Collections.Generic.List[object]
[SC]::EnumChildWindows($hwnd, {
  param($h, $lp)
  $cls = New-Object System.Text.StringBuilder 64
  [SC]::GetClassName($h, $cls, 64) | Out-Null
  if ($cls.ToString() -eq "Button") {
    $r = New-Object SC+RECT
    [SC]::GetWindowRect($h, [ref]$r) | Out-Null
    $script:btns.Add([PSCustomObject]@{ H = $h; W = $r.Right - $r.Left; T = $r.Top })
  }
  return $true
}, [IntPtr]::Zero) | Out-Null
# Delete buttons are width <= 44 (del is 28, promote is 44). Pick the narrowest 3.
$cands = @($script:btns | Where-Object { $_.W -le 44 } | Sort-Object T)
$n = [Math]::Min(3, $cands.Count)
for ($i = 0; $i -lt $n; $i++) { [SC]::SendMsgP($cands[$i].H, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 80 }
Start-Sleep -Milliseconds 300
Test-Overlaps $hwnd "deleted 3 rows while scrolled"

# Back to top
for ($i = 0; $i -lt 12; $i++) { [SC]::SendMsgP($hwnd, 0x0115, [IntPtr]2, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 30 }
Start-Sleep -Milliseconds 200
Test-Overlaps $hwnd "back to top after delete"
