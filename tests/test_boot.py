import json,tempfile,unittest
from pathlib import Path
from run_boot import boot_enabled,ac_online,check_log
class BootTests(unittest.TestCase):
 def test_exact_token(self):
  self.assertTrue(boot_enabled('quiet codex.max_tgp=225 rw'))
  for bad in ['quiet','codex.max_tgp=2250','x=codex.max_tgp=225','codex.max_tgp=175']:self.assertFalse(boot_enabled(bad))
 def test_ac(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);self.assertFalse(ac_online(root));p=root/'AC';p.mkdir();(p/'type').write_text('Mains\n');(p/'online').write_text('0');self.assertFalse(ac_online(root));(p/'online').write_text('1');self.assertTrue(ac_online(root))
 def test_transaction(self):
  def row(op):return dict(op=op,rc=0,status=0,result=0,poisoned=0,stage=100,max=225000,fe=225000,upper=225000,current=225000,active=1)
  rows=[row(i) for i in [0,1,2,4]]
  def log():return '\n'.join('GSP_POWER '+json.dumps(r) for r in rows)+'\nGSP_POWER SUCCESS mode=activate'
  self.assertEqual(len(check_log(log(),'activate',0)),4)
  rows[-1]['current']=145000
  with self.assertRaises(RuntimeError):check_log(log(),'activate',0)
  rows[-1]['current']=225000;rows[-1]['poisoned']=1
  with self.assertRaises(RuntimeError):check_log(log(),'activate',0)
  with self.assertRaises(RuntimeError):check_log('','activate',0)
if __name__=='__main__':unittest.main()
