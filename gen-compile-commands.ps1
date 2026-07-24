# Generate compile_commands.json and .clangd for clangd/IntelliSense.
# Run from the project root after cloning or when source files change.
# Requires InfNoiseRack and Rack as sibling folders (../Rack).

$ErrorActionPreference = "Stop"
$dir = (Get-Location).Path -replace '\\', '/'
$rackDir = if ($env:RACK_DIR) { $env:RACK_DIR -replace '\\', '/' } else { "../Rack" }

$compiler = $env:CXX
if (-not $compiler) {
    $cmd = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($cmd) { $compiler = $cmd.Source } else { $compiler = "C:/msys64/mingw64/bin/g++.exe" }
}
$compiler = $compiler -replace '\\', '/'

if (-not (Test-Path $compiler)) {
    Write-Error "g++ not found at $compiler. Set CXX or add g++ to PATH."
}

$compilerDir = (Split-Path $compiler -Parent) -replace '\\', '/'
$mingwRoot = (Resolve-Path "$compilerDir/..").Path -replace '\\', '/'
$gccVersion = (& $compiler -dumpversion).Trim()
$gccTriple = "x86_64-w64-mingw32"

$systemIncludes = @(
    "$mingwRoot/include/c++/$gccVersion",
    "$mingwRoot/include/c++/$gccVersion/$gccTriple",
    "$mingwRoot/include/c++/$gccVersion/backward",
    "$mingwRoot/include",
    "$mingwRoot/lib/gcc/$gccTriple/$gccVersion/include"
)

# Match Rack plugin.mk / compile.mk flags for Windows MinGW (IntelliSense only).
$commonArgs = @(
    "-std=c++11",
    "--target=$gccTriple",
    "-march=nehalem",
    "-D_USE_MATH_DEFINES",
    "-DARCH_WIN=1",
    "-fPIC",
    "-g",
    "-Wno-unused-parameter",
    "-Wno-vla-extension",
    "-ferror-limit=0"
) + @(
    "-I./src",
    "-I./src/ctrl",
    "-I$rackDir/include",
    "-I$rackDir/dep/include"
) + ($systemIncludes | ForEach-Object { "-isystem"; $_ })

function New-CompileEntry([string]$file, [string[]]$extraArgs) {
    @{
        directory = $dir
        arguments = @($compiler) + $commonArgs + $extraArgs + @($file)
        file = $file
    }
}

$items = @()
Get-ChildItem "src\*.cpp" | ForEach-Object {
    $rel = "src/$($_.Name)"
    $items += New-CompileEntry $rel @("-c")
}

# Extra header units so clangd can index shared declarations (e.g. InfNoiseModule helpers).
@(
    "src/plugin.hpp",
    "src/inUtil.hpp",
    "src/inComponents.hpp"
) | ForEach-Object {
    $items += New-CompileEntry $_ @("-xc++")
}

$entries = $items | ForEach-Object { $_ | ConvertTo-Json -Compress }
'[' + ($entries -join ',') + ']' | Set-Content -Encoding utf8 "compile_commands.json"

$clangdArgs = $commonArgs | ForEach-Object {
    if ($_ -match '^-isystem$') { "    - -isystem" } else { "    - $_" }
}
# Expand -isystem pairs for .clangd yaml
$clangdYamlArgs = @()
for ($i = 0; $i -lt $commonArgs.Count; $i++) {
    if ($commonArgs[$i] -eq '-isystem') {
        $clangdYamlArgs += "    - -isystem"
        $clangdYamlArgs += "    - $($commonArgs[$i + 1])"
        $i++
    } else {
        $clangdYamlArgs += "    - $($commonArgs[$i])"
    }
}

@"
CompileFlags:
  Compiler: $compiler
  Add:
$($clangdYamlArgs -join "`n")

Index:
  Background: Build

Diagnostics:
  UnusedIncludes: None
  Suppress:
    - '*'
"@ | Set-Content -Encoding utf8 ".clangd"

Write-Host "Wrote compile_commands.json ($($items.Count) entries)"
Write-Host "Wrote .clangd (compiler=$compiler, gcc=$gccVersion, RACK_DIR=$rackDir)"
