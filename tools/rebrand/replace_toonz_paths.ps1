# Replace toonz path segments and user-facing 'OpenToonz' mentions conservatively
$root = Get-Location
$exclusions = @('thirdparty', '\.git', '\.github', 'stuff\\doc\\LICENSE', 'doc\\LICENSE')
$includePatterns = '*.md','*.yml','*.sh','*.py','*.txt','*.xml','*.desktop','*.json','*.in','*.plist','*.c','*.cpp','*.h','*.cmake'

Get-ChildItem -Path $root -Recurse -File -Include $includePatterns -ErrorAction SilentlyContinue | ForEach-Object {
    $path = $_.FullName
    $skip = $false
    foreach ($ex in $exclusions) { if ($path -match $ex) { $skip = $true; break } }
    if ($skip) { return }

    try {
        $text = Get-Content -Raw -LiteralPath $path
    } catch { return }

    $new = $text -replace '/toonz/', '/flare/'
    $new = $new -replace '\$opentoonz/toonz', '$opentoonz/flare'
    $new = $new -replace 'opentoonz/toonz', 'opentoonz/flare'

    # Replace user-facing "OpenToonz" with "Flare" for docs and desktop files only
    if ($path -match '\.md$' -or $path -match '\\xdg-data\\' -or $path -match '\.desktop$' -or $path -match '\.xml$') {
        $new = $new -replace 'OpenToonz', 'Flare'
    }

    if ($new -ne $text) {
        Write-Host "Updating: $path"
        Set-Content -LiteralPath $path -Value $new -Force
    }
}

Write-Host "Rebrand replace script completed."
