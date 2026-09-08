// Recolors the native KiCad vector export for a readable bare-board preview.
// Geometry is unchanged; this is not an assembled-board photograph.
const fs=require('fs'),path=require('path');
const sharp=require('C:/Users/ardas/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
const root=path.join(__dirname,'exports');
let svg=fs.readFileSync(path.join(root,'assembly.svg'),'utf8');
svg=svg.replaceAll('#D864FF','#D5B56B').replaceAll('#F2EDA1','#F7F4E8').replaceAll('#D0D2CD','#C4DDCD').replaceAll('#FFFFFF','#143E30');
svg=svg.replace(/(<desc>[^<]*<\/desc>)/,'$1\n<rect x="0" y="0" width="152.4" height="187.96" fill="#143E30"/>');
fs.writeFileSync(path.join(root,'preview.svg'),svg);
Promise.all([
 sharp(Buffer.from(svg),{density:200}).flatten({background:'#143E30'}).png().toFile(path.join(root,'preview.png')),
 sharp(path.join(root,'yoklama-dip-v1.svg'),{density:100}).flatten({background:'#ffffff'}).png().toFile(path.join(root,'yoklama-dip-v1.png')),
 sharp(path.join(root,'board.svg'),{density:140}).flatten({background:'#ffffff'}).png().toFile(path.join(root,'board.png'))
]).then(()=>console.log('Native CAD previews rendered.'));
