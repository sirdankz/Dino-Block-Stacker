[CmdletBinding()]
param([string]$OutDir='',[string]$RomPath='',[ValidateRange(0,64)][int]$Jobs=0)
$ErrorActionPreference='Stop'
$scriptRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$pythonCommand=Get-Command py -ErrorAction SilentlyContinue
$prefix=@()
if($pythonCommand){$pythonExe=$pythonCommand.Source;$prefix=@('-3')}else{
 $pythonCommand=Get-Command python -ErrorAction SilentlyContinue
 if($pythonCommand){$pythonExe=$pythonCommand.Source}elseif(Test-Path -LiteralPath 'C:\Python314\python.exe'){$pythonExe='C:\Python314\python.exe'}else{throw 'Python 3.10 or newer is required.'}
}
$arguments=$prefix+@('-X','utf8',(Join-Path $scriptRoot 'pc_port\build_pc.py'),'--jobs',[string]$Jobs)
if($OutDir){$arguments+=@('--out-dir',$OutDir)}
if($RomPath){$arguments+=@('--rom',$RomPath)}
& $pythonExe @arguments
if($LASTEXITCODE -ne 0){throw 'Windows PC build failed; review the diagnostics above.'}
