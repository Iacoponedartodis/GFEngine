// gdd_docx2md.js — rigenera ProjectDocs/29_GDD.md da Galactic_Front_GDD.docx.
// Non fa parte della build (nessun target CMake la invoca): è uno strumento di
// manutenzione dei ProjectDocs, non del gioco. Vedi 23_GameDesignBridge.md
// ("Dove vive il GDD") per il razionale e il flusso di rigenerazione completo.
//
// Uso (da PowerShell/Bash, richiede `unzip` e Node):
//   unzip -o Galactic_Front_GDD.docx -d <tmp>
//   node tools/gdd_docx2md.js <tmp> <output.md>
//
// Un .docx è uno zip: <tmp>/word/document.xml contiene il testo con lo stile
// dei paragrafi (w:pStyle Heading1/2/3 -> ##/###/####) e le tabelle (<w:tbl>).
// Un semplice strip dei tag XML appiattisce tutto in prosa continua — per
// questo serve un parser dedicato invece di `sed`/regex generiche.
//
// DOPO la rigenerazione, verificare SEMPRE prima di sovrascrivere 29_GDD.md:
//   1. conteggio parole del .md (tag rimossi) == conteggio parole del testo grezzo
//   2. l'ultima riga del documento originale è presente nell'output
// (vedi 07_Changelog, voce "GDD convertito in 29_GDD.md", per i numeri di riferimento).
const fs = require('fs');
const SP = process.argv[2];
const OUT = process.argv[3];
const xml = fs.readFileSync(SP + '/word/document.xml', 'utf8');

function unesc(s) {
  return s.replace(/&amp;/g,'&').replace(/&lt;/g,'<').replace(/&gt;/g,'>')
          .replace(/&quot;/g,'"').replace(/&#39;/g,"'").replace(/&apos;/g,"'");
}

// Estrae il testo run-by-run di un blocco (paragrafo o cella), gestendo w:tab e grassetto/corsivo.
function runsText(block) {
  let out = '';
  const runRe = /<w:r\b[^>]*>([\s\S]*?)<\/w:r>/g;
  let rm;
  while ((rm = runRe.exec(block))) {
    const run = rm[1];
    const bold = /<w:b\/>|<w:b w:val="(?!false|0)/.test(run);
    const italic = /<w:i\/>|<w:i w:val="(?!false|0)/.test(run);
    let t = '';
    const tRe = /<w:t[^>]*>([\s\S]*?)<\/w:t>/g;
    let tm;
    while ((tm = tRe.exec(run))) t += unesc(tm[1]);
    if (/<w:tab\/>/.test(run)) t += '\t';
    if (t) {
      if (bold && italic) t = `***${t}***`;
      else if (bold) t = `**${t}**`;
      else if (italic) t = `*${t}*`;
      out += t;
    }
  }
  return out.trim();
}

// Splitta il documento in paragrafi (w:p) e tabelle (w:tbl) preservando l'ordine.
const bodyMatch = xml.match(/<w:body>([\s\S]*)<\/w:body>/);
const body = bodyMatch[1];

const blocks = [];
let pos = 0;
const blockRe = /<w:p\b[^>]*>[\s\S]*?<\/w:p>|<w:tbl>[\s\S]*?<\/w:tbl>/g;
let m;
while ((m = blockRe.exec(body))) blocks.push(m[0]);

let md = [];
for (const block of blocks) {
  if (block.startsWith('<w:tbl>')) {
    // Tabella: ogni w:tr = riga, ogni w:tc = cella
    const rows = [...block.matchAll(/<w:tr\b[^>]*>([\s\S]*?)<\/w:tr>/g)].map(r => {
      const cells = [...r[1].matchAll(/<w:tc\b[^>]*>([\s\S]*?)<\/w:tc>/g)].map(c => {
        const paras = [...c[1].matchAll(/<w:p\b[^>]*>([\s\S]*?)<\/w:p>/g)].map(p => runsText(p[1]));
        return paras.filter(Boolean).join(' — ') || ' ';
      });
      return cells;
    });
    if (rows.length) {
      md.push('');
      md.push('| ' + rows[0].join(' | ') + ' |');
      md.push('| ' + rows[0].map(() => '---').join(' | ') + ' |');
      for (let i = 1; i < rows.length; i++)
        md.push('| ' + rows[i].join(' | ') + ' |');
      md.push('');
    }
    continue;
  }
  // Paragrafo
  const styleMatch = block.match(/w:pStyle w:val="(Heading[123])"/);
  const text = runsText(block);
  if (!text) continue;
  if (styleMatch) {
    const level = { Heading1: '##', Heading2: '###', Heading3: '####' }[styleMatch[1]];
    md.push('');
    md.push(`${level} ${text}`);
  } else if (/w:pStyle w:val="ListParagraph"/.test(block)) {
    md.push(`- ${text}`);
  } else {
    md.push(text);
  }
}

fs.writeFileSync(OUT, md.join('\n').replace(/\n{3,}/g, '\n\n') + '\n');
console.log('Scritte', md.length, 'righe markdown ->', OUT);
