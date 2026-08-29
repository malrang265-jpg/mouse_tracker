const DEFAULTS={
 gestureEnabled:true,showTrail:true,sensitivity:18,gestureButton:"right",
 restoreContextMenu:true,restoreTextSelection:true,restoreDrag:true,restoreImageDrag:true,
 gestures:{}
};
const ACTIONS={
 none:"사용 안 함",back:"뒤로",forward:"앞으로",reload:"새로고침",hardReload:"강력 새로고침",
 newTab:"새 탭",closeTab:"탭 닫기",reopenClosedTab:"닫은 탭 다시 열기",duplicateTab:"탭 복제",
 nextTab:"다음 탭",previousTab:"이전 탭",firstTab:"첫 번째 탭",lastTab:"마지막 탭",
 nextWindow:"다음 창",previousWindow:"이전 창",newWindow:"새 창",incognitoWindow:"시크릿 창",closeWindow:"창 닫기",
 pinTab:"탭 고정",unpinTab:"탭 고정 해제",muteTab:"탭 음소거",unmuteTab:"탭 음소거 해제",
 zoomIn:"확대",zoomOut:"축소",zoomReset:"확대/축소 초기화",
 scrollTop:"페이지 맨 위",scrollBottom:"페이지 맨 아래",scrollUp:"페이지 위로",scrollDown:"페이지 아래로",
 pageBack:"페이지 한 화면 위",pageForward:"페이지 한 화면 아래",openOptions:"설정 열기"
};
const GESTURES=[];
const ARROW={L:"←",R:"→",U:"↑",D:"↓"};
let settings,drawPoints=[],drawing=false,selectedGesture="";
const pad=document.getElementById("pad"),ctx=pad.getContext("2d");

// Keep the canvas's internal pixel grid matched to how large it's actually
// rendered on screen. Without this, pos() below silently rescales real
// mouse-pixel distances into a smaller internal space, so a gesture drawn
// here needs different real hand movement than the same gesture drawn on a
// live page (content.js decodes raw, unscaled screen pixels).
function syncPadSize(){
 const r=pad.getBoundingClientRect();
 const w=Math.round(r.width),h=Math.round(r.height);
 if(w>0&&h>0&&(pad.width!==w||pad.height!==h)){pad.width=w;pad.height=h;return true;}
 return false;
}
syncPadSize();
window.addEventListener("resize",()=>{
 if(syncPadSize()){drawPoints=[];selectedGesture="";redraw();updateDraw();}
});

function pos(e){
 const r=pad.getBoundingClientRect();
 return{x:(e.clientX-r.left)*pad.width/r.width,y:(e.clientY-r.top)*pad.height/r.height};
}

/* 인식 로직 (content.js의 buildGesture()와 동일한 규칙을 사용):
   - 원시 점을 일정 거리 이상일 때만 저장
   - 이동 경로를 4방향(상하좌우)으로 양자화
   - 연속 동일 방향 제거
*/
function decode(points){
 if(points.length<2)return "";
 const sens=Math.max(8,Number(settings?.sensitivity||18));

 // Lightly resample the stroke by distance so fast drawing is not tied to
 // Chrome's mousemove event frequency.
 const p=[points[0]];
 for(let i=1;i<points.length;i++){
   const q=p[p.length-1];
   if(Math.hypot(points[i].x-q.x,points[i].y-q.y)>=2.5) p.push(points[i]);
 }
 if(p.length<2)return "";

 // Build a direction sequence from a moving baseline: a new direction is
 // accepted once the stroke has traveled minLen from the last turn. Kept in
 // step with content.js's buildGesture()/gestureMinLen so a gesture drawn
 // here decodes the same way it will on a live page.
 const minLen=Math.max(26,sens*1.7);
 const dirs=[];
 let anchor=0;
 let lastDir="";

 for(let i=1;i<p.length;i++){
   const dx=p[i].x-p[anchor].x, dy=p[i].y-p[anchor].y;
   const len=Math.hypot(dx,dy);
   if(len<minLen) continue;

   const dominant=Math.abs(dx)>=Math.abs(dy)
      ? (dx>=0?"R":"L") : (dy>=0?"D":"U");

   if(dominant!==lastDir){
     dirs.push({d:dominant, at:i});
     lastDir=dominant;
     anchor=i;
   }
 }

 // If the final segment was not long enough to be emitted, inspect the
 // overall endpoint direction. This catches very fast short final strokes.
 const tail=p[p.length-1];
 if(anchor<p.length-1){
   const dx=tail.x-p[anchor].x,dy=tail.y-p[anchor].y,len=Math.hypot(dx,dy);
   if(len>=minLen*0.42){
     const d=Math.abs(dx)>=Math.abs(dy)?(dx>=0?"R":"L"):(dy>=0?"D":"U");
     if(d!==lastDir) dirs.push({d,at:p.length-1});
   }
 }

 let result=dirs.map(x=>x.d);

 // Remove accidental one-frame reversals, but do not collapse legitimate
 // three-part gestures such as L-U-R.
 for(let i=1;i<result.length-1;i++){
   if(result[i-1]===result[i+1]){
     result.splice(i,1); i--;
   }
 }
 return result.join("");
}

function resolvePreviewGesture(raw){
 if(!raw || raw.length>4)return null;
 const map=settings&&settings.gestures?settings.gestures:{};
 let best=null;
 Object.keys(map).forEach(g=>{
   if(g && map[g]!=="none" && raw.startsWith(g) && (!best||g.length>best.gesture.length))
     best={gesture:g,action:map[g]};
 });
 return best;
}

function redraw(){
 ctx.clearRect(0,0,pad.width,pad.height);
 ctx.fillStyle="#68717d";ctx.font="16px Arial";
 if(!drawPoints.length){ctx.fillText("여기에 마우스로 제스처를 그려보세요",18,30);return;}
 if(drawPoints.length<2)return;
 ctx.beginPath();ctx.moveTo(drawPoints[0].x,drawPoints[0].y);
 if(drawPoints.length===2)ctx.lineTo(drawPoints[1].x,drawPoints[1].y);
 else{
   for(let i=1;i<drawPoints.length-1;i++){
     const a=drawPoints[i],b=drawPoints[i+1];
     ctx.quadraticCurveTo(a.x,a.y,(a.x+b.x)/2,(a.y+b.y)/2);
   }
   const a=drawPoints[drawPoints.length-2],b=drawPoints[drawPoints.length-1];
   ctx.quadraticCurveTo(a.x,a.y,b.x,b.y);
 }
 ctx.lineWidth=5;ctx.lineCap="round";ctx.lineJoin="round";ctx.strokeStyle="#2563eb";ctx.stroke();
}
function updateDraw(){
 selectedGesture=decode(drawPoints);
 document.getElementById("drawGesture").textContent=selectedGesture||"제스처 없음";
 const arrowsEl=document.getElementById("drawArrows");
 if(!selectedGesture){
   arrowsEl.textContent="마우스로 선을 그려보세요.";
   return;
 }
 const arrows=selectedGesture.split("").map(x=>ARROW[x]).join(" ");
 let extra;
 if(selectedGesture.length>4){
   extra=" — 4방향 초과로 이 제스처는 취소됩니다.";
 }else{
   const match=resolvePreviewGesture(selectedGesture);
   extra=match?` — "${match.gesture}" 등록됨 → ${ACTIONS[match.action]||match.action}`:" — 일치하는 등록 제스처 없음";
 }
 arrowsEl.textContent=arrows+extra;
}
function begin(e){
 drawing=true;
 drawPoints=[pos(e)];
 pad.setPointerCapture?.(e.pointerId);
 redraw();updateDraw();
 e.preventDefault();
}
function move(e){
 if(!drawing)return;
 const p=pos(e);
 const q=drawPoints[drawPoints.length-1];
 if(Math.hypot(p.x-q.x,p.y-q.y)>=3){
   drawPoints.push(p);
   redraw();updateDraw();
 }
 e.preventDefault();
}
function finish(e){
 if(!drawing)return;
 drawing=false;
 try{pad.releasePointerCapture?.(e.pointerId)}catch(_){}
 updateDraw();
 e.preventDefault();
 e.stopPropagation();
}
pad.addEventListener("pointerdown",begin);
pad.addEventListener("pointermove",move);
pad.addEventListener("pointerup",finish);
pad.addEventListener("pointercancel",finish);
pad.addEventListener("contextmenu",e=>{
 // 그리기 영역에서는 우클릭 메뉴를 항상 차단
 e.preventDefault();e.stopImmediatePropagation();
});

document.getElementById("clear").onclick=()=>{
 drawPoints=[];selectedGesture="";redraw();updateDraw();
};

document.getElementById("use").onclick=()=>{
 if(!selectedGesture){alert("먼저 제스처를 그려주세요.");return;}
 let sel=document.querySelector(`select[data-gesture="${CSS.escape(selectedGesture)}"]`);
 if(!sel){
   addGestureRow(selectedGesture,"none");
   sel=document.querySelector(`select[data-gesture="${CSS.escape(selectedGesture)}"]`);
 }
 sel.focus();
 sel.scrollIntoView({behavior:"smooth",block:"center"});
 sel.classList.add("flash");
 setTimeout(()=>sel.classList.remove("flash"),1000);
};

function addGestureRow(g,value){
 const list=document.getElementById("gestureList");
 if(!list)return;
 const old=list.querySelector(`.row[data-gesture="${CSS.escape(g)}"]`);
 if(old)old.remove();

 const row=document.createElement("div");
 row.className="row";row.dataset.gesture=g;

 const b=document.createElement("b");b.textContent=g;
 const sp=document.createElement("span");sp.className="desc";
 sp.textContent=g.split("").map(x=>ARROW[x]||x).join(" ");

 const sel=document.createElement("select");sel.dataset.gesture=g;
 for(const [v,l] of Object.entries(ACTIONS)){
   const o=document.createElement("option");o.value=v;o.textContent=l;sel.appendChild(o);
 }
 sel.value=value||"none";

 const del=document.createElement("button");
 del.type="button";del.textContent="삭제";del.className="deleteGesture";
 del.addEventListener("click",async()=>{
   delete settings.gestures[g];
   await chrome.storage.local.set({settings});
   row.remove();
 });

 sel.addEventListener("change",async()=>{
   settings.gestures[g]=sel.value;
   await chrome.storage.local.set({settings});
 });

 row.append(b,sp,sel,del);
 list.appendChild(row);
}


async function load(){
 const stored=await chrome.storage.local.get(DEFAULTS);
 settings={...DEFAULTS,...stored};

 // 1.7 migration: previous releases shipped built-in gestures.
 // Clear the old mapping once so no old/default gesture can execute.
 const migration=await chrome.storage.local.get("gestureSchemaVersion");
 if((migration.gestureSchemaVersion||0)<3){
   settings.gestures={};
   await chrome.storage.local.set({gestures:{},gestureSchemaVersion:3});
 } else {
   settings.gestures=stored.gestures||{};
 }

 ["gestureEnabled","showTrail","restoreContextMenu","restoreTextSelection","restoreDrag","restoreImageDrag"]
   .forEach(id=>document.getElementById(id).checked=!!settings[id]);
 document.getElementById("gestureButton").value=settings.gestureButton;
 document.getElementById("sensitivity").value=settings.sensitivity;
 document.getElementById("sensOut").value=settings.sensitivity;

 const gestureList=document.getElementById("gestureList");
 if(!gestureList) return;
 gestureList.innerHTML="";
 Object.keys(settings.gestures).forEach(g=>addGestureRow(g,settings.gestures[g]));
 redraw();updateDraw();
}

document.addEventListener("change",async e=>{
 if(!e.target.matches("select[data-gesture]")) return;
 const gestures={};
 document.querySelectorAll("select[data-gesture]").forEach(s=>{
   if(s.value!=="none") gestures[s.dataset.gesture]=s.value;
 });
 await chrome.storage.local.set({gestures});
 const s=document.getElementById("status");
 s.textContent="제스처가 등록되었습니다.";
 setTimeout(()=>s.textContent="",1400);
});

document.getElementById("sensitivity").oninput=e=>{
 document.getElementById("sensOut").value=e.target.value;
};
document.getElementById("save").onclick=async()=>{
 const gestures={};
 document.querySelectorAll("select[data-gesture]").forEach(s=>{
   if(s.value!=="none")gestures[s.dataset.gesture]=s.value;
 });
 await chrome.storage.local.set({
  gestureEnabled:document.getElementById("gestureEnabled").checked,
  showTrail:document.getElementById("showTrail").checked,
  sensitivity:Number(document.getElementById("sensitivity").value),
  gestureButton:document.getElementById("gestureButton").value,
  restoreContextMenu:document.getElementById("restoreContextMenu").checked,
  restoreTextSelection:document.getElementById("restoreTextSelection").checked,
  restoreDrag:document.getElementById("restoreDrag").checked,
  restoreImageDrag:document.getElementById("restoreImageDrag").checked,
  gestures
 });
 const s=document.getElementById("status");s.textContent="저장되었습니다.";
 setTimeout(()=>s.textContent="",1600);
};
load();
