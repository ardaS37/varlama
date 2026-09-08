"""Conservative two-layer grid routing, followed by KiCad DRC (not a DRC replacement)."""
from pathlib import Path
import json,re,heapq,math,time
import numpy as np
import pcbnew as p
ROOT=Path(__file__).resolve().parent
NAME='yoklama-dip-v1';STEP=.254;W=601;H=741
board=p.LoadBoard(str(ROOT/(NAME+'.kicad_pcb')))
data=json.loads((ROOT/'design.json').read_text())
def parse(s):
    st=[];roots=[]
    for t in re.findall(r'"(?:[^"\\]|\\.)*"|\(|\)|[^\s()]+',s):
        if t=='(':
            a=[];(st[-1] if st else roots).append(a);st.append(a)
        elif t==')':st.pop()
        else:st[-1].append(json.loads(t) if t.startswith('"') else t)
    return roots[0]
def child(a,k):return next(x for x in a if isinstance(x,list) and x[0]==k)
root=parse((ROOT/'reports/schematic.net').read_text())
expected={}
for n in child(root,'nets')[1:]:
    name=child(n,'name')[1]
    for node in n[1:]:
        if isinstance(node,list) and node[0]=='node':expected[(child(node,'ref')[1],child(node,'pin')[1])]=name
original={(c['ref'],str(pin[0])):'/'+pin[2] for c in data['components'] for pin in c['pins'] if pin[2]}
assert all(expected[k]==v for k,v in original.items()),'Schematic differs from design source'
netmap={n.GetNetname():n for n in board.GetNetsByNetcode().values()}
pads=[];groups={}
for f in board.GetFootprints():
    for pad in f.Pads():
        name=expected.get((f.GetReference(),pad.GetNumber()))
        if name:
            if name not in netmap:
                n=p.NETINFO_ITEM(board,name,len(netmap));board.Add(n);netmap[name]=n
            pad.SetNet(netmap[name])
        x=p.ToMM(pad.GetPosition().x);y=p.ToMM(pad.GetPosition().y)
        num=pad.GetNetCode() if name else -1
        r=p.ToMM(pad.GetSize().x)/2
        square=pad.GetShape()==p.PAD_SHAPE_RECT
        pads.append((x,y,r,square,num))
        if name and not name.startswith('unconnected-'):
            groups.setdefault(name,[]).append((int(round(x/STEP)),int(round(y/STEP))))
objects=[] # copper segments and vias (x1,y1,x2,y2,radius,net,layers)
result=[]
layers=[p.F_Cu,p.B_Cu]

def disk_mask(mask,x1,y1,x2,y2,r,layers=(0,1),square=False):
    xa=max(0,int(math.floor((min(x1,x2)-r)/STEP)));xb=min(W,int(math.ceil((max(x1,x2)+r)/STEP))+1)
    ya=max(0,int(math.floor((min(y1,y2)-r)/STEP)));yb=min(H,int(math.ceil((max(y1,y2)+r)/STEP))+1)
    if xa>=xb or ya>=yb:return
    yy,xx=np.ogrid[ya:yb,xa:xb];xx=xx*STEP;yy=yy*STEP
    if square:hit=(abs(xx-x1)<=r)&(abs(yy-y1)<=r)
    else:
        dx=x2-x1;dy=y2-y1
        if dx==dy==0:hit=(xx-x1)**2+(yy-y1)**2<=r*r
        else:
            t=np.clip(((xx-x1)*dx+(yy-y1)*dy)/(dx*dx+dy*dy),0,1)
            hit=(xx-(x1+t*dx))**2+(yy-(y1+t*dy))**2<=r*r
    for z in layers:mask[z,ya:yb,xa:xb]|=hit

def obstacles(netid,rad):
    m=np.zeros((2,H,W),dtype=np.bool_)
    # Additional .10 mm geometric margin accounts for grid discretization.
    gap=.25+rad+.10
    for x,y,r,sq,n in pads:
        if n!=netid:disk_mask(m,x,y,x,y,r+gap,square=sq)
    for x1,y1,x2,y2,r,n,zs in objects:
        if n!=netid:disk_mask(m,x1,y1,x2,y2,r+gap,zs)
    edge=int(math.ceil((.5+rad+.1)/STEP));m[:,:edge,:]=1;m[:,-edge:,:]=1;m[:,:,:edge]=1;m[:,:,-edge:]=1
    for x1,y1,x2,y2 in data['keepouts']:
        xa=max(0,int((x1-rad-.1)/STEP));xb=min(W,int(math.ceil((x2+rad+.1)/STEP))+1)
        ya=max(0,int((y1-rad-.1)/STEP));yb=min(H,int(math.ceil((y2+rad+.1)/STEP))+1)
        m[:,ya:yb,xa:xb]=1
    return m

def route(a,b,nid,width):
    blocked=obstacles(nid,width/2);vb=obstacles(nid,.5 if width>.5 else .4)
    ax,ay=a;bx,by=b
    if blocked[:,ay,ax].all() or blocked[:,by,bx].all():return None
    dist={};parent={};q=[]
    def h(x,y):
        dx=abs(x-bx);dy=abs(y-by);return 10*max(dx,dy)+4*min(dx,dy)
    for z in (0,1):
        if not blocked[z,ay,ax]:
            s=(z,ay,ax);dist[s]=0;heapq.heappush(q,(h(ax,ay),0,s))
    count=0;through=set(groups[next(n for n in groups if netmap[n].GetNetCode()==nid)])
    while q:
        _,d,s=heapq.heappop(q)
        if dist.get(s)!=d:continue
        z,y,x=s;count+=1
        if (x,y)==b:
            path=[s]
            while s in parent:s=parent[s];path.append(s)
            return path[::-1]
        if count>1100000:return None
        for dx,dy,cost in [(1,0,10),(-1,0,10),(0,1,10),(0,-1,10),(1,1,14),(-1,1,14),(1,-1,14),(-1,-1,14)]:
            nx=x+dx;ny=y+dy
            if not(0<=nx<W and 0<=ny<H) or blocked[z,ny,nx]:continue
            if dx and dy and (blocked[z,y,nx] or blocked[z,ny,x]):continue
            nd=d+cost+(1 if (z==0 and dy) or (z==1 and dx) else 0)
            ns=(z,ny,nx)
            if nd<dist.get(ns,10**12):dist[ns]=nd;parent[ns]=(z,y,x);heapq.heappush(q,(nd+h(nx,ny),nd,ns))
        nz=1-z
        if (x,y) in through:
            possible=not blocked[nz,y,x];cost=1
        else:possible=not vb[:,y,x].any();cost=65
        if possible:
            ns=(nz,y,x);nd=d+cost
            if nd<dist.get(ns,10**12):dist[ns]=nd;parent[ns]=(z,y,x);heapq.heappush(q,(nd+h(x,y),nd,ns))
    return None

def add_path(path,nid,width,through):
    def seg(a,b):
        if a==b:return
        z,y1,x1=a;_,y2,x2=b
        t=p.PCB_TRACK(board);t.SetStart(p.VECTOR2I(p.FromMM(x1*STEP),p.FromMM(y1*STEP)));t.SetEnd(p.VECTOR2I(p.FromMM(x2*STEP),p.FromMM(y2*STEP)));t.SetLayer(layers[z]);t.SetWidth(p.FromMM(width));t.SetNetCode(nid);board.Add(t)
        objects.append((x1*STEP,y1*STEP,x2*STEP,y2*STEP,width/2,nid,(z,)))
    start=path[0];prev=path[0];direction=None
    for s in path[1:]:
        if s[0]!=prev[0]:
            seg(start,prev)
            z,y,x=s
            if (x,y) not in through:
                diameter=1 if width>.5 else .8;drill=.5 if width>.5 else .4
                v=p.PCB_VIA(board);v.SetPosition(p.VECTOR2I(p.FromMM(x*STEP),p.FromMM(y*STEP)));v.SetWidth(p.FromMM(diameter));v.SetDrill(p.FromMM(drill));v.SetViaType(p.VIATYPE_THROUGH);v.SetLayerPair(p.F_Cu,p.B_Cu);v.SetNetCode(nid);board.Add(v)
                objects.append((x*STEP,y*STEP,x*STEP,y*STEP,diameter/2,nid,(0,1)))
            start=s;direction=None
        else:
            delta=(s[1]-prev[1],s[2]-prev[2])
            if direction is not None and delta!=direction:seg(start,prev);start=prev
            direction=delta
        prev=s
    seg(start,prev)

# Minimum spanning trees of terminal pads; routes may merge on the same net.
edges=[]
for name,points in groups.items():
    connected=[points[0]];todo=points[1:]
    while todo:
        _,a,b=min((math.dist(a,b),a,b) for a in connected for b in todo)
        edges.append((name,a,b));connected.append(b);todo.remove(b)
# Wide power distribution first, then short local signals and longer SPI traces.
edges.sort(key=lambda e:(0 if e[0] in ('/GND','/+3V3','/+5V_USB') else 1,math.dist(e[1],e[2])))
failed=[];started=time.time()
for i,(name,a,b) in enumerate(edges,1):
    width=.8 if name in ('/GND','/+3V3','/+5V_USB') else .3
    nid=netmap[name].GetNetCode();path=route(a,b,nid,width)
    if path is None:
        failed.append((name,a,b));print(f'{i}/{len(edges)} FAILED {name} {a}->{b}',flush=True)
    else:
        add_path(path,nid,width,set(groups[name]));result.append({'net':name,'a':a,'b':b,'width_mm':width,'steps':len(path)});print(f'{i}/{len(edges)} OK {name} ({len(path)} grid steps)',flush=True)
    if i%10==0:p.SaveBoard(str(ROOT/(NAME+'.kicad_pcb')),board)
p.SaveBoard(str(ROOT/(NAME+'.kicad_pcb')),board)
(ROOT/'reports/routing.json').write_text(json.dumps({'elapsed_s':time.time()-started,'routed':result,'failed':failed},indent=2))
print(f'Finished: {len(result)} routed, {len(failed)} failed, {time.time()-started:.1f}s',flush=True)
