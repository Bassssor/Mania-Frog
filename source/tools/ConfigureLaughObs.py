"""Update only the mascot's native plugin and paired audio while OBS is closed."""
import copy,json,os,shutil,sys,uuid,subprocess
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parents[1]
def main():
    running=subprocess.check_output(['tasklist','/FI','IMAGENAME eq obs64.exe','/FO','CSV'],text=True,errors='replace')
    if 'obs64.exe' in running.lower():raise RuntimeError('Close OBS before updating its plugin or scene file.')
    folder=ROOT.parent/'obs-input-overlay'
    config=Path(os.environ['APPDATA'])/'obs-studio/basic/scenes/未命名.json'
    backup=ROOT.parent/'verification/laugh-playback/scene-before.json'
    backup.parent.mkdir(parents=True,exist_ok=True)
    data=json.loads(config.read_text(encoding='utf-8-sig'))
    scene=next(s for s in data['sources'] if s.get('name')=='奶蛙打音游' and s['id']=='scene')
    native=next(s for s in data['sources'] if s.get('name')=='奶蛙 · DFJK 输入叠加')
    assert native['id']=='input-overlay'
    assert any(f['id']=='milk_frog_native_animation' for f in native.get('filters',[]))
    items=scene['settings']['items'];before=copy.deepcopy(items)
    audio_name='奶蛙 · F5 笑声'
    audio=next((s for s in data['sources'] if s.get('name')==audio_name),None)
    if audio is None:
        audio=copy.deepcopy(native)
        audio.update(name=audio_name,uuid=str(uuid.uuid4()),id='milk_frog_laugh_audio',versioned_id='milk_frog_laugh_audio',mixers=255)
        audio.pop('filters',None)
        data['sources'].append(audio)
    audio.update(settings={'audio_file':str(folder/'laugh.wav')},monitoring_type=0,muted=False,volume=1.0)
    if not any(item['source_uuid']==audio['uuid'] for item in items):
        item=copy.deepcopy(next(i for i in items if i['source_uuid']==native['uuid']))
        new_id=max(scene['settings']['id_counter'],*(i['id'] for i in items))+1
        item.update(name=audio_name,source_uuid=audio['uuid'],id=new_id,visible=True,locked=True,
                    crop_left=0,crop_right=0,crop_top=0,crop_bottom=0,pos={'x':0.0,'y':0.0},scale={'x':1.0,'y':1.0})
        items.append(item);scene['settings']['id_counter']=new_id
    assert items[:len(before)]==before,'Existing sources or transforms changed'
    if not backup.exists():shutil.copyfile(config,backup)
    temp=config.with_suffix('.json.tmp')
    temp.write_text(json.dumps(data,ensure_ascii=False,indent=4),encoding='utf-8');temp.replace(config)
    # The official source and its transform are kept exactly as they were.
    shutil.copyfile(folder/'milk-frog-native.dll',r'C:\Program Files\obs-studio\obs-plugins\64bit\milk-frog-native.dll')
    print('Configured native input-overlay + unmonitored laugh audio; existing scene items unchanged.')
if __name__=='__main__':main()
