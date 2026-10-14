# Halo MCC VR mod: export Halo 4 grenade models, materials and textures with
# the official H4EK tool.exe so the mod can show the real grenades.
# Read-only for tags: writes only into H4EK\vrmod_export. Never touches MCC.
$ErrorActionPreference = 'Continue'
$kit  = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $kit
$tags = Join-Path $kit 'tags'
$out  = Join-Path $kit 'vrmod_export'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$log  = Join-Path $out 'export.log'
"VR mod grenade export $(Get-Date -Format o)" | Set-Content $log
$tool = Join-Path $kit 'tool.exe'
if (-not (Test-Path $tool)) { "tool.exe not found in $kit" | Add-Content $log; Write-Host 'tool.exe not found'; exit 1 }

function Run([string[]]$arguments) {
    "> tool.exe $($arguments -join ' ')" | Add-Content $log
    $text = & $tool @arguments 2>&1 | Out-String
    $text | Add-Content $log
    "exit $LASTEXITCODE" | Add-Content $log
}
$exported = @{}
function ExportXml([string]$full) {
    if (-not (Test-Path $full) -or $exported.ContainsKey($full)) { return }
    $exported[$full] = $true
    $name = Split-Path -Leaf $full
    Write-Host "Exporting $name"
    Run @('export-tag-to-xml', $full, (Join-Path $out ($name + '.xml')))
}
function Reference([string]$text, [string]$field) {
    # Tag references appear as value="..." or as element text.
    $pattern = 'name="' + [regex]::Escape($field) + '"[^>]*?(?:value="([^"]*)"|>([^<]*)<)'
    foreach ($m in [regex]::Matches($text, $pattern)) {
        $v = if ($m.Groups[1].Success) { $m.Groups[1].Value } else { $m.Groups[2].Value }
        $v = $v.Trim()
        if ($v -and $v -ne 'NONE') { $v.Split(',')[0] }
    }
}
$bitmaps = @{}
function ExportBitmap([string]$relativeNoExtension) {
    $relativeNoExtension = $relativeNoExtension.Replace('/', '\')
    if ($bitmaps.ContainsKey($relativeNoExtension)) { return }
    if (-not (Test-Path (Join-Path $tags ($relativeNoExtension + '.bitmap')))) {
        "missing bitmap $relativeNoExtension" | Add-Content $log; return
    }
    $bitmaps[$relativeNoExtension] = $true
    $leaf = Split-Path -Leaf $relativeNoExtension
    Write-Host "Exporting texture $leaf"
    ExportXml (Join-Path $tags ($relativeNoExtension + '.bitmap'))
    Run @('export-bitmap-tga', $relativeNoExtension, (Join-Path $out ($leaf + '-')))
}

$grenades = 'storm_frag_grenade', 'storm_plasma_grenade', 'storm_energy_drain_grenade', 'storm_disruption_grenade'
foreach ($g in $grenades) {
    $dir = Join-Path $tags "objects\weapons\grenade\$g"
    if (-not (Test-Path $dir)) { "missing $dir" | Add-Content $log; continue }
    Get-ChildItem $dir -Recurse -File -Include *.render_model, *.projectile, *.equipment, *.model, *.material |
        ForEach-Object { ExportXml $_.FullName }
    Get-ChildItem $dir -Recurse -File -Include *.bitmap | ForEach-Object {
        $rel = $_.FullName.Substring($tags.Length + 1)
        ExportBitmap ($rel.Substring(0, $rel.Length - '.bitmap'.Length))
    }
}
# Materials the render models use, wherever they live, and their textures.
foreach ($x in @(Get-ChildItem $out -Filter '*.render_model.xml')) {
    $text = Get-Content $x.FullName -Raw
    foreach ($m in @(Reference $text 'render method')) {
        $path = Join-Path $tags $m
        if (-not $path.EndsWith('.material')) { $path += '.material' }
        "model $($x.Name) uses $m" | Add-Content $log
        ExportXml $path
    }
}
foreach ($x in @(Get-ChildItem $out -Filter '*.material.xml')) {
    $text = Get-Content $x.FullName -Raw
    foreach ($b in @(Reference $text 'bitmap')) {
        "material $($x.Name) uses bitmap $b" | Add-Content $log
        ExportBitmap $b
    }
}
"done $(Get-Date -Format o)" | Add-Content $log
Write-Host ''
Write-Host "Finished. Files are in $out"
