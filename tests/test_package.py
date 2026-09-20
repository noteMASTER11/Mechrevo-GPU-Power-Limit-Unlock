import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import package_common as common
import build

class PackageTests(unittest.TestCase):
    def test_source_paths_are_relative_to_script(self):
        self.assertTrue((common.ROOT / 'src/runtime/run_boot.py').is_file())
        self.assertTrue((common.ROOT / 'patches/nvidia-gsp-persistent-v6.patch').is_file())
        for name in common.RUNTIME:
            if name != 'query_power.so': self.assertTrue((common.ROOT / 'src/runtime' / name).is_file())

    def test_pin_and_module_names(self):
        self.assertEqual(len(common.NVIDIA_BASE), 40)
        self.assertEqual(len(common.FIRMWARE_SHA256), 64)
        self.assertEqual(len(common.MODULES), 4)
        for name in common.MODULES: self.assertNotIn('/', name)

    def test_wrong_base_refused_before_patch(self):
        with tempfile.TemporaryDirectory() as tmp, patch('build.subprocess.check_output', return_value='wrong\n'), patch('build.run') as runner:
            with self.assertRaisesRegex(RuntimeError, 'Source HEAD'): build.prepare_source(Path(tmp))
            runner.assert_not_called()

    def test_wrong_kernel_refused_before_reading_firmware(self):
        with patch('package_common.platform.release', return_value='other-kernel'):
            with self.assertRaisesRegex(RuntimeError, 'kernel'): common.validate_host()

    def test_prepare_is_repeatable_and_refuses_unrelated_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); source = root / 'source'; source.mkdir()
            def git(*args):
                return subprocess.check_output(['git', *args], cwd=source, stderr=subprocess.DEVNULL)
            git('init'); git('config', 'user.name', 'Test'); git('config', 'user.email', 'test@example.invalid')
            (source / 'tracked').write_text('base\n')
            git('add', '.'); git('commit', '-m', 'base')
            base = git('rev-parse', 'HEAD').decode().strip()
            (source / 'tracked').write_text('patched\n'); (source / 'added').write_text('new\n')
            git('add', '.')
            (root / 'patches').mkdir()
            (root / 'patches/nvidia-gsp-persistent-v6.patch').write_bytes(git('diff', 'HEAD', '--binary'))
            git('reset', '--hard', 'HEAD')
            with patch('build.ROOT', root), patch('build.NVIDIA_BASE', base):
                build.prepare_source(source); build.prepare_source(source)
                (source / 'extra').write_text('unrelated')
                with self.assertRaisesRegex(RuntimeError, 'Untracked'): build.prepare_source(source)
                (source / 'extra').unlink(); (source / 'tracked').write_text('unrelated\n')
                with self.assertRaisesRegex(RuntimeError, 'differs'): build.prepare_source(source)

    def test_manifest_requires_complete_module_and_runtime_sets(self):
        import copy
        m = dict(nvidia_base=common.NVIDIA_BASE, kernel=common.KERNEL,
                 patch_sha256=common.sha(common.ROOT/'patches/nvidia-gsp-persistent-v6.patch'),
                 source='/tmp/source/kernel-open', output='/tmp/deployment/runtime',
                 client_sha256='a'*64, firmware_sha256=common.FIRMWARE_SHA256,
                 probe_module_hashes={n+'.ko':'a'*64 for n in common.MODULES},
                 runtime_hashes={n:'a'*64 for n in common.RUNTIME},
                 stock_module_hashes={f'/usr/lib/modules/{common.KERNEL}/extramodules/{n}.ko.zst':'a'*64 for n in common.MODULES})
        common.validate_manifest(m, deployment=True)
        for field in ['probe_module_hashes', 'runtime_hashes', 'stock_module_hashes']:
            for kind in ['empty', 'missing', 'extra']:
                bad = copy.deepcopy(m)
                if kind == 'empty': bad[field] = {}
                elif kind == 'missing': bad[field].pop(next(iter(bad[field])))
                else: bad[field]['../unexpected'] = 'a'*64
                with self.subTest(field=field, kind=kind), self.assertRaises(RuntimeError):
                    common.validate_manifest(bad, deployment=True)

    def test_sha(self):
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp)/'module.ko'; file.write_bytes(b'abc')
            self.assertEqual(common.sha(file), 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')

if __name__ == '__main__': unittest.main()
