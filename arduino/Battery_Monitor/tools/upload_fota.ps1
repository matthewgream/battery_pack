[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$file_info,
    [Parameter(Mandatory=$true)]
    [string]$path_build,
    [Parameter(Mandatory=$true)]
    [string]$platform,
    [Parameter(Mandatory=$true)]
    [string]$image,
    [Parameter(Mandatory=$true)]
    [string]$server
)

function Extract-Info {
    param ([string]$filePath, [hashtable]$patterns)
    if (-not (Test-Path $filePath)) {
        return $null
    }
    $content = Get-Content $filePath -Raw
    $result = @{}
    foreach ($key in $patterns.Keys) {
        $match = [regex]::Match($content, $patterns[$key])
        if ($match.Success) {
            $result[$key] = $match.Groups[1].Value
        }
        else {
            $result[$key] = $null
        }
    }
    return $result
}
function Upload-Image {
    param ([string]$binPath, [string]$url, [string]$newFileName, [int]$timeout = 30)    
    try {
        Write-Verbose "Upload file: $newFileName to $url"
        $fileBytes = [System.IO.File]::ReadAllBytes($binPath)
        $fileEnc = [System.Text.Encoding]::GetEncoding('ISO-8859-1').GetString($fileBytes)
        $boundary = [System.Guid]::NewGuid().ToString()
        $LF = "`r`n"
        $bodyLines = (
            "--$boundary",
            "Content-Disposition: form-data; name=`"image`"; filename=`"$newFileName`"",
            "Content-Type: application/octet-stream$LF",
            $fileEnc,
            "--$boundary--$LF"
        ) -join $LF
        $response = Invoke-RestMethod -Uri $url -Method Put -ContentType "multipart/form-data; boundary=`"$boundary`"" -Body $bodyLines -TimeoutSec $timeout
        Write-Verbose "Upload response: $response"
        return $response
    }
    catch {
        throw "Upload failed: $_"
    }
}

$patterns = @{
    'name' = '#define\s+DEFAULT_NAME\s+"([^"]+)"'
    'vers' = '#define\s+DEFAULT_VERS\s+"(\d+\.\d+\.\d+)"'
}
Write-Verbose "Using $file_info"
$matches = Extract-Info -filePath $file_info -patterns $patterns
if (-not $matches -or ($matches.Values -contains $null)) {
    Write-Error "Could not extract type or vers from '$file_info'"
    exit 1
}

$name = $matches['name'].ToLower()
$vers = $matches['vers'].ToLower()
$plat = $platform.ToLower() -replace '_', ''
$file = "$($name)-$($plat)_v$($vers).bin"
$path = Join-Path $path_build $file

Write-Verbose "Image name: $name"
Write-Verbose "Image vers: $vers"
Write-Verbose "Image type: $plat"
Write-Verbose "Image name: $file"
Write-Verbose "Image path: $path"
Write-Verbose "Image host: $server"

if (Test-Path $path) {
    Write-Output "Image $file already exists in build directory. No action taken."
    exit 0
}

try {
    Write-Verbose "Image upload: $image as $file"
    Upload-Image -binPath $image -url $server -newFileName $file
    Write-Output  "Image upload succeeded: $file"
    Copy-Item -Path $image -Destination $path -Force
    Write-Output  "Image copied to build directory: $path"
}
catch {
    Write-Error "Image transfer error: $_"
    exit 1
}
