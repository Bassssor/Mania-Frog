"""Replace only the mascot's browser source with official native input-overlay."""
import json
from pathlib import Path
from ObsControl import Obs

ROOT = Path(__file__).resolve().parents[1]
SCENE = '奶蛙打音游'
NAME = '奶蛙 · DFJK 输入叠加'
BACKUP = NAME + '（网页备份）'
FILTER = '奶蛙 DFJK · 原生动作与粒子'


def main():
    folder = ROOT.parent / 'obs-input-overlay' if ROOT.name == 'source' else ROOT / 'dist/input-overlay'
    (ROOT / 'build/obs-native').mkdir(parents=True, exist_ok=True)
    for file in ['milk-frog.png', 'milk-frog.json', 'background-atlas.png', 'foreground-atlas.png']:
        if not (folder / file).is_file():
            raise RuntimeError(f'Missing native asset: {file}')
    client = Obs()
    try:
        if client.call('GetStreamStatus')['outputActive'] or client.call('GetRecordStatus')['outputActive']:
            raise RuntimeError('An output is running; conversion has not changed it.')
        kinds = client.call('GetInputKindList')['inputKinds']
        if 'input-overlay' not in kinds:
            raise RuntimeError('Official input-overlay source type unavailable')
        # Confirm the native filter is loaded before hiding/replacing anything.
        client.call('GetSourceFilterDefaultSettings', {'filterKind': 'milk_frog_native_animation'})
        inputs = client.call('GetInputList')['inputs']
        old = next((i for i in inputs if i['inputName'] == NAME), None)
        items = client.call('GetSceneItemList', {'sceneName': SCENE})['sceneItems']
        item = next((i for i in items if i['sourceName'] == NAME), None)
        transform = None
        if item:
            transform = client.call('GetSceneItemTransform', {'sceneName': SCENE, 'sceneItemId': item['sceneItemId']})['sceneItemTransform']
            (ROOT / 'build/obs-native/migration-transform.json').write_text(
                json.dumps(transform, ensure_ascii=False, indent=2), encoding='utf-8')
        if old and old['unversionedInputKind'] == 'browser_source':
            if any(i['inputName'] == BACKUP for i in inputs):
                raise RuntimeError('Backup name already used; no source was replaced')
            client.call('SetInputSettings', {'inputName': NAME, 'inputSettings': {'shutdown': True}, 'overlay': True})
            if item:
                client.call('SetSceneItemEnabled', {'sceneName': SCENE, 'sceneItemId': item['sceneItemId'], 'sceneItemEnabled': False})
            client.call('SetInputName', {'inputName': NAME, 'newInputName': BACKUP})
            old = None
        elif old and old['unversionedInputKind'] != 'input-overlay':
            raise RuntimeError('Mascot source name is occupied by another source type')
        settings = {'io.overlay_image': str(folder / 'milk-frog.png'),
                    'io.layout_file': str(folder / 'milk-frog.json'),
                    'io.input_source': '', 'linear_alpha': False}
        if not old:
            item_id = client.call('CreateInput', {'sceneName': SCENE, 'inputName': NAME,
                'inputKind': 'input-overlay', 'inputSettings': settings, 'sceneItemEnabled': True})['sceneItemId']
        else:
            client.call('SetInputSettings', {'inputName': NAME, 'inputSettings': settings, 'overlay': True})
            item_id = item['sceneItemId']
        filters = client.call('GetSourceFilterList', {'sourceName': NAME})['filters']
        filter_settings = {'config_file': str(folder / 'milk-frog.json'), 'particles': True}
        if not any(f['filterName'] == FILTER for f in filters):
            client.call('CreateSourceFilter', {'sourceName': NAME, 'filterName': FILTER,
                'filterKind': 'milk_frog_native_animation', 'filterSettings': filter_settings})
        else:
            client.call('SetSourceFilterSettings', {'sourceName': NAME, 'filterName': FILTER, 'filterSettings': filter_settings})
        if transform is None:
            video = client.call('GetVideoSettings')
            scale = 400 / 808
            transform = {'positionX': video['baseWidth'] - 420, 'positionY': video['baseHeight'] - 20 - 712 * scale,
                'scaleX': scale, 'scaleY': scale, 'alignment': 5, 'rotation': 0, 'boundsType': 'OBS_BOUNDS_NONE',
                'cropLeft': 36, 'cropRight': 36, 'cropTop': 0, 'cropBottom': 168}
        transform = {k: v for k, v in transform.items() if k in {
            'positionX', 'positionY', 'rotation', 'scaleX', 'scaleY', 'alignment',
            'boundsType', 'cropLeft', 'cropRight', 'cropTop', 'cropBottom'}}
        client.call('SetSceneItemTransform', {'sceneName': SCENE, 'sceneItemId': item_id, 'sceneItemTransform': transform})
        client.call('SetSceneItemLocked', {'sceneName': SCENE, 'sceneItemId': item_id, 'sceneItemLocked': True})
        current_items = client.call('GetSceneItemList', {'sceneName': SCENE})['sceneItems']
        client.call('SetSceneItemIndex', {'sceneName': SCENE, 'sceneItemId': item_id, 'sceneItemIndex': len(current_items) - 1})
        actual = client.call('GetInputSettings', {'inputName': NAME})
        if actual['inputKind'] != 'input-overlay':
            raise AssertionError('Installed source is not native input-overlay')
        audio_name='奶蛙 · F5 笑声'
        audio_settings={'audio_file':str(folder/'laugh.wav')}
        if not any(i['inputName']==audio_name for i in inputs):
            client.call('CreateInput',{'sceneName':SCENE,'inputName':audio_name,
                'inputKind':'milk_frog_laugh_audio','inputSettings':audio_settings,'sceneItemEnabled':True})
        else:
            client.call('SetInputSettings',{'inputName':audio_name,'inputSettings':audio_settings,'overlay':True})
        client.call('SetInputAudioMonitorType',{'inputName':audio_name,'monitorType':'OBS_MONITORING_TYPE_NONE'})
        report = {'source': NAME, 'scene': SCENE, 'sceneItemId': item_id, 'settings': actual,
                  'filters': client.call('GetSourceFilterList', {'sourceName': NAME})['filters'],
                  'transform': client.call('GetSceneItemTransform', {'sceneName': SCENE, 'sceneItemId': item_id}),
                  'browserBackupHidden': True}
        (ROOT / 'build/obs-native/installation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({'nativeSource': actual['inputKind'], 'scene': SCENE, 'source': NAME,
                          'animations': 16, 'browserBackupHidden': True}, ensure_ascii=False))
    finally:
        client.close()


if __name__ == '__main__':
    main()
