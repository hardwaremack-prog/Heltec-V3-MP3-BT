#pragma once
// Single-page control panel served at http://<ip>/
const char WEB_UI[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0f14"><title>Heltec MP3</title>
<style>
:root{--bg:#0b0f14;--p:#121922;--l:#223041;--t:#d7e3ee;--d:#7f93a8;--o:#7fe3ff;--a:#3aa0ff;--ok:#6be38a;--bad:#ff6b6b;--w:#ffcc66}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--t);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding-bottom:84px}
header{position:sticky;top:0;z-index:5;background:#0b0f14ee;backdrop-filter:blur(8px);border-bottom:1px solid var(--l);padding:12px 16px;display:flex;align-items:center;gap:10px}
header b{font-size:18px}header b span{color:var(--o)}
.dot{width:9px;height:9px;border-radius:50%;background:var(--d);display:inline-block}
.chip{font-size:12px;color:var(--d);border:1px solid var(--l);border-radius:99px;padding:1px 9px;display:flex;gap:6px;align-items:center}
main{max-width:720px;margin:0 auto;padding:14px}
.card{background:var(--p);border:1px solid var(--l);border-radius:14px;padding:16px;margin-bottom:14px}
h2{font-size:16px;margin:0 0 10px}
.tabs{position:fixed;bottom:0;left:0;right:0;display:flex;background:#0e141bf2;border-top:1px solid var(--l);padding-bottom:env(safe-area-inset-bottom);z-index:5}
.tabs button{flex:1;background:none;border:0;color:var(--d);padding:10px 0 12px;font:600 12px system-ui;display:flex;flex-direction:column;align-items:center;gap:3px;cursor:pointer}
.tabs button.on{color:var(--o)}.tabs svg{width:22px;height:22px}
.pg{display:none}.pg.on{display:block}
button.b{font:600 14px system-ui;border:0;border-radius:10px;padding:10px 14px;background:var(--a);color:#fff;cursor:pointer}
button.g{background:transparent;border:1px solid var(--l);color:var(--t)}
button.r{background:#3a1820;color:#ffb3b3;border:1px solid #5a2630}
button:disabled{opacity:.4}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
input[type=text],input[type=password],select{width:100%;background:#0a1018;border:1px solid var(--l);color:var(--t);border-radius:10px;padding:10px;font:inherit}
.np-t{font-size:22px;font-weight:700;color:#fff;word-break:break-word}
.np-a{color:var(--d);margin-top:2px}
.bar{height:8px;background:#0a0f15;border:1px solid var(--l);border-radius:6px;overflow:hidden;margin:14px 0 6px}
.bar>div{height:100%;width:0;background:linear-gradient(90deg,var(--a),var(--o))}
.ctl{display:flex;justify-content:center;gap:18px;margin:14px 0 6px}
.ctl button{width:58px;height:58px;border-radius:50%;border:1px solid var(--l);background:#0e151d;color:#fff;cursor:pointer;display:grid;place-items:center}
.ctl button.big{width:72px;height:72px;background:var(--a);border:0}
.ctl svg{width:26px;height:26px;fill:currentColor}
input[type=range]{width:100%;accent-color:var(--o)}
.list{border:1px solid var(--l);border-radius:12px;overflow:hidden}
.it{display:flex;align-items:center;gap:10px;padding:11px 12px;border-bottom:1px solid var(--l);cursor:pointer}
.it:last-child{border-bottom:0}.it:hover{background:#16202b}
.it .n{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.it .s{color:var(--d);font-size:12px}.it.cur{color:var(--o)}
.ic{width:20px;height:20px;flex:none;fill:var(--d)}
.x{background:none;border:0;color:var(--d);cursor:pointer;padding:2px 6px;font-size:16px}
.crumb{color:var(--d);font-size:13px;margin-bottom:8px;word-break:break-all}.crumb a{color:var(--o);cursor:pointer}
.muted{color:var(--d);font-size:13px}
.log{background:#070a0e;border:1px solid var(--l);border-radius:10px;padding:8px;height:170px;overflow:auto;font:12px/1.4 ui-monospace,Consolas,monospace;color:#9fb3c6;white-space:pre-wrap}
.note{border-left:3px solid var(--w);background:#1a1710;color:#e9dcc0;padding:8px 12px;border-radius:6px;font-size:13px}
.ok{color:var(--ok)}.bad{color:var(--bad)}
.seg{display:flex;border:1px solid var(--l);border-radius:10px;overflow:hidden}
.seg button{flex:1;background:none;border:0;color:var(--d);padding:8px 4px;font:600 12px system-ui;cursor:pointer}
.seg button.on{background:#1b2a3a;color:var(--o)}
.spin{display:inline-block;width:12px;height:12px;border:2px solid var(--l);border-top-color:var(--o);border-radius:50%;animation:s 1s linear infinite}
@keyframes s{to{transform:rotate(360deg)}}
.toast{position:fixed;left:50%;bottom:96px;transform:translateX(-50%);background:#1b2a3a;border:1px solid var(--l);padding:8px 14px;border-radius:10px;opacity:0;transition:.25s;pointer-events:none;z-index:9}
.toast.on{opacity:1}
</style></head><body>
<header><b>Heltec <span>MP3</span></b><div style="flex:1"></div>
<div class="chip" id="btChip"><span class="dot" id="btDot"></span><span id="btTxt">BT</span></div></header>
<main>

<section class="pg on" id="pg-play">
 <div class="card">
  <div class="np-t" id="title">Nothing playing</div>
  <div class="np-a" id="artist">Pick a song in Library</div>
  <div class="bar"><div id="prog"></div></div>
  <div class="row muted" style="justify-content:space-between"><span id="time">0:00</span><span id="pos"></span></div>
  <div class="ctl">
   <button onclick="cmd('prev')" title="Previous"><svg viewBox="0 0 24 24"><path d="M6 6h2v12H6zm3.5 6 8.5 6V6z"/></svg></button>
   <button class="big" onclick="cmd('toggle')" title="Play/Pause"><svg viewBox="0 0 24 24" id="ppIcon"><path d="M8 5v14l11-7z"/></svg></button>
   <button onclick="cmd('next')" title="Next"><svg viewBox="0 0 24 24"><path d="M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z"/></svg></button>
  </div>
 </div>
 <div class="card">
  <h2>Volume <span class="muted" id="volTxt"></span></h2>
  <input type="range" min="0" max="20" id="vol">
  <h2 style="margin-top:14px">Play mode</h2>
  <div class="seg" id="modes"><button data-m="0">Normal</button><button data-m="1">Repeat all</button><button data-m="2">Repeat one</button><button data-m="3">Shuffle</button></div>
 </div>
</section>

<section class="pg" id="pg-lib">
 <div class="card">
  <div class="crumb" id="crumb"></div>
  <div class="list" id="files"></div>
  <div class="row" style="margin-top:12px">
   <label class="b" style="background:var(--a);padding:10px 14px;border-radius:10px;font:600 14px system-ui;cursor:pointer">Upload MP3s<input type="file" id="up" accept=".mp3,audio/mpeg" multiple hidden></label>
   <button class="b g" onclick="mkdir()">New folder</button>
   <button class="b g" onclick="ls(cur)">Refresh</button>
  </div>
  <div id="upStat" class="muted" style="margin-top:8px"></div>
 </div>
</section>

<section class="pg" id="pg-bt">
 <div class="card">
  <h2>Bluetooth headset</h2>
  <div id="btState" class="row" style="justify-content:space-between"></div>
  <div id="btNoUart" class="note" style="display:none;margin-top:10px"></div>
  <div class="row" style="margin-top:12px" id="btBtns">
   <button class="b" onclick="bt('scan')">Scan for headsets</button>
   <button class="b g" onclick="bt('pair')">Reconnect</button>
   <button class="b g" onclick="bt('disconnect')">Disconnect</button>
  </div>
 </div>
 <div class="card">
  <h2>Found nearby <span id="scanSpin"></span></h2>
  <p class="muted" style="margin-top:0">Put the headset in <b>pairing mode</b>, then tap Scan. Tap a headset to connect to it. The module remembers it.</p>
  <div class="list" id="btFound"></div>
 </div>
 <div class="card">
  <h2>Remembered headsets</h2>
  <div class="list" id="btSaved"></div>
  <p class="muted">Up to 10. With none saved, the module connects to any headset that's in pairing mode.</p>
  <div class="row"><input type="text" id="btName" placeholder="Add by exact name, e.g. JBL TUNE510BT" style="flex:1"><button class="b g" onclick="addName()">Add</button></div>
  <div class="row" style="margin-top:10px"><button class="b r" onclick="if(confirm('Forget all remembered headsets?'))bt('forget')">Forget all</button><button class="b g" onclick="bt('saved')">Refresh</button></div>
 </div>
 <div class="card">
  <h2>Module console</h2>
  <div class="log" id="btLog"></div>
  <div class="row" style="margin-top:8px"><input type="text" id="btRaw" placeholder="AT+GMR?" style="flex:1"><button class="b g" onclick="raw()">Send</button></div>
 </div>
</section>

<section class="pg" id="pg-wifi">
 <div class="card"><h2>Wi-Fi status</h2><div id="wifiStat" class="muted">…</div></div>
 <div class="card">
  <h2>Join a Wi-Fi network</h2>
  <div class="row"><button class="b g" onclick="wscan()">Scan networks</button><span id="wspin"></span></div>
  <div class="list" id="nets" style="margin:10px 0"></div>
  <input type="text" id="ssid" placeholder="Network name (SSID)" style="margin-bottom:8px">
  <input type="password" id="pass" placeholder="Password">
  <div class="row" style="margin-top:10px"><button class="b" onclick="wsave()">Save &amp; restart</button><button class="b r" onclick="wforget()">Forget network (hotspot only)</button></div>
  <p class="muted">If it can't join, it falls back to its own hotspot <b id="apn">Heltec-MP3</b>, password <b>heltecmp3</b>, at 192.168.4.1.</p>
 </div>
</section>

<section class="pg" id="pg-sys">
 <div class="card"><h2>System</h2><div id="sys" class="muted">…</div>
  <div class="row" style="margin-top:12px"><button class="b g" onclick="post('/api/reboot').then(()=>toast('Restarting…'))">Restart</button></div></div>
 <div class="card"><h2>Firmware update</h2>
  <p class="muted" style="margin-top:0">Pick the <b>heltec-mp3-app</b> .bin file (not the full flasher image).</p>
  <input type="file" id="ota" accept=".bin">
  <div class="bar"><div id="otaBar"></div></div><div id="otaStat" class="muted"></div></div>
</section>
</main>

<nav class="tabs">
 <button class="on" data-p="play"><svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 3v10.55A4 4 0 1 0 14 17V7h4V3z"/></svg>Player</button>
 <button data-p="lib"><svg viewBox="0 0 24 24" fill="currentColor"><path d="M10 4H4a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-8z"/></svg>Library</button>
 <button data-p="bt"><svg viewBox="0 0 24 24" fill="currentColor"><path d="M17.7 7.7 12 2h-1v7.6L6.4 5 5 6.4 10.6 12 5 17.6 6.4 19l4.6-4.6V22h1l5.7-5.7-4.3-4.3zM13 5.8l1.9 1.9L13 9.6zm1.9 10.5L13 18.2v-3.8z"/></svg>Bluetooth</button>
 <button data-p="wifi"><svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 21l3.6-4.8a6 6 0 0 0-7.2 0zM1 9l2 2a12.7 12.7 0 0 1 18 0l2-2A15.5 15.5 0 0 0 1 9zm4 4 2 2a7 7 0 0 1 10 0l2-2a9.9 9.9 0 0 0-14 0z"/></svg>Wi-Fi</button>
 <button data-p="sys"><svg viewBox="0 0 24 24" fill="currentColor"><path d="M19.4 13a7.5 7.5 0 0 0 0-2l2.1-1.6-2-3.5-2.5 1a7 7 0 0 0-1.7-1L15 3h-4l-.4 2.7a7 7 0 0 0-1.7 1l-2.5-1-2 3.5L6.6 11a7.5 7.5 0 0 0 0 2l-2.1 1.6 2 3.5 2.5-1a7 7 0 0 0 1.7 1L11 21h4l.4-2.7a7 7 0 0 0 1.7-1l2.5 1 2-3.5zM13 15.5a3.5 3.5 0 1 1 0-7 3.5 3.5 0 0 1 0 7z"/></svg>System</button>
</nav>
<div class="toast" id="toast"></div>

<script>
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const q=o=>Object.entries(o||{}).map(([k,v])=>k+'='+encodeURIComponent(v)).join('&');
async function post(u,o){const r=await fetch(u+(o?'?'+q(o):''),{method:'POST'});const j=await r.json().catch(()=>({}));if(!r.ok||j.ok===false)throw new Error(j.error||r.statusText);return j}
async function get(u,o){const r=await fetch(u+(o?'?'+q(o):''));const j=await r.json();if(!r.ok||j.ok===false)throw new Error(j.error||r.statusText);return j}
let tt;function toast(m){const t=$('toast');t.textContent=m;t.classList.add('on');clearTimeout(tt);tt=setTimeout(()=>t.classList.remove('on'),2200)}
const fmt=s=>Math.floor(s/60)+':'+String(s%60).padStart(2,'0');
let page='play';
document.querySelectorAll('.tabs button').forEach(b=>b.onclick=()=>{
 document.querySelectorAll('.tabs button').forEach(x=>x.classList.toggle('on',x===b));
 page=b.dataset.p;document.querySelectorAll('.pg').forEach(p=>p.classList.toggle('on',p.id==='pg-'+page));
 if(page==='lib')ls(cur);if(page==='bt')btLoad();if(page==='wifi')wLoad();if(page==='sys')sLoad();
});

// ---- player ----
let volDrag=false,st={};
function showStatus(s){st=s;
 $('title').textContent=s.title||(s.file?s.file.replace(/\.mp3$/i,''):'Nothing playing');
 $('artist').textContent=s.artist||(s.playing?(s.dir||''):'Pick a song in Library');
 $('prog').style.width=(s.progress*100)+'%';$('time').textContent=fmt(s.elapsed);
 $('pos').textContent=s.count?s.index+' of '+s.count:'';
 $('ppIcon').innerHTML=s.playing&&!s.paused?'<path d="M6 5h4v14H6zm8 0h4v14h-4z"/>':'<path d="M8 5v14l11-7z"/>';
 if(!volDrag){$('vol').value=s.volume}$('volTxt').textContent=s.volume+' / 20';
 document.querySelectorAll('#modes button').forEach(b=>b.classList.toggle('on',+b.dataset.m===s.mode));
 $('btDot').style.background=s.bt.connected?'var(--ok)':(s.bt.uart?'var(--w)':'var(--d)');
 $('btTxt').textContent=s.bt.connected?'Headset on':'BT';
}
async function cmd(a,v){try{showStatus(await post('/api/cmd',v===undefined?{a}:{a,v}))}catch(e){toast(e.message)}}
$('vol').oninput=()=>{volDrag=true;$('volTxt').textContent=$('vol').value+' / 20'};
$('vol').onchange=()=>{volDrag=false;cmd('vol',$('vol').value)};
document.querySelectorAll('#modes button').forEach(b=>b.onclick=()=>cmd('mode',b.dataset.m));
async function poll(){try{showStatus(await get('/api/status'))}catch(e){}}
setInterval(poll,1000);poll();

// ---- library ----
let cur='/';
const icF='<svg class="ic" viewBox="0 0 24 24"><path d="M10 4H4a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-8z"/></svg>';
const icM='<svg class="ic" viewBox="0 0 24 24"><path d="M12 3v10.55A4 4 0 1 0 14 17V7h4V3z"/></svg>';
const join=(d,n)=>d.endsWith('/')?d+n:d+'/'+n;
async function ls(d){
 try{const j=await get('/api/ls',{dir:d});cur=j.dir;
  const parts=cur.split('/').filter(Boolean);let acc='';
  $('crumb').innerHTML='<a data-d="/">SD card</a>'+parts.map(p=>{acc+='/'+p;return ' / <a data-d="'+esc(acc)+'">'+esc(p)+'</a>'}).join('');
  $('crumb').querySelectorAll('a').forEach(a=>a.onclick=()=>ls(a.dataset.d));
  const e=j.entries.filter(x=>x.d||/\.mp3$/i.test(x.n)).sort((a,b)=>a.d!==b.d?(a.d?-1:1):a.n.localeCompare(b.n));
  const h=[];if(cur!=='/')h.push('<div class="it" data-up="1">'+icF+'<span class="n">..</span></div>');
  e.forEach(x=>{const playing=st.playing&&st.dir===cur&&st.file===x.n;
   h.push('<div class="it'+(playing?' cur':'')+'" data-n="'+esc(x.n)+'" data-d="'+(x.d?1:0)+'">'+(x.d?icF:icM)+'<span class="n">'+esc(x.d?x.n:x.n.replace(/\.mp3$/i,''))+'</span>'+(x.d?'':'<span class="s">'+(x.s/1048576).toFixed(1)+' MB</span>')+'<button class="x" title="Delete">✕</button></div>')});
  if(!h.length)h.push('<div class="it muted">No MP3s here yet. Upload some!</div>');
  $('files').innerHTML=h.join('');
  $('files').querySelectorAll('.it').forEach(el=>{
   el.onclick=async ev=>{
    if(el.dataset.up){ls(cur.substring(0,cur.lastIndexOf('/'))||'/');return}
    const n=el.dataset.n;if(!n)return;
    if(ev.target.classList.contains('x')){ev.stopPropagation();if(!confirm('Delete '+n+'?'))return;
     try{await post('/api/delete',{path:join(cur,n)});ls(cur)}catch(e){toast(e.message)}return}
    if(el.dataset.d==='1')ls(join(cur,n));
    else{try{showStatus(await post('/api/play',{dir:cur,file:n}));toast('Playing '+n.replace(/\.mp3$/i,''));ls(cur)}catch(e){toast(e.message)}}
   }});
 }catch(e){$('files').innerHTML='<div class="it bad">'+esc(e.message)+'</div>'}
}
async function mkdir(){const n=prompt('New folder name');if(!n)return;try{await post('/api/mkdir',{path:join(cur,n.replace(/[\/\\]/g,''))});ls(cur)}catch(e){toast(e.message)}}
$('up').onchange=async()=>{
 const files=[...$('up').files];
 for(let i=0;i<files.length;i++){const f=files[i];
  await new Promise(res=>{const x=new XMLHttpRequest();const fd=new FormData();fd.append('f',f,f.name);
   x.upload.onprogress=e=>{$('upStat').textContent='Uploading '+(i+1)+'/'+files.length+': '+f.name+' '+Math.round(e.loaded/e.total*100)+'%'};
   x.onload=()=>{if(x.status!==200)toast('Upload failed: '+f.name);res()};x.onerror=()=>{toast('Upload failed');res()};
   x.open('POST','/api/upload?'+q({dir:cur}));x.send(fd)})}
 $('upStat').textContent=files.length?'Uploaded '+files.length+' file(s).':'';$('up').value='';ls(cur)};

// ---- bluetooth ----
let btTimer;
function btRender(j){
 const s=j.connected?'<b class="ok">Headset connected</b>':(j.uart?'<b style="color:var(--w)">No headset connected</b>':'<b class="bad">Module not answering</b>');
 $('btState').innerHTML='<span>'+s+'</span><span class="muted">'+(j.version?esc(j.version)+' · '+j.baud+' baud':'')+'</span>';
 $('btNoUart').style.display=j.uart?'none':'block';
 $('btNoUart').innerHTML='Wire the KCX_BT_EMITTER serial pins: module <b>TX → GPIO '+j.rx+'</b>, module <b>RX ← GPIO '+j.tx+'</b> (through a 3.3 V ↔ 5 V level shifter), power it from <b>5V</b>. Then <button class="b g" onclick="bt(\'retry\')" style="padding:4px 10px">Try again</button>';
 document.querySelectorAll('#btBtns button').forEach(b=>b.disabled=!j.uart);
 $('scanSpin').innerHTML=j.scanning?'<span class="spin"></span>':'';
 $('btFound').innerHTML=j.found.length?j.found.map(d=>'<div class="it" data-mac="'+d.mac+'"><span class="n">'+esc(d.name||'(no name)')+'<br><span class="s">'+d.mac.replace(/(..)(?!$)/g,'$1:')+'</span></span>'+(d.saved?'<span class="s ok">saved</span>':'')+'<button class="b" style="padding:6px 12px">Connect</button></div>').join(''):'<div class="it muted">'+(j.scanning?'Searching…':'Nothing yet. Tap Scan.')+'</div>';
 $('btFound').querySelectorAll('[data-mac]').forEach(el=>el.onclick=()=>{bt('connect',{mac:el.dataset.mac});toast('Connecting…')});
 $('btSaved').innerHTML=j.saved.length?j.saved.map(d=>'<div class="it"><span class="n">'+esc(d.name||'(by address)')+'</span><span class="s">'+(d.mac?d.mac.replace(/(..)(?!$)/g,'$1:'):'by name')+'</span></div>').join(''):'<div class="it muted">None saved</div>';
 const lg=$('btLog'),atBottom=lg.scrollTop+lg.clientHeight>=lg.scrollHeight-8;lg.textContent=j.log.join('\n');if(atBottom)lg.scrollTop=lg.scrollHeight;
}
async function btLoad(){clearTimeout(btTimer);try{btRender(await get('/api/bt'))}catch(e){}if(page==='bt')btTimer=setTimeout(btLoad,1500)}
async function bt(a,extra){try{btRender(await post('/api/bt',Object.assign({a},extra||{})))}catch(e){toast(e.message)}}
function addName(){const n=$('btName').value.trim();if(n){bt('addname',{name:n});$('btName').value='';toast('Saved '+n)}}
function raw(){const c=$('btRaw').value.trim();if(c){bt('raw',{cmd:c});$('btRaw').value=''}}
$('btRaw').onkeydown=e=>{if(e.key==='Enter')raw()};

// ---- wifi ----
async function wLoad(){try{const w=await get('/api/wifi');$('apn').textContent=w.apName;
 $('wifiStat').innerHTML=(w.connected?'<b class="ok">Connected to '+esc(w.ssid)+'</b><br>IP '+w.ip+' · signal '+w.rssi+' dBm':'<b>Not on home Wi-Fi</b>'+(w.ssid?' (trying '+esc(w.ssid)+')':''))+(w.ap?'<br>Hotspot <b>'+esc(w.apName)+'</b> at '+w.apIp:'')+'<br>Also at <b>http://heltec-mp3.local</b>';
 if(w.ssid&&!$('ssid').value)$('ssid').value=w.ssid}catch(e){}}
async function wscan(){$('wspin').innerHTML='<span class="spin"></span>';try{const n=await get('/api/wifi/scan');const seen={};
 $('nets').innerHTML=n.filter(x=>x.ssid&&!seen[x.ssid]&&(seen[x.ssid]=1)).sort((a,b)=>b.rssi-a.rssi).map(x=>'<div class="it" data-s="'+esc(x.ssid)+'"><span class="n">'+esc(x.ssid)+'</span><span class="s">'+(x.open?'open · ':'')+x.rssi+' dBm</span></div>').join('')||'<div class="it muted">No networks found</div>';
 $('nets').querySelectorAll('[data-s]').forEach(el=>el.onclick=()=>{$('ssid').value=el.dataset.s;$('pass').focus()})}catch(e){toast(e.message)}$('wspin').innerHTML=''}
async function wsave(){const s=$('ssid').value.trim();if(!s)return toast('Enter a network name');
 try{await post('/api/wifi',{ssid:s,pass:$('pass').value});alert('Saved. The player restarts and joins '+s+'.\nIts new IP shows on the OLED: Home → Wi-Fi.\nOr try http://heltec-mp3.local')}catch(e){toast(e.message)}}
async function wforget(){if(!confirm('Forget the home network and use only the hotspot?'))return;try{await post('/api/wifi',{forget:1});alert('Restarting in hotspot mode.')}catch(e){toast(e.message)}}

// ---- system ----
async function sLoad(){try{const s=await get('/api/sys');const up=s.uptime;
 $('sys').innerHTML='Firmware <b>v'+esc(s.fw)+'</b><br>Up '+Math.floor(up/3600)+'h '+Math.floor(up/60)%60+'m · free memory '+Math.round(s.heap/1024)+' KB'+(s.sdTotal?'<br>SD card '+(s.sdUsed/1024).toFixed(1)+' of '+(s.sdTotal/1024).toFixed(1)+' GB used':'<br>No SD card')}catch(e){}}
$('ota').onchange=()=>{const f=$('ota').files[0];if(!f)return;if(!confirm('Install '+f.name+'?'))return;
 const x=new XMLHttpRequest(),fd=new FormData();fd.append('fw',f,f.name);
 x.upload.onprogress=e=>{const p=Math.round(e.loaded/e.total*100);$('otaBar').style.width=p+'%';$('otaStat').textContent='Uploading '+p+'%'};
 x.onload=()=>{$('otaStat').textContent=x.status===200?'Done! Restarting… reload this page in ~10 s.':'Update failed: '+x.responseText};
 x.onerror=()=>{$('otaStat').textContent='Connection lost'};x.open('POST','/update');x.send(fd)};
</script></body></html>)HTML";
