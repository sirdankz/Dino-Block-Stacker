[CmdletBinding()]
param([string]$OutDir='', [string]$RomPath='', [string]$AccountKeyPath='', [string]$SdkPath='', [string]$XexToolPath='')
$ErrorActionPreference='Stop'
$python='C:\Python314\python.exe'
if(!(Test-Path -LiteralPath $python)){$python=(Get-Command python -ErrorAction Stop).Source}
$buildArguments=@((Join-Path $PSScriptRoot 'xbox360_port\build_360.py'))
if($OutDir){$buildArguments+=@('--out',$OutDir)}
if($RomPath){$buildArguments+=@('--rom',$RomPath)}
if($AccountKeyPath){$buildArguments+=@('--public-key',$AccountKeyPath)}
if($SdkPath){$buildArguments+=@('--sdk',$SdkPath)}
if($XexToolPath){$buildArguments+=@('--xextool',$XexToolPath)}
& $python @buildArguments
if($LASTEXITCODE -ne 0){throw 'Xbox 360 build failed; see the build output for the complete diagnostics.'}
