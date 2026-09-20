#!/usr/bin/env python3
"""Disposable, offline execution of the original Nvpwr.sys in Unicorn.

No host device, driver, Windows service, or GPU API is opened. Windows and
NVIDIA callbacks are explicitly modeled. Successful model execution is NOT
evidence of GPU power-limit acceptance. Unknown callbacks/accesses stop the run.
"""
import collections
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import capstone
import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ, UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP

HERE = Path(__file__).resolve().parent
SYS = Path.cwd() / 'work/levin-power/Nvpwr.sys'
SYS_SHA = 'e9cb3f6a0a6c92df8e646f931c4078627812680e5e2051a4392330cf8cce80e8'
PE_BASE = 0x140000000
NV_BASE, NV_SIZE = 0x180000000, 0x06d3e000
OBJ_BASE, STACK_BASE, API_BASE = 0x200000000, 0x300000000, 0x400000000
GLOBAL, TABLE, MAJOR, ROOT, BOARD = [OBJ_BASE+x for x in (0x10000, 0x20000, 0x70000, 0x80000, 0x90000)]
LOOKUP = NV_BASE + 0x10000
BOARD_SET, SET_AMOUNT, SET_ELIG = [NV_BASE+x for x in (0x8d6960,0x4e4610,0x4e4680)]
DRIVER, DEVICE, IRP, IRP_STACK, BUFFER = [OBJ_BASE+x for x in (0x1000,0x2000,0x3000,0x4000,0x5000)]
STOP = API_BASE + 0xf000
FIELDS = {
    ROOT+0x3d10: ('Root.initialized',1), ROOT+0x3d11: ('Root.eligibility',1),
    ROOT+0x3d12: ('Root.amountActive',1), ROOT+0x3d14: ('Root.CTGP',4),
    ROOT+0x3d18: ('Root.amount',4), ROOT+0x3d1c: ('Root.policyKey',1),
    ROOT+0x3d20: ('Root.LOWER',4), ROOT+0x3d24: ('Root.UPPER',4),
    BOARD+0x108: ('Board.MAX.effective',4), BOARD+0x10c: ('Board.MAX.secondary',4),
    BOARD+0x114: ('Board.MAX.FE',4), BOARD+0x1f8: ('Board.CURRENT.effective',4),
    BOARD+0x204: ('Board.CURRENT.F7',4),
}

class Emulation:
    def __init__(self, scenario, baseline=175000):
        self.scenario, self.baseline = scenario, baseline
        self.events, self.reads, self.executed = [], collections.Counter(), set()
        self.phase = 'setup'
        self.failed_once = False
        self.unexpected = 0
        self.heap = OBJ_BASE+0xa0000
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        raw = SYS.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == SYS_SHA, 'Unreviewed SYS binary'
        self.pe = pefile.PE(data=raw)
        assert self.pe.OPTIONAL_HEADER.ImageBase == PE_BASE
        self.uc.mem_map(PE_BASE, 0x10000)
        self.uc.mem_write(PE_BASE, self.pe.get_memory_mapped_image())
        self.uc.mem_map(NV_BASE, (NV_SIZE+4095)&~4095)
        for base, size in [(OBJ_BASE,0x100000),(STACK_BASE,0x100000),(API_BASE,0x10000)]:
            self.uc.mem_map(base,size)
        self.api = {}
        for n, imp in enumerate(self.pe.DIRECTORY_ENTRY_IMPORT[0].imports):
            name = imp.name.decode()
            address = API_BASE+n*0x100
            self.put(imp.address,address,8)
            if name == 'MmSystemRangeStart':
                # Synthetic address-space boundary only; no host kernel mapping.
                self.put(address,0x100000000,8)
            else:
                self.api[address] = name
                self.uc.mem_write(address,b'\xc3')
        self.api[PE_BASE+0xa000] = 'AuxKlibInitialize'
        self.api[PE_BASE+0xa06c] = 'AuxKlibQueryModuleInformation'
        self.put(self.pe.DIRECTORY_ENTRY_LOAD_CONFIG.struct.SecurityCookie,0x123456789abc,8)
        self.synthetic_nvidia()
        self.uc.hook_add(UC_HOOK_CODE,self.on_code)
        self.uc.hook_add(UC_HOOK_MEM_WRITE,self.on_write)
        self.uc.hook_add(UC_HOOK_MEM_READ,self.on_read)
        self.uc.hook_add(UC_HOOK_MEM_INVALID,self.on_invalid)

    def put(self,address,value,size=4):
        self.uc.mem_write(address,int(value & ((1<<(8*size))-1)).to_bytes(size,'little'))

    def get(self,address,size=4):
        return int.from_bytes(self.uc.mem_read(address,size),'little')

    def cstring(self,address,wide=False):
        if not address: return ''
        unit=2 if wide else 1
        b=bytearray()
        for n in range(4096):
            c=self.uc.mem_read(address+n*unit,unit)
            if not any(c): break
            b.extend(c)
        return b.decode('utf-16-le' if wide else 'utf-8',errors='replace')

    def args(self,n=8):
        regs=[UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9]
        stack=self.uc.reg_read(UC_X86_REG_RSP)
        return [self.uc.reg_read(r) for r in regs[:n]]+[self.get(stack+0x28+8*i,8) for i in range(max(0,n-4))]

    def event(self,kind,**kw):
        if kind=='nvidia_call':
            kw['callee_va']=hex(self.uc.reg_read(UC_X86_REG_RIP))
            kw['caller_return_va']=hex(self.get(self.uc.reg_read(UC_X86_REG_RSP),8))
        self.events.append(dict(seq=len(self.events),phase=self.phase,kind=kind,**kw))

    def instruction(self):
        pc=self.uc.reg_read(UC_X86_REG_RIP)
        ins=next(self.dis.disasm(bytes(self.uc.mem_read(pc,15)),pc,count=1),None)
        return f'{ins.mnemonic} {ins.op_str}' if ins else '?'

    def return_from_hook(self,value=0):
        stack=self.uc.reg_read(UC_X86_REG_RSP)
        self.uc.reg_write(UC_X86_REG_RAX,value & 0xffffffffffffffff)
        self.uc.reg_write(UC_X86_REG_RIP,self.get(stack,8))
        self.uc.reg_write(UC_X86_REG_RSP,stack+8)

    def synthetic_nvidia(self):
        self.uc.mem_write(NV_BASE,b'MZ')
        self.put(NV_BASE+0x3c,0x100)
        self.uc.mem_write(NV_BASE+0x100,b'PE\0\0')
        self.put(NV_BASE+0x108,0x6a9b4070 if self.scenario!='wrong_build' else 0x6a9b4071)
        self.put(NV_BASE+0x118,0x20b,2)
        self.put(NV_BASE+0x150,NV_SIZE)
        signatures={
            0x107ac0:'488b0551b32a01448bd94c8b8808020000',
            0x4e3fbf:'448b8b243d0000', 0x4e4088:'66c7442448f703',
            0x8d6960:'48895c240848896c24104889742418',
            0x4e4610:'4883ec38488b81b0250000448bc280b8103d000000',
            0x4e4680:'4883ec28488b81b025000080b8103d000000',
        }
        for rva,hexbytes in signatures.items(): self.uc.mem_write(NV_BASE+rva,bytes.fromhex(hexbytes))
        if self.scenario=='wrong_signature': self.put(NV_BASE+0x4e3fbf,0,1)
        for address,value in [(NV_BASE+0x13b2e18,GLOBAL),(GLOBAL+0x208,TABLE),
                              (TABLE+0x48a48,MAJOR),(MAJOR+0x25b0,ROOT),
                              (ROOT+0x1cf8,LOOKUP),(BOARD+0x2d0,BOARD_SET)]:
            self.put(address,value,8)
        self.put(TABLE+0x48c48,1)
        self.put(TABLE+0x48a50,0x5080) # opaque GPU registry ID, NOT a PCI ID
        self.put(ROOT+0x3d10,1,1)
        self.put(ROOT+0x3d1c,2,1)
        self.put(ROOT+0x3d14,self.baseline)
        self.put(ROOT+0x3d20,5000)
        self.put(ROOT+0x3d24,self.baseline)
        for slot,source in [(BOARD+0x104,0xfe),(BOARD+0x1f4,0xf7)]:
            self.put(slot,0,1);self.put(slot+1,1,1)
            self.put(slot+4,self.baseline);self.put(slot+8,self.baseline)
            self.put(slot+12,source,1);self.put(slot+16,self.baseline)

    def snapshot(self):
        offsets={'CTGP':ROOT+0x3d14,'amount':ROOT+0x3d18,'LOWER':ROOT+0x3d20,
                 'UPPER':ROOT+0x3d24,'MAX':BOARD+0x108,'MAX_FE':BOARD+0x114,
                 'CURRENT':BOARD+0x1f8,'F7':BOARD+0x204}
        state={k:self.get(a) for k,a in offsets.items()}
        state.update(eligibility=self.get(ROOT+0x3d11,1),amountActive=self.get(ROOT+0x3d12,1))
        return state

    def model_write(self,address,value,size=4):
        self.event('write',origin='nvidia_model',field=FIELDS.get(address,(hex(address),size))[0],
                   address=hex(address),size=size,old=self.get(address,size),new=value)
        self.put(address,value,size)

    def generator(self):
        state=self.snapshot()
        if self.scenario=='generator_stale' and state['UPPER']==225000 and not self.failed_once:
            self.failed_once=True
            self.event('fault_injected',where='NVIDIA generator',behavior='leave F7/CURRENT stale')
            return
        value=min(state['CTGP'],state['UPPER'])
        if state['eligibility'] and state['amountActive']:
            assert state['UPPER']>state['LOWER'] and state['amount']<=state['UPPER']-state['LOWER']
            value=min(state['CTGP'],state['UPPER']-state['amount'])+state['amount']
        self.model_write(BOARD+0x204,value)
        self.model_write(BOARD+0x1f8,value)

    def nvidia_call(self,address):
        a=self.args(6)
        if address==LOOKUP:
            assert a[0]==ROOT+0x1cc0 and a[1]==2
            self.event('nvidia_call',name='PolicyLookup',registry=hex(a[0]),key=a[1])
            self.return_from_hook(BOARD);return
        if address==BOARD_SET:
            assert a[:3]==[MAJOR,ROOT,BOARD]
            selector,source,value=[x&0xffffffff for x in a[3:6]]
            assert selector==2 and source==0xfe, (selector,source,value)
            self.event('nvidia_call',name='BoardSet',selector=selector,source=hex(source),value=value)
            if self.scenario=='board_set_refuses' and value==225000 and not self.failed_once:
                self.failed_once=True
                self.event('fault_injected',where='BoardSet',return_status='0x25')
                self.return_from_hook(0x25);return
            for offset in (4,8,16):self.model_write(BOARD+0x104+offset,value)
        elif address==SET_AMOUNT:
            assert a[0]==MAJOR
            self.event('nvidia_call',name='SetAmount',value=a[1])
            self.model_write(ROOT+0x3d18,a[1]);self.model_write(ROOT+0x3d12,int(a[1]!=0),1)
            self.generator()
        elif address==SET_ELIG:
            assert a[0]==MAJOR and a[1] in (0,1)
            self.event('nvidia_call',name='SetEligibility',value=a[1])
            self.model_write(ROOT+0x3d11,a[1],1);self.generator()
        self.event('nvidia_return',status=0,state=self.snapshot())
        self.return_from_hook(0)

    def dbg(self):
        a=self.args(24)
        fmt=self.cstring(a[2]);values=iter(a[3:]);raw=[]
        pattern=r'%%|%([-+ #0]*)(\d*)(?:\.(\d+))?(I64|ll|l|z)?([pudxXsc])'
        def replace(m):
            if m.group(0)=='%%':return '%'
            value=next(values);raw.append(value)
            typ=m.group(5);width=int(m.group(2) or 0)
            if typ=='p':return f'0x{value:016x}'
            if typ=='s':return self.cstring(value)
            if typ=='c':return chr(value&255)
            value &= 0xffffffffffffffff if m.group(4) in ('I64','ll','z') else 0xffffffff
            if typ=='d' and value&0x80000000:value-=0x100000000
            result=format(value,typ) if typ in ('x','X') else str(value)
            return result.rjust(width,'0' if '0' in m.group(1) else ' ')
        rendered=re.sub(pattern,replace,fmt)
        self.event('dbgprint',text=rendered.rstrip(),format=fmt,args=raw)

    def windows_call(self,name):
        a=self.args(8);result=0
        if name=='DbgPrintEx':self.dbg()
        elif name in ('AuxKlibInitialize','KeGetCurrentIrql','KeInitializeMutex','KeReleaseMutex',
                      'KeWaitForSingleObject','ExFreePoolWithTag','IofCompleteRequest',
                      'IoCreateSymbolicLink','IoDeleteSymbolicLink','IoDeleteDevice'):pass
        elif name=='AuxKlibQueryModuleInformation':
            assert a[1]==272, a[1]
            self.put(a[0],272)
            if a[2]:
                self.put(a[2],NV_BASE,8);self.put(a[2]+8,NV_SIZE)
                self.put(a[2]+12,0,2)
                self.uc.mem_write(a[2]+14,b'nvlddmkm.sys\0')
            self.event('windows_model',name=name,provided='synthetic NVIDIA 616.92 module')
        elif name in ('ExAllocatePool2','ExAllocatePoolWithQuotaTag'):
            result=self.heap;self.heap+=(a[1]+15)&~15
            assert self.heap<OBJ_BASE+0x100000
        elif name in ('RtlInitAnsiString','RtlInitUnicodeString'):
            wide=name=='RtlInitUnicodeString';s=self.cstring(a[1],wide)
            size=len(s.encode('utf-16-le' if wide else 'utf-8'))
            self.put(a[0],size,2);self.put(a[0]+2,size+(2 if wide else 1),2);self.put(a[0]+8,a[1],8)
        elif name=='RtlEqualString':
            strings=[bytes(self.uc.mem_read(self.get(p+8,8),self.get(p,2))) for p in a[:2]]
            result=int(strings[0].lower()==strings[1].lower()) if a[2] else int(strings[0]==strings[1])
        elif name=='IoCreateDevice':self.put(a[6],DEVICE,8)
        else:raise RuntimeError('Unmodeled Windows callback '+name)
        self.return_from_hook(result)

    def on_code(self,uc,address,size,_):
        if address==STOP:uc.emu_stop();return
        if address in self.api:self.windows_call(self.api[address]);return
        if address in (LOOKUP,BOARD_SET,SET_AMOUNT,SET_ELIG):self.nvidia_call(address);return
        if not PE_BASE<=address<PE_BASE+self.pe.OPTIONAL_HEADER.SizeOfImage:
            raise RuntimeError(f'Unexpected execution at {address:#x}')
        self.executed.add(address-PE_BASE)

    def on_write(self,uc,access,address,size,value,_):
        if NV_BASE<=address<NV_BASE+NV_SIZE or GLOBAL<=address<OBJ_BASE+0xa0000:
            self.event('write',origin='sys_instruction',field=FIELDS.get(address,(hex(address),size))[0],
                       address=hex(address),size=size,old=self.get(address,size),
                       new=value&((1<<(8*size))-1),pc=hex(uc.reg_read(UC_X86_REG_RIP)),
                       sys_rva=hex(uc.reg_read(UC_X86_REG_RIP)-PE_BASE),instruction=self.instruction())

    def on_read(self,uc,access,address,size,value,_):
        if address in FIELDS:self.reads[FIELDS[address][0]]+=1

    def on_invalid(self,uc,access,address,size,value,_):
        self.unexpected+=1
        self.event('invalid_access',address=hex(address),size=size,access=access,pc=hex(uc.reg_read(UC_X86_REG_RIP)))
        return False

    def call(self,address,*args):
        sp=STACK_BASE+0x80000-8
        self.put(sp,STOP,8)
        self.uc.reg_write(UC_X86_REG_RSP,sp)
        for reg,value in zip((UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9),args):self.uc.reg_write(reg,value)
        self.uc.emu_start(address,STOP,count=500000)
        if self.uc.reg_read(UC_X86_REG_RIP)!=STOP:raise RuntimeError('Instruction budget exhausted')
        return self.uc.reg_read(UC_X86_REG_RAX)&0xffffffff

    def ioctl(self,code,target=225000,version=2):
        self.uc.mem_write(IRP,bytes(0x100));self.uc.mem_write(IRP_STACK,bytes(0x80));self.uc.mem_write(BUFFER,bytes(0x1000))
        self.put(IRP+0x18,BUFFER,8);self.put(IRP+0xb8,IRP_STACK,8)
        self.put(IRP_STACK+8,192);self.put(IRP_STACK+16,16);self.put(IRP_STACK+24,code)
        self.uc.mem_write(BUFFER,struct.pack('<4I',version,target,2,0))
        self.event('ioctl',code=hex(code),version=version,profile=2,target_mW=target)
        dispatch=self.get(DRIVER+0xe0,8)
        result=self.call(dispatch,DEVICE,IRP)
        assert self.get(IRP+0x30)==result
        self.event('ioctl_return',status=f'0x{result:08x}',state=self.snapshot())
        return f'0x{result:08x}'

    def run(self):
        self.phase='driver_entry'
        result=self.call(PE_BASE+self.pe.OPTIONAL_HEADER.AddressOfEntryPoint,DRIVER,0)
        assert result==0
        initial=self.snapshot()
        self.phase='set'
        status=self.ioctl(0x22e004,250000 if self.scenario=='unsupported_250w' else 225000,
                          999 if self.scenario=='invalid_request_version' else 2)
        after=self.snapshot()
        summary=dict(set_status=status,initial=initial,after_set=after,
                     policy_writes=sum(e['kind']=='write' for e in self.events),
                     unexpected_accesses=self.unexpected)
        if self.scenario=='compatible_5080_225w':
            self.phase='restore';summary['restore_status']=self.ioctl(0x22e008)
            summary['after_restore']=self.snapshot()
        return summary

def main():
    global SYS
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sys',type=Path,default=SYS,help='Original, hash-pinned Nvpwr.sys')
    parser.add_argument('--output',type=Path,default=HERE/'results')
    options=parser.parse_args()
    SYS=options.sys.resolve()
    out=options.output;out.mkdir(exist_ok=True,parents=True)
    summaries={}
    scenarios=['compatible_5080_225w','wrong_build','wrong_signature','unsupported_250w',
               'wrong_baseline','invalid_request_version','board_set_refuses','generator_stale']
    for scenario in scenarios:
        emu=Emulation(scenario,140000 if scenario=='wrong_baseline' else 175000)
        try: summaries[scenario]=emu.run()
        finally:
            payload=dict(scenario=scenario,sys_sha256=SYS_SHA,events=emu.events,
                         reads=dict(emu.reads),executed_rvas=[hex(x) for x in sorted(emu.executed)],
                         dependencies={'unicorn':unicorn.__version__,'pefile':pefile.__version__,'capstone':capstone.__version__})
            (out/f'{scenario}.json').write_text(json.dumps(payload,indent=2)+'\n')
        print(scenario,summaries[scenario]['set_status'])
    (out/'summary.json').write_text(json.dumps(summaries,indent=2)+'\n')

if __name__=='__main__':main()
