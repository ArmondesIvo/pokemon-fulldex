from pathlib import Path
import os,sys,subprocess,zipfile,tarfile,json,struct,time
asset=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)
stage=out/'extracted';stage.mkdir(exist_ok=True)
if asset.suffix=='.zip':
 with zipfile.ZipFile(asset) as z:
  for m in z.infolist():
   p=(stage/m.filename).resolve();assert p==stage or stage in p.parents
   z.extract(m,stage)
   if m.external_attr>>16 and not m.is_dir():p.chmod((m.external_attr>>16)&0o777)
elif asset.name.endswith('.tar.gz'):
 with tarfile.open(asset) as t:t.extractall(stage,filter='data')
if sys.platform=='win32':exe=next(stage.rglob('Pokemon Fulldex.exe'))
elif sys.platform=='darwin':exe=next(stage.rglob('Contents/MacOS/pokemon-ds-wide'))
else:exe=next(p for p in stage.rglob('pokemon-ds-wide') if p.is_file())
if os.name!='nt':exe.chmod(exe.stat().st_mode|0o111)
shot=out/'launcher.bmp';env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SSR_LIST_SHOT=str(shot),HOME=str(out/'home'),XDG_DATA_HOME=str(out/'xdg'),SSR_UPDATE_TEST='1')
Path(env['HOME']).mkdir(exist_ok=True)
tick=time.monotonic();p=subprocess.run([str(exe)],cwd=exe.parent,env=env,capture_output=True,text=True,timeout=45)
(out/'process-output.log').write_text(p.stdout+p.stderr,encoding='utf-8')
assert p.returncode==0,p.returncode
assert shot.exists(),'No launcher screenshot produced'
b=shot.read_bytes();assert b[:2]==b'BM';offset=struct.unpack_from('<I',b,10)[0];width,height=struct.unpack_from('<ii',b,18)
assert len(set(b[offset:]))>4,'Launcher canvas is a uniform colour'
result={'asset':asset.name,'exit':p.returncode,'seconds':round(time.monotonic()-tick,2),'width':width,'height':height,'test':'stock release launcher with empty library, SDL dummy video driver; CPU canvas screenshot'}
(out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
