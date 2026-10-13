# Uso: python3 combinar.py firmwarev02.hex Multi_Lector_0x7000.hex Multi_Lector.hex
import sys
def load(fn):
    mem={}; base=0
    for l in open(fn):
        l=l.strip()
        if not l.startswith(':'): continue
        b=bytes.fromhex(l[1:]); n,a,t=b[0],(b[1]<<8)|b[2],b[3]
        if t==0:
            for i in range(n): mem[base+a+i]=b[4+i]
        elif t==2: base=((b[4]<<8)|b[5])<<4
        elif t==4: base=((b[4]<<8)|b[5])<<16
    return mem
c=load(sys.argv[1]); h=load(sys.argv[2])
assert max(c)<0x7000 and min(h)>=0x7000 and max(h)<0x8000, (hex(max(c)),hex(min(h)),hex(max(h)))
img=bytearray(b'\xff'*0x8000)
for a,v in c.items(): img[a]=v
for a,v in h.items(): img[a]=v
ce=(max(c)//128+1)*128; he=(max(h)//128+1)*128
for a in range(max(c)+1,ce): img[a]=0xff
for a in range(max(h)+1,he): img[a]=0
out=[]
for start,end in ((0,ce),(0x7000,he)):
    for a in range(start,end,16):
        d=img[a:a+16]; rec=bytes([len(d),a>>8,a&255,0])+d
        out.append(':'+rec.hex().upper()+'%02X'%((-sum(rec))&255))
out.append(':00000001FF')
open(sys.argv[3],'w',newline='').write('\r\n'.join(out)+'\r\n')
print(hex(ce),hex(he),'HID bytes',max(h)-0x7000+1)
