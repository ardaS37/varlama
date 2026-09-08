from pathlib import Path
import subprocess,zipfile,json,hashlib
ROOT=Path(__file__).resolve().parent
CLI=Path('C:/Program Files/KiCad/10.0/bin/kicad-cli.exe')
board=str(ROOT/'yoklama-dip-v1.kicad_pcb')
def run(args):
    r=subprocess.run([str(CLI),*args],capture_output=True,text=True)
    if r.returncode:raise RuntimeError(r.stdout+'\n'+r.stderr)
    print('OK: '+' '.join(args[:3]))
run(['pcb','export','svg','--layers','F.Mask,F.SilkS,Edge.Cuts','--mode-single','--fit-page-to-board','--exclude-drawing-sheet','-o',str(ROOT/'exports/assembly.svg'),board])
run(['pcb','export','svg','--layers','F.Cu,B.Cu,F.SilkS,Edge.Cuts','--mode-single','--fit-page-to-board','--exclude-drawing-sheet','-o',str(ROOT/'exports/board.svg'),board])
run(['sch','export','svg','-o',str(ROOT/'exports')+'/',str(ROOT/'yoklama-dip-v1.kicad_sch')])
run(['pcb','export','gerbers','--layers','F.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,Edge.Cuts','--subtract-soldermask','--check-zones','-o',str(ROOT/'gerbers')+'/',board])
run(['pcb','export','drill','--excellon-separate-th','--generate-report','--report-path',str(ROOT/'reports/drill.rpt'),'-o',str(ROOT/'gerbers')+'/',board])
gerbers=list((ROOT/'gerbers').iterdir())
assert len([f for f in gerbers if f.suffix=='.drl'])==2
assert len([f for f in gerbers if f.suffix in ('.gtl','.gbl','.gto','.gbo','.gts','.gbs','.gm1')])==7
manifest={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in gerbers}
(ROOT/'reports/manufacturing-sha256.json').write_text(json.dumps(manifest,indent=2))
with zipfile.ZipFile(ROOT/'yoklama-dip-v1-gerber.zip','w',zipfile.ZIP_DEFLATED) as z:
    for f in gerbers:z.write(f,f.name)
print('Gerber archive verified: 7 copper/mask/silk/outline files + PTH/NPTH drills.')
