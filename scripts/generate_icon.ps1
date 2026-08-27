Add-Type -AssemblyName System.Drawing

function Create-RegexIcon($size) {
  $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

  # Background Squircle with subtle gradient
  $pad = [Math]::Max(1, [int]($size * 0.05))
  $rect = New-Object System.Drawing.Rectangle($pad, $pad, ($size - 2 * $pad), ($size - 2 * $pad))
  $radius = [int]($size * 0.22)
  
  # Rounded rectangle path
  $path = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = $radius * 2
  $path.AddArc($rect.X, $rect.Y, $d, $d, 180, 90)
  $path.AddArc($rect.Right - $d, $rect.Y, $d, $d, 270, 90)
  $path.AddArc($rect.Right - $d, $rect.Bottom - $d, $d, $d, 0, 90)
  $path.AddArc($rect.X, $rect.Bottom - $d, $d, $d, 90, 90)
  $path.CloseFigure()

  # Gradient Brush (Modern Windows Deep Blue #005A9E -> #0078D4)
  $pt1 = New-Object System.Drawing.PointF($rect.X, $rect.Y)
  $pt2 = New-Object System.Drawing.PointF($rect.Right, $rect.Bottom)
  $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush($pt1, $pt2, [System.Drawing.Color]::FromArgb(255, 0, 90, 158), [System.Drawing.Color]::FromArgb(255, 0, 120, 212))
  $g.FillPath($grad, $path)

  # Border
  $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(120, 255, 255, 255), [Math]::Max(1.0, $size * 0.02))
  $g.DrawPath($pen, $path)

  # Barcode Lines in background
  $barY = $size * 0.22
  $barH = $size * 0.32
  $bars = @(
    @{ X = 0.20; W = 0.05 },
    @{ X = 0.28; W = 0.03 },
    @{ X = 0.34; W = 0.07 },
    @{ X = 0.44; W = 0.03 },
    @{ X = 0.50; W = 0.06 },
    @{ X = 0.59; W = 0.04 },
    @{ X = 0.66; W = 0.08 },
    @{ X = 0.77; W = 0.04 }
  )
  $barBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(210, 255, 255, 255))
  foreach ($b in $bars) {
    $bx = $size * $b.X
    $bw = [Math]::Max(1.0, $size * $b.W)
    $g.FillRectangle($barBrush, $bx, $barY, $bw, $barH)
  }

  # Regex Accent Plate at Bottom
  $plateY = $size * 0.52
  $plateH = $size * 0.36
  $plateRect = New-Object System.Drawing.RectangleF(($size * 0.14), $plateY, ($size * 0.72), $plateH)
  $platePath = New-Object System.Drawing.Drawing2D.GraphicsPath
  $pr = [int]($size * 0.10)
  $pd = $pr * 2
  $platePath.AddArc($plateRect.X, $plateRect.Y, $pd, $pd, 180, 90)
  $platePath.AddArc($plateRect.Right - $pd, $plateRect.Y, $pd, $pd, 270, 90)
  $platePath.AddArc($plateRect.Right - $pd, $plateRect.Bottom - $pd, $pd, $pd, 0, 90)
  $platePath.AddArc($plateRect.X, $plateRect.Bottom - $pd, $pd, $pd, 90, 90)
  $platePath.CloseFigure()

  $plateBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(240, 24, 32, 47))
  $g.FillPath($plateBrush, $platePath)
  $platePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(180, 0, 180, 255), [Math]::Max(1.0, $size * 0.02))
  $g.DrawPath($platePen, $platePath)

  # Regex Text ".*" / "[.*]"
  $fontSize = $size * 0.24
  $font = New-Object System.Drawing.Font("Consolas", $fontSize, [System.Drawing.FontStyle]::Bold)
  $textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 230, 255))
  $sf = New-Object System.Drawing.StringFormat
  $sf.Alignment = [System.Drawing.StringAlignment]::Center
  $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
  $g.DrawString(".*", $font, $textBrush, $plateRect, $sf)

  $g.Dispose()
  return $bmp
}

$sizes = @(256, 48, 32, 16)
$pngStreams = @()

foreach ($s in $sizes) {
  $bmp = Create-RegexIcon $s
  $ms = New-Object System.IO.MemoryStream
  $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  $pngStreams += $ms
}

$icoPath = "src\app\app.ico"
$fs = [System.IO.File]::Create($icoPath)
$bw = New-Object System.IO.BinaryWriter($fs)

# ICONHEADER
$bw.Write([uint16]0) # Reserved
$bw.Write([uint16]1) # Type (1 = ICO)
$bw.Write([uint16]$sizes.Count) # Image count

# Calculate offsets
$headerSize = 6 + (16 * $sizes.Count)
$currentOffset = $headerSize

for ($i = 0; $i -lt $sizes.Count; $i++) {
  $s = $sizes[$i]
  $wByte = if ($s -ge 256) { 0 } else { [byte]$s }
  $hByte = if ($s -ge 256) { 0 } else { [byte]$s }
  $bytesInRes = [uint32]$pngStreams[$i].Length

  $bw.Write([byte]$wByte)        # bWidth
  $bw.Write([byte]$hByte)        # bHeight
  $bw.Write([byte]0)             # bColorCount
  $bw.Write([byte]0)             # bReserved
  $bw.Write([uint16]1)           # wPlanes
  $bw.Write([uint16]32)          # wBitCount
  $bw.Write([uint32]$bytesInRes) # dwBytesInRes
  $bw.Write([uint32]$currentOffset) # dwImageOffset

  $currentOffset += $bytesInRes
}

# Write Image Data
for ($i = 0; $i -lt $sizes.Count; $i++) {
  $data = $pngStreams[$i].ToArray()
  $bw.Write($data)
  $pngStreams[$i].Dispose()
}

$bw.Close()
$fs.Close()

Write-Output "Successfully generated $icoPath with sizes: $($sizes -join ', ')"
