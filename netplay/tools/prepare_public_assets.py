from pathlib import Path
import json,sys,re
r=Path(__file__).resolve().parents[2];j=json.loads((r/'release_identity.json').read_text());out=r/'out/N17Assets';out.mkdir(parents=True,exist_ok=True)
pin=Path(sys.argv[1]) if len(sys.argv)>1 else r/'config/server-public-key.hex';key=pin.read_text().strip()
if not re.fullmatch('[0-9a-fA-F]{64}',key) or not any(bytes.fromhex(key)):raise ValueError('Expected32-byte server PUBLIC key')
assert j['xbox_title_id']=='54413045' and j['xbox_save_id']=='00000001' and j['android_application_id']=='org.dinoblockstacker.app','Permanent release identity must not change with a version update'
for name in ['PlayN22PublicLogged','PlayN22PublicNoLog']:
 c=json.loads((r/'rxdk.project.json').read_text())['configurations'][name];assert c['imageBuild']['testId'].lower()=='0x'+j['xbox_title_id'].lower();assert c['imageBuild']['testName']==j['name']
header='#pragma once\n/* Public endpoint/pin only. User ROM data is prepared at runtime. */\n#define TA_N17_SERVER '+json.dumps(j['server'])+'\n#define TA_N17_PORT '+str(j['server_udp_port'])+'u\nstatic const unsigned char ta_n17_account_key[]={'+','.join(str(x) for x in bytes.fromhex(key))+'};\nstatic const unsigned char ta_n17_start_cache[]={0};\n'
(out/'ta_n17_assets.h').write_text(header,encoding='ascii',newline='\n');(out/'public_manifest.json').write_text(json.dumps({'ROM_bundled':False,'ROM_derived_assets_bundled':False,'identity':j},indent=2)+'\n');print('Public release assets prepared: no ROM/checkpoint/portraits embedded.')
