"""Behavior checks for a disposable SYS execution experiment."""
import json
import subprocess
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).parent

class EmulationTests(unittest.TestCase):
    def test_binary_trace_and_negative_controls(self):
        runner = HERE / 'emulate_nvpwr.py'
        self.assertTrue(runner.exists(), 'Missing executable SYS emulation probe')
        subprocess.run([sys.executable, str(runner)], check=True)
        results = json.loads((HERE / 'results/summary.json').read_text())
        ok = results['compatible_5080_225w']
        self.assertEqual(ok['set_status'], '0x00000000')
        self.assertEqual(ok['after_set']['MAX'], 225000)
        self.assertEqual(ok['after_set']['UPPER'], 225000)
        self.assertEqual(ok['after_set']['F7'], 225000)
        self.assertEqual(ok['restore_status'], '0x00000000')
        self.assertEqual(ok['after_restore'], ok['initial'])
        trace = json.loads((HERE / 'results/compatible_5080_225w.json').read_text())['events']
        writes = [e for e in trace if e['kind'] == 'write' and e['origin'] == 'sys_instruction' and e['phase'] == 'set']
        self.assertEqual([(e['field'], e['new']) for e in writes],
                         [('Root.UPPER',175000), ('Root.CTGP',200000), ('Root.UPPER',225000)])
        self.assertTrue(all(e['instruction'].startswith('xchg') for e in writes))
        for name in ['wrong_build', 'wrong_signature', 'unsupported_250w', 'wrong_baseline', 'invalid_request_version']:
            result = results[name]
            self.assertNotEqual(result['set_status'], '0x00000000', name)
            self.assertEqual(result['policy_writes'], 0, name)
            self.assertEqual(result['initial'], result['after_set'], name)
        for name in ['board_set_refuses', 'generator_stale']:
            result = results[name]
            self.assertNotEqual(result['set_status'], '0x00000000', name)
            self.assertEqual(result['initial'], result['after_set'], name)
        self.assertTrue(all(r['unexpected_accesses'] == 0 for r in results.values()))

if __name__ == '__main__':
    unittest.main()
