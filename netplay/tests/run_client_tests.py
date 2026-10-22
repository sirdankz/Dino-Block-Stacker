"""Build and run native client checks. Requires Python 3.10+ and a C compiler.
Windows: installed RXDK clang, Visual Studio C++ Build Tools and Windows SDK.
Linux/macOS: CC (or clang/cc) on PATH. No ROM, console or network service needed.
"""
import os,pathlib,shutil,subprocess,sys,tempfile
root=pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='dino-party-test-') as tmp:
    target=pathlib.Path(tmp)/('party_test.dll' if os.name=='nt' else 'party_test.so')
    compiler=os.environ.get('CC') or shutil.which('clang') or shutil.which('cc')
    if not compiler and os.name=='nt':compiler=r'C:\ProgramData\RXDK\llvm\xboxog-windows-x64\bin\clang.exe'
    if not compiler:raise SystemExit('Set CC to your C compiler.')
    args=[compiler,'-shared','-O2','-Wno-deprecated-declarations']
    if os.name=='nt':
        vcroot=pathlib.Path(os.environ.get('VCToolsInstallDir',r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC'))
        vc=vcroot if (vcroot/'include').exists() else sorted(vcroot.glob('*'))[-1]
        sdk=pathlib.Path(os.environ.get('WindowsSdkDir',r'C:\Program Files (x86)\Windows Kits\10'))
        version=sorted(p.name for p in (sdk/'Lib').glob('*') if (p/'ucrt/x64/ucrt.lib').exists())[-1]
        args+=['--target=x86_64-pc-windows-msvc','--ld-path='+str(vc/'bin/Hostx64/x64/link.exe'),'-I'+str(vc/'include'),'-L'+str(vc/'lib/x64')]
        args+=['-I'+str(sdk/'Include'/version/x) for x in ['ucrt','um','shared']]
        args+=['-L'+str(sdk/'Lib'/version/x/'x64') for x in ['ucrt','um']]
    else:args+=['-fPIC']
    args+=['-DTA_VS_REMATCH=1','-DTA_VS_PREPARED_PICTURE=1','-I'+str(root/'netplay/src'),'-I'+str(root/'third_party/lakesnes/snes')]
    args+=[str(root/'netplay/tests/party_native_test.c')]+[str(root/'netplay/src'/f'{n}.c') for n in ['ta_vs_session','ta_vs_attack','ta_vs_party','ta_vs_picture','ta_vs_lan_screen','ta_vs_local_game','ta_vs_board_rng','ta_vs_board_rows']]+['-o',str(target)]
    subprocess.run(args,check=True)
    geometry=pathlib.Path(tmp)/('layout_test.exe' if os.name=='nt' else 'layout_test')
    source_at=args.index(str(root/'netplay/tests/party_native_test.c'))
    geometry_args=[x for x in args[:source_at] if x not in {'-shared','-fPIC'}]+[str(root/'netplay/tests/test_party_layout.c'),'-o',str(geometry)]
    subprocess.run(geometry_args,check=True);subprocess.run([str(geometry)],check=True)
    presence=pathlib.Path(tmp)/('presence_test.exe' if os.name=='nt' else 'presence_test')
    presence_args=[x for x in args[:source_at] if x not in {'-shared','-fPIC'}]+[str(root/'netplay/tests/test_presence.c'),str(root/'netplay/src/ta_vs_public.c'),str(root/'netplay/src/ta_vs_account.c'),str(root/'third_party/monocypher/monocypher.c'),'-o',str(presence)]
    subprocess.run(presence_args,check=True);subprocess.run([str(presence)],check=True)
    controls=pathlib.Path(tmp)/('controls_test.exe' if os.name=='nt' else 'controls_test')
    control_args=[x for x in args[:source_at] if x not in {'-shared','-fPIC'}]+[str(root/'netplay/tests/test_controls.c'),'-o',str(controls)]
    subprocess.run(control_args,check=True);subprocess.run([str(controls)],check=True)
    mobile=pathlib.Path(tmp)/('mobile_view_test.exe' if os.name=='nt' else 'mobile_view_test')
    mobile_args=[x for x in args[:source_at] if x not in {'-shared','-fPIC'}]+[str(root/'netplay/tests/test_mobile_view.c'),'-o',str(mobile)]
    subprocess.run(mobile_args,check=True);subprocess.run([str(mobile)],check=True)
    print("Native client layout, presence, controls and mobile-view checks passed.")
