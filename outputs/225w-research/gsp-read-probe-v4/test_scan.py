import struct,unittest
from scan_policy import scan
class ScanTest(unittest.TestCase):
 def test_candidate_crosses_page(self):
  b=bytearray(8192);at=4096-128
  for off,val in [(0x2d0,0x177f3c4),(0x2d8,0x177c0f4),(0x2e0,0x177c250)]:struct.pack_into('<Q',b,at+off,val)
  struct.pack_into('<I',b,at+0x108,175000)
  r=scan(b,0x2000);self.assertEqual(len(r),1);self.assertEqual(r[0]['heap_offset'],hex(at+0x2000));self.assertEqual(r[0]['max_effective_raw'],175000)
 def test_observed_board_subclass(self):
  b=bytearray(4096)
  for off,val in [(8,0x4193460),(0x2d0,0x17b2248),(0x2d8,0x177c0f4),(0x2e0,0x177c250)]:struct.pack_into('<Q',b,off,val)
  struct.pack_into('<I',b,0x108,175000)
  self.assertEqual(len(scan(b,0)),1)
  b[0]=18;self.assertEqual(scan(b,0),[])
 def test_value_alone_rejected(self):
  self.assertEqual(scan(struct.pack('<I',175000)*1024,0),[])
 def test_partial_signature_rejected(self):
  b=bytearray(4096);struct.pack_into('<Q',b,0x2d0,0x177f3c4);self.assertEqual(scan(b,0),[])
 def test_truncated(self):
  b=bytearray(0x2d8);struct.pack_into('<Q',b,0x2d0,0x177f3c4);self.assertEqual(scan(b,0),[])
if __name__=='__main__':unittest.main()
