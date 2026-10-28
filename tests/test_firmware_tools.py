import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import warnings
import urllib.error
import io
from contextlib import redirect_stdout

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / 'scripts'), str(ROOT / 'releases')]
import discover_examples as discovery
import download_firmware as download
import package_firmware as pack
import validate_release as release

COMMIT = 'a' * 40
FRAMEWORK = 'b' * 40
ROW = dict(name='01_demo', path='examples/esp-idf/01_demo', version='test-fixture', commit=FRAMEWORK,
           experimental=True, target='esp32s31', framework='esp-idf', component_manager='3.1.2')


class DiscoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.base = self.root / 'examples/esp-idf'
        self.base.mkdir(parents=True)
        self.add_project('01_demo')
        (self.root/'config').mkdir()
        (self.root/'config/frameworks.json').write_text(json.dumps({'target':'esp32s31', 'frameworks':[{'version':'test-fixture','commit':FRAMEWORK,'experimental':True}]}))

    def add_project(self, name):
        path = self.base / name
        (path/'main').mkdir(parents=True)
        (path/'CMakeLists.txt').write_text(f'project({name})\n')
        (path/'main/CMakeLists.txt').write_text('idf_component_register()\n')
        return path

    def test_all_name_path(self):
        for selector in ['all','01_demo','examples/esp-idf/01_demo']:
            self.assertEqual(len(discovery.discover(self.root,selector)),1)
            self.assertEqual(len(discovery.matrix(self.root,selector)['include']),1)

    def test_invalid_selector(self):
        for selector in ['', '../01_demo', '01_DEMO', 'examples/esp-idf/01_demo/', '/tmp/01_demo']:
            with self.subTest(selector=selector), self.assertRaises(ValueError):
                discovery.discover(self.root,selector)

    def test_nested_third_party_and_factory_excluded(self):
        for parent in ['examples/esp-idf/01_demo/managed_components/lib/examples/02_nested', 'firmware/02_factory', 'backups/02_old', 'examples/esp-idf/03_backup', 'examples/esp-idf/04_vendor']:
            path=self.root/parent
            path.mkdir(parents=True)
            (path/'CMakeLists.txt').write_text('project(02_nested)')
        self.assertEqual(len(discovery.discover(self.root)),1)

    def test_empty_and_duplicate(self):
        self.add_project('01_duplicate')
        with self.assertRaises(ValueError):discovery.discover(self.root)
        shutil.rmtree(self.base)
        self.base.mkdir()
        with self.assertRaises(ValueError):discovery.discover(self.root)

    def test_duplicate_project_name(self):
        p=self.add_project('02_other')
        (p/'CMakeLists.txt').write_text('project(01_demo)')
        with self.assertRaises(ValueError):discovery.discover(self.root)

    def test_empty_matrix_explicit_error(self):
        (self.root/'config/frameworks.json').write_text('{"frameworks": []}')
        with self.assertRaisesRegex(ValueError,'blocked'):discovery.matrix(self.root)

    def test_duplicate_framework_rejected(self):
        config=json.loads((self.root/'config/frameworks.json').read_text())
        config['frameworks']*=2
        (self.root/'config/frameworks.json').write_text(json.dumps(config))
        with self.assertRaises(ValueError):discovery.matrix(self.root)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.build=self.root/'build'
        self.build.mkdir()
        header=bytearray(64)
        header[0]=0xe9
        header[12:14]=(32).to_bytes(2,'little')
        for name in ['bootloader.bin','app.bin']:(self.build/name).write_bytes(header)
        (self.build/'partitions.bin').write_bytes(b'partition test fixture')
        self.flash={'flash_files':{'0x2000':'bootloader.bin','0x8000':'partitions.bin','0x10000':'app.bin'},
                    'flash_settings':{'flash_mode':'dio','flash_freq':'80m','flash_size':'16MB'},
                    'extra_esptool_args':{'chip':'esp32s31','before':'default_reset','after':'hard_reset','stub':True}}
        for role,file in [('bootloader','bootloader.bin'),('app','app.bin'),('partition-table','partitions.bin')]:
            self.flash[role]={'file':file}
        self.save_flash()
        (self.build/'project_description.json').write_text(json.dumps({'target':'esp32s31','project_name':'01_demo','idf_path':str(self.root/'fake-idf')}))
        self.lock=self.root/'dependencies.lock'
        self.lock.write_text('dependencies:\n  idf:\n    version: 0.0.0-test\n')
        self.out=self.root/'output'

    def save_flash(self):
        (self.build/'flasher_args.json').write_text(json.dumps(self.flash))

    def package(self,output=None):
        with patch.object(pack,'git',return_value=FRAMEWORK):
            return pack.package(self.build,ROW,COMMIT,1700000000,self.lock,output or self.out)

    def mutate(self, file, change):
        with zipfile.ZipFile(file) as z: data={n:z.read(n) for n in z.namelist()}
        change(data)
        data['SHA256SUMS']=''.join(f'{download.sha(data[n])}  {n}\n' for n in sorted(set(data)-{'SHA256SUMS'})).encode()
        with zipfile.ZipFile(file,'w') as z:
            for n,b in data.items():z.writestr(n,b)

    def test_round_trip_compact_reproducible(self):
        file=self.package()
        m,data=download.inspect_archive(file)
        self.assertEqual(len(data['bin/combined.bin']),0x10000+64)
        self.assertEqual(m['segments'][0]['offset'],0x2000)
        self.assertEqual(file.read_bytes(),self.package(self.root/'other').read_bytes())
        self.assertEqual(release.validate_set(self.out,[ROW],COMMIT),[file])

    def test_no_overwrite(self):
        self.package()
        with self.assertRaises(FileExistsError):self.package()

    def test_missing_segment(self):
        (self.build/'app.bin').unlink()
        with self.assertRaises(ValueError):self.package()

    def test_wrong_target(self):
        self.flash['extra_esptool_args']['chip']='esp32s3';self.save_flash()
        with self.assertRaises(ValueError):self.package()

    def test_wrong_chip_image(self):
        (self.build/'app.bin').write_bytes(b'\xe9'+b'\0'*63)
        with self.assertRaises(ValueError):self.package()

    def test_overlap(self):
        self.flash['flash_files']['0x2001']='app.bin';self.save_flash()
        with self.assertRaises(ValueError):self.package()

    def test_build_path_escape(self):
        self.flash['flash_files']['0x10000']='../dependencies.lock';self.save_flash()
        with self.assertRaises(ValueError):self.package()

    def test_missing_lock(self):
        self.lock.unlink()
        with self.assertRaises(ValueError):self.package()

    def test_wrong_framework_commit(self):
        with patch.object(pack,'git',return_value='c'*40), self.assertRaises(ValueError):
            pack.package(self.build,ROW,COMMIT,1700000000,self.lock,self.out)

    def test_padding_rejected_even_rehashed(self):
        file=self.package()
        def pad(data):
            data['bin/combined.bin']+=b'\xff'*100
            m=json.loads(data['manifest.json']);m['combined_sha256']=download.sha(data['bin/combined.bin'])
            data['manifest.json']=json.dumps(m).encode()
        self.mutate(file,pad)
        with self.assertRaises(ValueError):download.inspect_archive(file)

    def test_mismatch_gap_rejected_even_rehashed(self):
        file=self.package()
        def corrupt(data):
            b=bytearray(data['bin/combined.bin']);b[0]=0;data['bin/combined.bin']=bytes(b)
            m=json.loads(data['manifest.json']);m['combined_sha256']=download.sha(b);data['manifest.json']=json.dumps(m).encode()
        self.mutate(file,corrupt)
        with self.assertRaises(ValueError):download.inspect_archive(file)

    def test_helper_tampering_rejected(self):
        file=self.package();self.mutate(file,lambda d:d.update({'flash.sh':b'echo changed\n'}))
        with self.assertRaises(ValueError):download.inspect_archive(file)

    def test_checksum_mismatch(self):
        file=self.package()
        with zipfile.ZipFile(file,'a') as z:z.writestr('extra.txt',b'bad')
        with self.assertRaises(ValueError):download.inspect_archive(file)

    def test_zip_traversal_and_symlink(self):
        for name,mode in [('../escape',0o100644),('/absolute',0o100644),('link',0o120777),('bin\\evil',0o100644)]:
            file=self.package(self.root/name.replace('/','_').replace('\\','_'))
            with zipfile.ZipFile(file,'a') as z:
                info=zipfile.ZipInfo(name);info.external_attr=mode<<16;z.writestr(info,b'bad')
            with self.subTest(name=name),self.assertRaises(ValueError):download.inspect_archive(file)

    def test_missing_extra_duplicate_stale_matrix(self):
        file=self.package()
        with self.assertRaises(ValueError):release.validate_set(self.out,[ROW], 'c'*40)
        with self.assertRaises(ValueError):release.validate_set(self.out,[ROW,ROW],COMMIT)
        extra=dict(ROW,name='02_other')
        with self.assertRaises(ValueError):release.validate_set(self.out,[ROW,extra],COMMIT)
        shutil.copy(file,self.out/'extra.zip')
        with self.assertRaises(ValueError):release.validate_set(self.out,[ROW],COMMIT)

    def test_cli_selected_artifact_set_does_not_relax_full_matrix(self):
        row = discovery.matrix()['include'][0]
        description = self.build/'project_description.json'
        metadata = json.loads(description.read_text())
        metadata['project_name'] = row['name']
        description.write_text(json.dumps(metadata))
        with patch.object(pack, 'git', return_value=row['commit']):
            pack.package(self.build, row, COMMIT, 1700000000, self.lock, self.out)
        command = [sys.executable, str(ROOT/'releases/validate_release.py'),
                   str(self.out), '--commit', COMMIT]
        selected = subprocess.run(command + ['--example', row['name']], capture_output=True, text=True)
        self.assertEqual(selected.returncode, 0, selected.stderr)
        self.assertIn('Validated 1 firmware packages', selected.stdout)
        full = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(full.returncode, 0)
        self.assertIn('Expected 12 archives, found 1', full.stderr)
        stale = subprocess.run(command + ['--example', row['name'], '--commit', 'c'*40],
                               capture_output=True, text=True)
        self.assertNotEqual(stale.returncode, 0)
        self.assertIn('mismatch', stale.stderr)

    @unittest.skipUnless(os.name == 'posix', 'POSIX shell execution test')
    def test_shell_fallback_and_port_quoting(self):
        file=self.package();m,data=download.inspect_archive(file)
        work=self.root/'flash dir';work.mkdir()
        (work/'flash.sh').write_bytes(data['flash.sh'])
        tools=self.root/'fake tools';tools.mkdir()
        shutil.copy('/usr/bin/dirname',tools/'dirname')
        (tools/'python').write_text('#!/bin/sh\nexit 1\n')
        (tools/'python3').write_text('#!/bin/sh\n[ "$1" = "-c" ] && exit 0\nprintf "%s\\n" "$@" > "$CAPTURE"\n')
        for p in tools.iterdir():p.chmod(0o755)
        capture=self.root/'args'
        env=dict(os.environ,PATH=str(tools),CAPTURE=str(capture))
        subprocess.run(['/bin/sh',str(work/'flash.sh'),'serial port with spaces'],env=env,check=True)
        args=capture.read_text().splitlines()
        self.assertEqual(args[args.index('--port')+1],'serial port with spaces')
        self.assertIn('esp32s31',args)
        self.assertIn('py -3 -m esptool --port "%PORT%"',data['flash.bat'].decode())
        self.assertIn('exit /b %errorlevel%',data['flash.bat'].decode())

    def test_download_verify_extract_no_overwrite(self):
        file=self.package();payload=file.read_bytes()
        asset={'name':file.name,'digest':'sha256:'+download.sha(payload),'size':len(payload),'state':'uploaded','browser_download_url':'https://example.com/test.zip'}
        destination=self.root/'downloads'
        with patch.object(download,'request',return_value=payload):
            result=download.install_asset(asset,destination,{'name':ROW['name']})
            self.assertTrue((result/'manifest.json').exists())
            with self.assertRaises(FileExistsError):download.install_asset(asset,destination,{'name':ROW['name']})
        self.assertFalse((self.root/'escape').exists())

    def test_download_wrong_digest_and_selection(self):
        file=self.package();payload=file.read_bytes()
        asset={'name':file.name,'digest':'sha256:'+'0'*64,'size':len(payload),'state':'uploaded','browser_download_url':'https://example.com/test.zip'}
        with patch.object(download,'request',return_value=payload):
            with self.assertRaises(ValueError):download.install_asset(asset,self.root/'downloads',{})
            asset['digest']='sha256:'+download.sha(payload)
            with self.assertRaises(ValueError):download.install_asset(asset,self.root/'downloads',{'name':'wrong'})

    def test_insecure_download_url(self):
        for url in ['http://example.com/x','file:///tmp/x','https://user:secret@example.com/x']:
            with self.assertRaises(ValueError):download.request(url)

    def test_duplicate_member(self):
        file=self.package()
        with warnings.catch_warnings():
            warnings.simplefilter('ignore',UserWarning)
            with zipfile.ZipFile(file,'a') as z:z.writestr('README.md',b'duplicate')
        with self.assertRaisesRegex(ValueError,'Duplicate'):download.inspect_archive(file)

    def test_corrupted_checksum(self):
        file=self.package()
        with zipfile.ZipFile(file) as z:data={n:z.read(n) for n in z.namelist()}
        data['README.md']+=b'corrupt'
        with zipfile.ZipFile(file,'w') as z:
            for n,b in data.items():z.writestr(n,b)
        with self.assertRaisesRegex(ValueError,'Checksum'):download.inspect_archive(file)

    def test_download_retries_bounded(self):
        error=urllib.error.URLError('transient')
        with patch.object(download.urllib.request,'urlopen',side_effect=error) as call, patch.object(download.time,'sleep') as sleep:
            with self.assertRaises(urllib.error.URLError):download.request('https://example.com/x')
            self.assertEqual(call.call_count,3)
            self.assertEqual(sleep.call_count,2)

    def test_release_publish_gates(self):
        import publish_release as publish
        archive=self.package()
        payload=archive.read_bytes()
        asset={'name':archive.name,'size':len(payload),'digest':'sha256:'+download.sha(payload),
               'state':'uploaded','browser_download_url':'https://example.com/firmware.zip'}
        metadata={'id':7,'tag_name':'v1.0.0','draft':False,'prerelease':False}
        def fake_git(*args):
            if args[0]=='ls-remote':return COMMIT+'\trefs/tags/v1.0.0'
            return COMMIT
        for wrong_digest, existing in [(False,False),(True,False),(False,True)]:
            calls=[]
            def fake_api(route,method='GET',payload=None,binary=None):
                calls.append((route,method,payload))
                if route=='/repos/owner/repo':return {'default_branch':'main'}
                if '/tags/' in route:
                    if existing:return metadata
                    raise urllib.error.HTTPError(route,404,'missing',{},None)
                if method=='POST' and route.endswith('/releases'):return {'id':7}
                if method=='POST' and '/assets?' in route:return asset
                if '/assets?' in route:return [dict(asset,digest='sha256:'+'0'*64)] if wrong_digest else [asset]
                if method=='PATCH':return metadata
                return {'draft':True}
            def fake_request(url):
                if '/assets?' in url:return json.dumps([asset]).encode()
                if '/releases/tags/' in url:return json.dumps(metadata).encode()
                return archive.read_bytes()
            env={'GITHUB_REF_NAME':'v1.0.0','GITHUB_REPOSITORY':'owner/repo','GITHUB_REF_TYPE':'tag','GITHUB_SHA':COMMIT}
            with patch.dict(os.environ,env),patch.object(publish,'git',side_effect=fake_git),patch.object(publish,'api',side_effect=fake_api),patch.object(publish,'request',side_effect=fake_request),patch.object(publish,'matrix',return_value={'include':[ROW]}),patch.object(publish,'validate_set',return_value=[archive]),patch.object(publish.subprocess,'run'),redirect_stdout(io.StringIO()):
                if wrong_digest or existing:
                    with self.assertRaises(ValueError):publish.main()
                else:publish.main()
            self.assertEqual(any(method=='PATCH' for _,method,_ in calls),not wrong_digest and not existing)
            if existing:self.assertFalse(any(method=='POST' for _,method,_ in calls))



class BuildEntryTests(unittest.TestCase):
    def invoke(self, configure=True, revision=FRAMEWORK):
        import build_example as build
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            (root/ROW['path']).mkdir(parents=True)
            args=['build_example.py','01_demo','--version',ROW['version']]
            if configure:args+=['--configure-only']
            with patch.object(sys,'argv',args),patch.object(build,'ROOT',root),patch.dict(os.environ,{'IDF_PATH':str(root/'idf')}),patch.object(build,'matrix',return_value={'include':[ROW]}),patch.object(build.subprocess,'check_output',return_value=revision+'\n'),patch.object(build.subprocess,'run') as run:
                build.main()
                return run.call_args.args[0]

    def test_configure_only_uses_preview_without_build(self):
        command=self.invoke()
        self.assertIn('--preview',command)
        self.assertEqual(command[-1],'reconfigure')
        self.assertNotIn('build',command)
        self.assertNotIn('flash',command)

    def test_normal_build_uses_preview(self):
        command=self.invoke(configure=False)
        self.assertIn('--preview',command)
        self.assertEqual(command[-1],'build')

    def test_wrong_framework_refuses_execution(self):
        from contextlib import redirect_stderr
        with redirect_stderr(io.StringIO()),self.assertRaises(SystemExit) as error:
            self.invoke(revision='c'*40)
        self.assertEqual(error.exception.code,2)


class WorkflowTests(unittest.TestCase):
    def test_release_dependencies_no_failure_bypass(self):
        workflow=(ROOT/'.github/workflows/firmware.yml').read_text()
        self.assertIn('needs: [checks, build, validate-artifacts]',workflow)
        self.assertNotIn('always()',workflow)
        self.assertNotIn('continue-on-error',workflow)
        self.assertIn('if-no-files-found: error',workflow)
        self.assertIn("github.ref_type == 'tag' && 'all'",workflow)
        self.assertIn("!examples/esp-idf/**/*.md",workflow)
        for line in workflow.splitlines():
            if 'uses:' in line:
                self.assertRegex(line,r'@[0-9a-f]{40}$')


if __name__=='__main__':unittest.main()
