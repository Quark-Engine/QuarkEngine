param(
    [string]$Source = "assets/lang/english.json",
    [string]$LangDir = "assets/lang",
    [string[]]$Targets
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Load-JsonOrdered {
    param([string]$Path)

    $rawText = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    $raw = $rawText | ConvertFrom-Json

    return ConvertTo-OrderedHashtable -Value $raw
}

function ConvertTo-OrderedHashtable {
    param([object]$Value)

    if ($null -eq $Value) {
        return $null
    }

    if ($Value -is [System.Management.Automation.PSCustomObject]) {
        $ordered = [ordered]@{}
        foreach ($property in $Value.PSObject.Properties) {
            $ordered[$property.Name] = ConvertTo-OrderedHashtable -Value $property.Value
        }
        return $ordered
    }

    if ($Value -is [System.Collections.IEnumerable] -and $Value -isnot [string]) {
        $items = @()
        foreach ($item in $Value) {
            $items += ,(ConvertTo-OrderedHashtable -Value $item)
        }
        return $items
    }

    return $Value
}

function Merge-MissingKeys {
    param(
        [System.Collections.IDictionary]$SourceData,
        [System.Collections.IDictionary]$TargetData
    )

    $added = 0

    foreach ($key in $SourceData.Keys) {

        if (-not $TargetData.Contains($key)) {
            $TargetData[$key] = $SourceData[$key]
            $added++
            continue
        }

        $sourceValue = $SourceData[$key]
        $targetValue = $TargetData[$key]

        if (
            $sourceValue -is [System.Collections.IDictionary] -and
            $targetValue -is [System.Collections.IDictionary]
        ) {
            $added += Merge-MissingKeys `
                -SourceData $sourceValue `
                -TargetData $targetValue
        }
    }

    return $added
}

function Resolve-TargetFiles {
    param(
        [string]$Directory,
        [string[]]$Names,
        [string]$SourcePath
    )

    if ($Names -and $Names.Count -gt 0) {
        return $Names | ForEach-Object {
            if ([System.IO.Path]::GetExtension($_)) {
                Join-Path $Directory $_
            } else {
                Join-Path $Directory ("{0}.json" -f $_)
            }
        }
    }

    return Get-ChildItem -LiteralPath $Directory -Filter *.json |
        Where-Object { $_.FullName -ne (Resolve-Path $SourcePath).Path } |
        Select-Object -ExpandProperty FullName
}

if (-not (Test-Path -LiteralPath $Source)) {
    throw "Source file not found: $Source"
}

if (-not (Test-Path -LiteralPath $LangDir)) {
    throw "Language directory not found: $LangDir"
}

$sourcePath = (Resolve-Path $Source).Path
$sourceData = Load-JsonOrdered -Path $sourcePath
$targetFiles = Resolve-TargetFiles -Directory $LangDir -Names $Targets -SourcePath $sourcePath

foreach ($file in $targetFiles) {
    if (-not (Test-Path -LiteralPath $file)) {
        Write-Warning "Skipped missing target: $file"
        continue
    }

    $targetData = Load-JsonOrdered -Path $file
    $addedKeys = Merge-MissingKeys -SourceData $sourceData -TargetData $targetData

    $json = $targetData | ConvertTo-Json -Depth 100
    [System.IO.File]::WriteAllText((Resolve-Path $file).Path, $json + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))

    Write-Host ("{0}: added {1} missing keys" -f [System.IO.Path]::GetFileName($file), $addedKeys)
}
