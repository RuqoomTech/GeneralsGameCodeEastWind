[CmdletBinding()]
param(
    [switch]$IncludeLegacyX86,
    [switch]$SkipMsysUpdate,
    [switch]$SkipPathUpdate,
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Step([string]$Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Find-Msys2Root {
    $candidates = @()
    if ($env:MSYS2_ROOT) {
        $candidates += $env:MSYS2_ROOT
    }
    $candidates += 'C:\msys64'

    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (Test-Path (Join-Path $candidate 'usr\bin\bash.exe')) {
            return (Resolve-Path $candidate).Path
        }
    }
    return $null
}

function Install-WingetPackage([string]$Id) {
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw "'$Id' is required, but winget.exe is unavailable. Install it manually, then rerun this script."
    }

    Write-Step "Installing $Id with winget"
    & $winget.Source install --exact --id $Id --silent --accept-package-agreements --accept-source-agreements --disable-interactivity
    if ($LASTEXITCODE -ne 0) {
        throw "winget failed while installing $Id (exit code $LASTEXITCODE)."
    }
}

function Invoke-Msys([string]$BashPath, [string]$Command) {
    & $BashPath -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "MSYS2 command failed (exit code $LASTEXITCODE): $Command"
    }
}

function Add-UserPathEntry([string]$Entry) {
    $current = [Environment]::GetEnvironmentVariable('Path', 'User')
    $parts = @()
    if ($current) {
        $parts = $current -split ';' | Where-Object { $_ }
    }

    $alreadyPresent = $parts | Where-Object { $_.TrimEnd('\\') -ieq $Entry.TrimEnd('\\') }
    if (-not $alreadyPresent) {
        $newPath = (($parts + $Entry) -join ';')
        [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
        Write-Host "Added to user PATH: $Entry"
    }

    if (($env:Path -split ';') -notcontains $Entry) {
        $env:Path = "$Entry;$env:Path"
    }
}

function Assert-Tool([string]$Path, [string]$Name, [string[]]$Arguments = @('--version')) {
    if (-not (Test-Path $Path)) {
        throw "$Name was not found at '$Path'."
    }

    Write-Host ("{0}: {1}" -f $Name, $Path)
    & $Path @Arguments | Select-Object -First 2 | ForEach-Object { Write-Host "  $_" }
    if ($LASTEXITCODE -ne 0) {
        throw "$Name verification failed (exit code $LASTEXITCODE)."
    }
}

if ($PSVersionTable.PSEdition -eq 'Core' -and -not $IsWindows) {
    throw 'scripts/setup-windows-dev.ps1 must be run on Windows.'
}

$msysRoot = Find-Msys2Root
if (-not $msysRoot) {
    if ($VerifyOnly) {
        throw 'MSYS2 was not found. Expected C:\msys64 or MSYS2_ROOT.'
    }
    Install-WingetPackage 'MSYS2.MSYS2'
    $msysRoot = Find-Msys2Root
    if (-not $msysRoot) {
        throw 'MSYS2 installation completed but its root could not be located. Set MSYS2_ROOT and rerun.'
    }
}

$bash = Join-Path $msysRoot 'usr\bin\bash.exe'
$mingw64Bin = Join-Path $msysRoot 'mingw64\bin'
$mingw32Bin = Join-Path $msysRoot 'mingw32\bin'

if (-not $VerifyOnly) {
    if (-not $SkipMsysUpdate) {
        Write-Step 'Updating the MSYS2 package database and base system'
        # Two passes handle the normal MSYS2 core-package self-update boundary.
        Invoke-Msys $bash 'pacman -Syu --noconfirm'
        Invoke-Msys $bash 'pacman -Syu --noconfirm'
    }

    Write-Step 'Installing the x64 Evolution build toolchain'
    Invoke-Msys $bash 'pacman -S --needed --noconfirm mingw-w64-x86_64-toolchain mingw-w64-x86_64-tools mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-python'

    if ($IncludeLegacyX86) {
        Write-Step 'Installing the frozen i686 determinism-oracle toolchain'
        Invoke-Msys $bash 'pacman -S --needed --noconfirm mingw-w64-i686-toolchain'
    }

    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        try {
            Install-WingetPackage 'Git.Git'
            $gitCmd = 'C:\Program Files\Git\cmd'
            if (-not $SkipPathUpdate -and (Test-Path (Join-Path $gitCmd 'git.exe'))) {
                Add-UserPathEntry $gitCmd
            }
        }
        catch {
            Write-Step 'Git for Windows was unavailable; installing MSYS2 git as a fallback'
            Invoke-Msys $bash 'pacman -S --needed --noconfirm git'
            if (-not $SkipPathUpdate) {
                Add-UserPathEntry (Join-Path $msysRoot 'usr\bin')
            }
        }
    }
}

if (-not $SkipPathUpdate) {
    Add-UserPathEntry $mingw64Bin
}

Write-Step 'Verifying the x64 toolchain'
Assert-Tool (Join-Path $mingw64Bin 'gcc.exe') 'x64 GCC'
Assert-Tool (Join-Path $mingw64Bin 'g++.exe') 'x64 G++'
Assert-Tool (Join-Path $mingw64Bin 'cmake.exe') 'CMake'
Assert-Tool (Join-Path $mingw64Bin 'ninja.exe') 'Ninja'
Assert-Tool (Join-Path $mingw64Bin 'widl.exe') 'WIDL'
Assert-Tool (Join-Path $mingw64Bin 'python.exe') 'Python'

if ($IncludeLegacyX86) {
    Write-Step 'Verifying the optional i686 oracle toolchain'
    Assert-Tool (Join-Path $mingw32Bin 'gcc.exe') 'i686 GCC'
    Assert-Tool (Join-Path $mingw32Bin 'g++.exe') 'i686 G++'
    Assert-Tool (Join-Path $mingw32Bin 'widl.exe') 'i686 WIDL'
}

Write-Host ''
Write-Host 'Windows development dependencies are ready.' -ForegroundColor Green
Write-Host 'Next x64 validation commands:'
Write-Host '  cmake --preset mingw64-tests'
Write-Host '  cmake --build --preset mingw64-tests'
Write-Host '  ctest --preset mingw64-tests --output-on-failure'
if ($IncludeLegacyX86) {
    Write-Host ''
    Write-Host 'Frozen i686 determinism oracle:'
    Write-Host '  cmake --preset mingw-w64-i686-determinism'
    Write-Host '  cmake --build --preset mingw-w64-i686-determinism --target z_determinismcheck'
}
