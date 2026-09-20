"""Real subprocess regression: parent Ctrl-C must not SIGKILL a child mid-restore."""
import os,signal,subprocess,sys,tempfile,time,unittest
from pathlib import Path
class SignalsTest(unittest.TestCase):
 def test_interrupt_waits_for_child_restore_and_parent_evidence(self):
  root=Path(__file__).resolve().parents[1]/'src/runtime'
  with tempfile.TemporaryDirectory() as tmp:
   child="from pathlib import Path;import sys,time;p=Path(sys.argv[1]);(p/'applied').touch();time.sleep(0.3);(p/'restored').touch()"
   parent="from transaction_signals import blocked_signals;import subprocess,sys;from pathlib import Path\nwith blocked_signals():\n subprocess.run([sys.executable,'-c',sys.argv[2],sys.argv[1]],check=True)\n (Path(sys.argv[1])/'evidence').touch()\n"
   p=subprocess.Popen([sys.executable,'-c',parent,tmp,child],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   deadline=time.monotonic()+5
   while not (Path(tmp)/'applied').exists() and p.poll() is None and time.monotonic()<deadline:time.sleep(.005)
   self.assertTrue((Path(tmp)/'applied').exists())
   os.kill(p.pid,signal.SIGINT);p.communicate(timeout=5)
   self.assertTrue((Path(tmp)/'restored').exists(),'Ctrl-C killed child before rollback')
   self.assertTrue((Path(tmp)/'evidence').exists(),'interruption arrived before evidence persisted')
if __name__=='__main__':unittest.main()
