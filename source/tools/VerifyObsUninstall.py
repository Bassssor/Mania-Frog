"""Exercise uninstall discovery and removal in disposable OBS configurations."""
import base64
import copy
import json
import os
from pathlib import Path
import subprocess
import uuid


def verify_uninstall(output):
    profile = output / ('uninstall-' + uuid.uuid4().hex[:8])
    config = profile / 'AppData/obs-studio'
    scenes = config / 'basic/scenes'
    scenes.mkdir(parents=True)
    plugin = profile / 'ProgramData/obs-studio/plugins/milk-frog-easy'
    legacy = config / 'plugins/milk-frog-easy'
    desktop = profile / 'LocalAppData/MilkFrog/config.txt'
    desktop.parent.mkdir(parents=True)
    desktop.write_text('shared desktop settings', encoding='utf-8')
    unrelated_plugin = plugin.parent / 'other-plugin/keep.txt'
    unrelated_plugin.parent.mkdir(parents=True)
    unrelated_plugin.write_text('keep', encoding='utf-8')

    def item(name, source_uuid, item_id):
        return {'name': name, 'source_uuid': source_uuid, 'id': item_id,
                'visible': False, 'pos': {'x': 123.25, 'y': 234.5}}

    fixture = {'name': '集合甲', 'modules': {'unrelated': [1, 'keep', False]},
               'sources': [
                   {'name': '主场景', 'uuid': 'scene-a', 'id': 'scene', 'settings': {'items': [
                       item('相机', 'camera', 1), item('旧显示名称', 'shared', 2),
                       item('分组', 'group-a', 3),
                       dict(item('共享奶蛙', 'shared', 4), group_item_backup=True),
                       item('嵌套场景', 'scene-b', 5), item('共享奶蛙', 'foreign', 6)]}},
                   {'name': '嵌套场景', 'uuid': 'scene-b', 'id': 'scene', 'settings': {'items': [
                       item('共享奶蛙', 'shared', 7), {'name': '笑声音频', 'id': 8}]}},
                   {'name': '无关场景', 'id': 'scene', 'settings': {'items': []}},
                   {'name': '共享奶蛙', 'uuid': 'shared', 'id': 'milk_frog_source'},
                   {'name': '保留奶蛙', 'uuid': 'keep', 'id': 'milk_frog_source'},
                   {'name': '未使用奶蛙', 'uuid': 'orphan', 'id': 'milk_frog_source'},
                   {'name': '奶蛙', 'uuid': 'foreign', 'id': 'input-overlay',
                    'settings': {'io.layout_file': 'unrelated.json'}},
                   {'name': '相机', 'uuid': 'camera', 'id': 'dshow_input', 'filters': [
                       {'name': '保留色彩', 'id': 'color_filter', 'settings': {'contrast': 0.25}},
                       {'name': '奶蛙滤镜', 'id': 'milk_frog_native_animation'}]},
                   {'name': '笑声音频', 'uuid': 'audio', 'id': 'milk_frog_laugh_audio'}],
               'groups': [{'name': '分组', 'uuid': 'group-a', 'id': 'group', 'settings': {'items': [
                   item('共享奶蛙', 'shared', 9), item('保留奶蛙', 'keep', 10),
                   item('相机', 'camera', 11)]}}]}
    scene_file = scenes / 'a.json'
    scene_file.write_text(json.dumps(fixture, ensure_ascii=False), encoding='utf-8')
    other_file = scenes / 'b.json'
    other_file.write_text(json.dumps({'name': '集合乙', 'sources': [
        {'name': '另一个场景', 'id': 'scene', 'settings': {'items': [{'name': '共享奶蛙'}]}},
        {'name': '共享奶蛙', 'id': 'milk_frog_source'}]}), encoding='utf-8')
    before, other_before = scene_file.read_bytes(), other_file.read_bytes()
    for folder in (plugin, legacy):
        (folder / 'data/requests').mkdir(parents=True)
        (folder / 'data/keybindings.json').write_text('{"custom":true}', encoding='utf-8')
        (folder / 'milk-frog-easy.dll').write_bytes(b'fixture only')
        (folder / 'data/requests/shared.json').write_text(json.dumps({
            'collection': '集合甲', 'scene': '主场景',
            'source': '共享奶蛙'}), encoding='utf-8')
    pending = plugin / 'data/requests/pending.json'
    pending.write_text(json.dumps({'action': 'Add', 'collection': '集合甲',
                                  'scene': '主场景', 'source': '待安装奶蛙'}), encoding='utf-8')
    wrapper = profile / 'run.ps1'
    wrapper.write_text(
        'param($Installer,$ArgumentsBase64)\n'
        '$options=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($ArgumentsBase64)) | ConvertFrom-Json\n'
        '$global:milkFrogTestRunning=[bool]$options.TestObsRunning\n'
        'function Get-Process {param($Name,$ErrorAction);if($global:milkFrogTestRunning){[pscustomobject]@{Path=$env:TEST_OBS_EXE;Id=42}}}\n'
        'function Get-CimInstance {param($ClassName,$Filter);[pscustomobject]@{CommandLine=""}}\n'
        'if($options.TestDialog){\n'
        '  $ErrorActionPreference="Stop";$utf8=[Text.UTF8Encoding]::new($false);[Console]::OutputEncoding=$utf8\n'
        '  $ConfigRoot=Join-Path $env:APPDATA "obs-studio";$plugin=Join-Path $env:PROGRAMDATA "obs-studio/plugins/milk-frog-easy"\n'
        '  $tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($Installer,[ref]$tokens,[ref]$errors)\n'
        '  if($errors.Count){throw ($errors | Out-String)}\n'
        '  foreach($definition in $ast.EndBlock.Statements | Where-Object {$_ -is [Management.Automation.Language.FunctionDefinitionAst]}){\n'
        '    $code=$definition.Extent.Text.Replace(\'[void]$dialog.ShowDialog($owner)\',\'return $dialog\').Replace(\'[void]$form.ShowDialog()\',\'return $form\')\n'
        '    Invoke-Expression $code\n'
        '  }\n'
        '  $owner=Show-Installer;$dialog=Show-Uninstaller $owner\n'
        '  $views=@($dialog.Controls | Where-Object {$_ -is [Windows.Forms.ListView]})\n'
        '  $sources=$views | Where-Object {$_.CheckBoxes};$files=$views | Where-Object {-not $_.CheckBoxes}\n'
        '  $answer=[pscustomobject]@{windowsShown=($owner.Visible -or $dialog.Visible);sourceRows=$sources.Items.Count;fileRows=@($files.Items | ForEach-Object {[pscustomobject]@{kind=$_.Text;size=$_.SubItems[1].Text;path=$_.SubItems[2].Text}})}\n'
        '  $dialog.Dispose();$owner.Dispose();$answer | ConvertTo-Json -Depth 5 -Compress;exit 0\n'
        '}\n'
        '$parameters=@{};foreach($property in $options.PSObject.Properties){if($property.Name -notin @("TestObsRunning","TestDialog")){$parameters[$property.Name]=$property.Value}}\n'
        '& $Installer @parameters\nexit $LASTEXITCODE\n', encoding='utf-8-sig')
    installer = Path(__file__).resolve().parents[2] / 'obs-input-overlay/InstallObsEasy.ps1'
    env = {key: value for key, value in os.environ.items()
           if key.upper() not in ('APPDATA', 'PROGRAMDATA', 'LOCALAPPDATA')}
    env.update(APPDATA=str(profile / 'AppData'), PROGRAMDATA=str(profile / 'ProgramData'),
               LOCALAPPDATA=str(profile / 'LocalAppData'), TEST_OBS_EXE=str(profile / 'OBS/bin/64bit/obs64.exe'))

    def run(success=True, **options):
        options.update(Quiet=True, Json=True)
        encoded = base64.b64encode(json.dumps(options).encode()).decode()
        result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive',
                                 '-ExecutionPolicy', 'Bypass', '-File', str(wrapper),
                                 str(installer), encoded], capture_output=True,
                                encoding='utf-8', errors='replace', env=env, timeout=20)
        assert (result.returncode == 0) == success, result.stdout + result.stderr
        answer = json.loads(result.stdout.strip())
        if not success:
            assert answer['status'] == 'error', answer
        return answer

    def targets(**options):
        return run(ListInstalled=True, **options)['targets']

    def find(source, collection='集合甲', kind=None):
        matches = [entry for entry in targets() if entry['source'] == source
                   and entry['collection'] == collection and (kind is None or entry['kind'] == kind)]
        assert len(matches) == 1, matches
        return matches[0]

    listing = run(ListInstalled=True)
    installed = listing['targets']
    file_installs = {entry['path']: entry for entry in listing['fileInstalls']}
    assert set(file_installs) == {str(plugin), str(legacy)}, 'Plugin/material directories were hidden'
    assert file_installs[str(plugin)]['kind'] == 'plugin'
    assert file_installs[str(legacy)]['kind'] == 'legacy'
    for folder in (plugin, legacy):
        files = [path for path in folder.rglob('*') if path.is_file()]
        assert file_installs[str(folder)]['fileCount'] == len(files)
        assert file_installs[str(folder)]['bytes'] == sum(path.stat().st_size for path in files)
    dialog = run(TestDialog=True)
    assert dialog['windowsShown'] is False and dialog['sourceRows'] == len(installed)
    assert {entry['path'] for entry in dialog['fileRows']} == {str(plugin), str(legacy)}
    assert {entry['kind'] for entry in dialog['fileRows']} == {'一键版目录', '旧路径副本'}
    assert all(entry['size'].endswith(' MB') for entry in dialog['fileRows'])
    assert run(ListInstalled=True, TestObsRunning=True)['obsRunning'] is True
    assert len(installed) == 7, installed
    shared = find('共享奶蛙')
    assert set(shared['locations']) == {'主场景 / 共享奶蛙', '主场景 / 分组 / 共享奶蛙',
                                        '主场景 / 嵌套场景 / 共享奶蛙', '嵌套场景 / 共享奶蛙'}
    assert find('未使用奶蛙')['locations'] == ['[未被场景引用] / 未使用奶蛙']
    assert find('相机')['kind'] == 'milk_frog_native_animation'
    assert not any(entry['source'] == '奶蛙' for entry in installed), 'Matched a foreign name'
    assert scene_file.read_bytes() == before and other_file.read_bytes() == other_before
    run(success=False, Action='Uninstall')
    run(success=False, Action='Uninstall', TargetId=['invalid'])
    run(success=False, RemoveFiles=True)
    run(success=False, Action='Uninstall', TargetId=[shared['targetId']], TestObsRunning=True)
    run(success=False, Action='Uninstall', TargetId=[shared['targetId']], RemoveFiles=True)
    assert scene_file.read_bytes() == before and pending.exists()
    scene_file.write_bytes(before + b'\n')
    run(success=False, Action='Uninstall', TargetId=[shared['targetId']])
    assert scene_file.read_bytes() == before + b'\n', 'Stale selection changed the collection'
    scene_file.write_bytes(before)
    answer = run(Action='Uninstall', TargetId=[shared['targetId']])
    assert answer['status'] == 'complete' and len(answer['removed']) == 1
    assert answer['cancelledRequests'] == 2
    assert (Path(answer['backupPath']) / '0-a.json').read_bytes() == before
    expected = copy.deepcopy(fixture)
    expected['sources'] = [source for source in expected['sources'] if source.get('uuid') != 'shared']
    for node in expected['sources'] + expected['groups']:
        if node['id'] in ('scene', 'group'):
            node['settings']['items'] = [entry for entry in node['settings']['items']
                                         if entry.get('source_uuid') != 'shared']
    assert json.loads(scene_file.read_text(encoding='utf-8')) == expected
    assert other_file.read_bytes() == other_before and pending.exists()
    assert not (legacy / 'data/requests/shared.json').exists()
    assert not (plugin / 'data/requests/shared.json').exists()
    assert (plugin / 'data/keybindings.json').exists()
    assert len(run(ListInstalled=True)['fileInstalls']) == 2, 'Removing a source hid its remaining files'
    run(success=False, Action='Uninstall', TargetId=[shared['targetId']])
    run(Action='Uninstall', TargetId=[find('待安装奶蛙')['targetId']])
    assert not pending.exists()
    assert json.loads(scene_file.read_text(encoding='utf-8')) == expected
    run(Action='Uninstall', TargetId=[find('相机')['targetId']])
    expected['sources'][6]['filters'] = [expected['sources'][6]['filters'][0]]
    assert json.loads(scene_file.read_text(encoding='utf-8')) == expected, 'Removed the host or foreign filter'

    # File cleanup fails closed for links and custom configuration roots.
    custom = profile / 'CustomConfig'
    (custom / 'basic/scenes').mkdir(parents=True)
    run(success=False, Action='Uninstall', RemoveFiles=True, ConfigRoot=str(custom))
    linked = profile / 'LinkedAssets'
    linked.mkdir()
    (linked / 'keep.txt').write_text('keep', encoding='utf-8')
    junction = plugin / 'linked'
    subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(junction), str(linked)],
                   check=True, capture_output=True)
    remaining_ids = [entry['targetId'] for entry in targets()]
    current = scene_file.read_bytes()
    run(success=False, Action='Uninstall', TargetId=remaining_ids, RemoveFiles=True)
    assert scene_file.read_bytes() == current and (linked / 'keep.txt').is_file()
    junction.rmdir()  # Remove only the junction itself, never its target.

    answer = run(Action='Uninstall', TargetId=remaining_ids, RemoveFiles=True)
    assert answer['status'] == 'complete' and len(answer['removedFolders']) == 2
    assert not targets() and not plugin.exists() and not legacy.exists()
    assert run(ListInstalled=True)['fileInstalls'] == []
    assert unrelated_plugin.read_text() == 'keep'
    assert desktop.read_text() == 'shared desktop settings'
    assert Path(answer['backupPath']).is_dir()

    # Cleanup remains available after someone already deleted every source in OBS.
    plugin.mkdir()
    (plugin / 'leftover.txt').write_text('old files', encoding='utf-8')
    legacy.mkdir()
    (legacy / 'old-asset.txt').write_text('old copy', encoding='utf-8')
    listing = run(ListInstalled=True)
    assert listing['targets'] == [], 'Leftover-only fixture unexpectedly has sources'
    assert {entry['path'] for entry in listing['fileInstalls']} == {str(plugin), str(legacy)}
    dialog = run(TestDialog=True)
    assert dialog['windowsShown'] is False and dialog['sourceRows'] == 0
    assert {entry['path'] for entry in dialog['fileRows']} == {str(plugin), str(legacy)}
    assert set(run(Action='Uninstall', RemoveFiles=True)['removedFolders']) == {str(plugin), str(legacy)}

    portable = profile / 'Portable'
    portable_scenes = portable / 'config/obs-studio/basic/scenes'
    portable_scenes.mkdir(parents=True)
    (portable / 'portable_mode.txt').touch()
    (portable_scenes / 'b.json').write_bytes(other_before)
    portable_plugin = portable / 'plugins/milk-frog-easy'
    portable_plugin.mkdir(parents=True)
    (portable_plugin / 'asset.txt').write_text('portable', encoding='utf-8')
    portable_listing = run(ListInstalled=True, ObsRoot=str(portable))
    assert [entry['path'] for entry in portable_listing['fileInstalls']] == [str(portable_plugin)]
    portable_target = portable_listing['targets'][0]
    answer = run(Action='Uninstall', TargetId=[portable_target['targetId']],
                 RemoveFiles=True, ObsRoot=str(portable))
    assert answer['removedFolders'] == [str(portable_plugin)] and not portable_plugin.exists()
    assert desktop.is_file() and unrelated_plugin.is_file()
    return ['uninstall: installed and legacy file directories listed with paths, counts and bytes',
            'uninstall: both file copies discoverable after all scene sources are removed',
            'uninstall: actual GUI controls contain both directories with and without sources; no window shown',
            'uninstall: scene/group/nested/shared locations and UUID resolution',
            'uninstall: foreign source names ignored and unused sources discoverable',
            'uninstall: explicit selection, OBS closed, stale selection and all-reference guards',
            'uninstall: selected source and all references removed with exact backup',
            'uninstall: unrelated collection, transforms, source settings and plugin preserved',
            'uninstall: selected pending imports cancelled in both plugin directories',
            'uninstall: legacy filter removed without its host or foreign filters',
            'uninstall: legacy audio, unused sources and pending-only removal',
            'uninstall: custom config and junction cleanup rejected before mutation',
            'uninstall: optional file cleanup preserves desktop settings and backups',
            'uninstall: leftover-only and portable cleanup without UI']


if __name__ == '__main__':
    output = Path(__file__).resolve().parents[1] / 'build/obs-easy/verification'
    output.mkdir(parents=True, exist_ok=True)
    checks = verify_uninstall(output)
    report = {'passed': True, 'checks': checks}
    (output / 'uninstall-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))
