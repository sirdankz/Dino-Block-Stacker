[CmdletBinding()]
param([string]$SdkRoot='', [string]$GradleRoot='', [string]$JdkRoot='', [string]$AccountKeyPath='', [switch]$Offline, [switch]$InstallApk, [string]$SigningStore='', [string]$SigningPasswordFile='', [switch]$SkipLint)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(!$SdkRoot){foreach($p in @($env:ANDROID_HOME,$env:ANDROID_SDK_ROOT,'C:\SHSDK',(Join-Path $env:LOCALAPPDATA 'Android\Sdk'))){if($p -and (Test-Path -LiteralPath (Join-Path $p 'platform-tools\adb.exe'))){$SdkRoot=$p;break}}}
if(!$SdkRoot){throw 'Android SDK missing: pass -SdkRoot with SDK 34, build-tools 34.0.0, NDK 27.3.13750724 and CMake 3.22.1.'}
foreach($n in @('platforms\android-34\android.jar','build-tools\34.0.0\aapt2.exe','ndk\27.3.13750724\source.properties','cmake\3.22.1\bin\cmake.exe')){if(!(Test-Path -LiteralPath (Join-Path $SdkRoot $n))){throw "Missing Android SDK package: $n"}}
if(!$JdkRoot){$JdkRoot=$env:JAVA_HOME};if(!$JdkRoot){$jdk=Get-ChildItem 'C:\Program Files\Microsoft' -Directory -Filter 'jdk*' -ErrorAction SilentlyContinue|Select-Object -Last 1;if($jdk){$JdkRoot=$jdk.FullName}}
if(!$JdkRoot -or !(Test-Path -LiteralPath (Join-Path $JdkRoot 'bin\javac.exe'))){throw 'JDK 17/21 missing: pass -JdkRoot.'}
if(!$GradleRoot){foreach($p in @($env:GRADLE_HOME,'C:\Android\gradle-8.9',(Join-Path $env:LOCALAPPDATA 'SilentHillAndroidBuild\gradle-8.9'),(Join-Path $env:LOCALAPPDATA 'Gradle\gradle-8.9'))){if($p -and (Test-Path -LiteralPath (Join-Path $p 'bin\gradle.bat'))){$GradleRoot=$p;break}}}
if(!$GradleRoot -or !(Test-Path -LiteralPath (Join-Path $GradleRoot 'bin\gradle.bat'))){throw 'Gradle 8.9 missing: pass -GradleRoot.'}
if(!$AccountKeyPath){$AccountKeyPath=Join-Path $root 'config\server-public-key.hex'}
$python='C:\Python314\python.exe';if(!(Test-Path -LiteralPath $python)){$python=(Get-Command python -ErrorAction Stop).Source}
& $python (Join-Path $root 'android_port\tools\prepare_android.py') $AccountKeyPath
if($LASTEXITCODE -ne 0){throw 'Android adapter/pinned PUBLIC key preparation failed.'}
$env:JAVA_HOME=$JdkRoot;$env:ANDROID_HOME=$SdkRoot;$env:ANDROID_SDK_ROOT=$SdkRoot;$env:Path="$(Join-Path $JdkRoot 'bin');$env:Path"
[IO.File]::WriteAllText((Join-Path $root 'android_port\local.properties'),('sdk.dir='+$SdkRoot.Replace('\','/')+"`n"),[Text.Encoding]::ASCII)
if(!$SigningStore){$SigningStore=Join-Path $env:USERPROFILE '.android\dinoblockstacker-release.jks'}
if(!$SigningPasswordFile){$SigningPasswordFile=Join-Path $env:USERPROFILE '.android\dinoblockstacker-release.pass'}
New-Item -ItemType Directory -Force (Split-Path -Parent $SigningStore)|Out-Null
if(!(Test-Path -LiteralPath $SigningStore)){
 if(!(Test-Path -LiteralPath $SigningPasswordFile)){$rng=[Security.Cryptography.RandomNumberGenerator]::Create();$bytes=New-Object byte[] 36;try{$rng.GetBytes($bytes)}finally{$rng.Dispose()};[IO.File]::WriteAllText($SigningPasswordFile,[Convert]::ToBase64String($bytes),[Text.Encoding]::ASCII)}
 & (Join-Path $JdkRoot 'bin\keytool.exe') -genkeypair -keystore $SigningStore -storetype PKCS12 -storepass:file $SigningPasswordFile -keypass:file $SigningPasswordFile -alias dinoblockstacker -keyalg RSA -keysize 3072 -validity 10000 -dname 'CN=Dino Block Stacker'
 if($LASTEXITCODE -ne 0){throw 'Release signing key creation failed'}
}
if(!(Test-Path -LiteralPath $SigningPasswordFile)){throw 'Signing password file missing; preserve the existing signing key'}
$env:DBS_SIGNING_STORE_FILE=(Resolve-Path -LiteralPath $SigningStore).Path;$env:DBS_SIGNING_STORE_PASSWORD=[IO.File]::ReadAllText($SigningPasswordFile).Trim()
New-Item -ItemType Directory -Force (Join-Path $root 'out\DinoBlockStackerAndroid')|Out-Null
$args=@('--no-daemon',':app:assembleRelease');if($SkipLint){$args+=@('-x',':app:lintVitalAnalyzeRelease','-x',':app:lintVitalReportRelease','-x',':app:lintVitalRelease')};if($Offline){$args=@('--offline')+$args}
Push-Location (Join-Path $root 'android_port')
try{& (Join-Path $GradleRoot 'bin\gradle.bat') @args;if($LASTEXITCODE -ne 0){throw 'Android APK build failed.'}}finally{Pop-Location;Remove-Item Env:DBS_SIGNING_STORE_PASSWORD -ErrorAction SilentlyContinue;Remove-Item Env:DBS_SIGNING_STORE_FILE -ErrorAction SilentlyContinue}
$apk=Join-Path $root 'android_port\app\build\outputs\apk\release\app-release.apk';$buildVersion=(Get-Content -LiteralPath (Join-Path $root 'release_identity.json') -Raw|ConvertFrom-Json).version;$dest=Join-Path $root ('out\DinoBlockStackerAndroid\DinoBlockStacker-'+$buildVersion+'.apk');Copy-Item -LiteralPath $apk -Destination $dest -Force
& (Join-Path $SdkRoot 'build-tools\34.0.0\apksigner.bat') verify --verbose $dest
if($LASTEXITCODE -ne 0){throw 'APK signature verification failed.'}
Write-Host "APK READY: $dest" -ForegroundColor Green
Write-Host 'Dino Block Stacker: no ROM included. Import your compatible ROM. Keep the release signing key/password file private for all future updates.' -ForegroundColor Cyan
if($InstallApk){& (Join-Path $SdkRoot 'platform-tools\adb.exe') install -r $dest;if($LASTEXITCODE -ne 0){throw 'ADB install failed.'}}
