[CmdletBinding()]
param([ValidateSet('PlayN22PublicLogged','PlayN22PublicNoLog')][string]$Configuration='PlayN22PublicNoLog',[string]$AccountKeyPath='',[string]$RomPath='', [switch]$MakeIso)
$ErrorActionPreference='Stop';$root=$PSScriptRoot
$python='C:\Python314\python.exe';if(!(Test-Path -LiteralPath $python)){$python=(Get-Command python -ErrorAction Stop).Source}
if(!$AccountKeyPath){$AccountKeyPath=Join-Path $root 'config\server-public-key.hex'}
& $python (Join-Path $root 'netplay\tools\prepare_public_assets.py') $AccountKeyPath;if($LASTEXITCODE -ne 0){throw 'Public asset preparation failed'}
& $python (Join-Path $root 'tools\prepare_music.py') --out (Join-Path $root 'out\Music');if($LASTEXITCODE -ne 0){throw 'Music preparation failed'}
$projectName=(Get-Content -LiteralPath (Join-Path $root 'rxdk.project.json') -Raw|ConvertFrom-Json).name
$cli='C:\ProgramData\RXDK\tools\Rxdk.Cli.exe'
& $cli build --project-root $root --configuration $Configuration
if($LASTEXITCODE -ne 0){throw 'RXDK build failed'}
$folder=Join-Path $root ('out\DinoBlockStackerXbox\'+$Configuration);New-Item -ItemType Directory -Force $folder|Out-Null
Copy-Item -LiteralPath (Join-Path $root ('out\'+$Configuration+'\'+$projectName+'.xbe')) -Destination (Join-Path $folder 'default.xbe') -Force
New-Item -ItemType Directory -Force -Path (Join-Path $folder 'music')|Out-Null
foreach($musicFile in @('menu.wav','results.wav','manifest.json','LICENSE-MUSIC.txt')){Copy-Item -LiteralPath (Join-Path $root ('out\Music\'+$musicFile)) -Destination (Join-Path $folder ('music\'+$musicFile)) -Force}
Copy-Item -LiteralPath (Join-Path $root 'LICENSE-MUSIC.txt') -Destination (Join-Path $folder 'LICENSE-MUSIC.txt') -Force
if($RomPath){$rom=(Resolve-Path -LiteralPath $RomPath).Path;if((Get-FileHash -LiteralPath $rom -Algorithm SHA256).Hash -ne '579bf1a1988d4af06a69cc1d82a2478ebe51940c5ced7f97e83029a24e6aa778'){throw 'Incompatible ROM'};Copy-Item -LiteralPath $rom -Destination (Join-Path $folder 'game.sfc') -Force}
if($MakeIso){if(!$RomPath){throw 'ISO creation requires your own -RomPath; do not distribute the resulting ISO'};& 'C:\ProgramData\RXDK\tools\xdvdfs.exe' pack $folder (Join-Path $root 'out\DinoBlockStacker-private.iso');if($LASTEXITCODE -ne 0){throw 'ISO build failed'}}
Write-Host "Dino Block Stacker XBE ready: $folder" -ForegroundColor Green
Write-Host 'Public XBE has no ROM or ROM-derived cache. User adds game.sfc beside default.xbe.'
