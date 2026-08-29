(() => {
  const DEFAULTS = {
    gestureEnabled: true, showTrail: true, sensitivity: 18, gestureButton: "right",
    restoreContextMenu: true, restoreTextSelection: true, restoreDrag: true,
    restoreImageDrag: true,
    gestures: {}
  };

  let settings = {...DEFAULTS};
  let tracking = false;
  let points = [];
  let trail = null;
  let lastDirection = "";
  let moved = false;

  chrome.storage.local.get(DEFAULTS).then(v => settings = {...DEFAULTS, ...v});

  chrome.storage.onChanged.addListener(changes => {
    for (const [k, c] of Object.entries(changes)) settings[k] = c.newValue;
    applyUnlockStyle();
  });

  const buttonCode = () =>
    settings.gestureButton === "left" ? 0 :
    settings.gestureButton === "middle" ? 1 : 2;

  function isEditable(el) {
    return !!el?.closest?.('input, textarea, select, [contenteditable="true"], [contenteditable=""]');
  }

  function createTrail() {
    if (trail || !settings.showTrail || !document.documentElement) return;
    trail = document.createElement("div");
    trail.id = "__mg_trail";
    trail.style.cssText =
      "position:fixed;inset:0;pointer-events:none;z-index:2147483647;";
    document.documentElement.appendChild(trail);
  }

  function addTrailPoint(x, y) {
    if (!trail || !settings.showTrail || points.length < 2) return;
    const a = points[points.length - 2];
    const b = points[points.length - 1];
    const dx = b.x - a.x, dy = b.y - a.y;
    const len = Math.hypot(dx, dy);
    if (len < 1) return;
    const seg = document.createElement("div");
    seg.style.cssText =
      `position:fixed;left:${a.x}px;top:${a.y}px;width:${len}px;height:4px;` +
      `background:#2d7ff9;border-radius:4px;transform-origin:0 50%;` +
      `transform:rotate(${Math.atan2(dy,dx)}rad);`;
    trail.appendChild(seg);
  }

  function clearTrail() {
    trail?.remove();
    trail = null;
  }

  // Minimum on-screen travel (CSS px) before a stroke segment counts as a
  // new gesture direction. Shared with move()'s "moved" threshold below so
  // native browser behavior is only suppressed once a stroke is long enough
  // to plausibly become a gesture (avoids a dead zone where the context
  // menu is blocked but no gesture ends up firing).
  function gestureMinLen(sens){
    return Math.max(26, sens*1.7);
  }

  function direction(a,b){
    const dx=b.x-a.x,dy=b.y-a.y,len=Math.hypot(dx,dy);
    if(len<8)return "";
    return Math.abs(dx)>=Math.abs(dy)?(dx>=0?"R":"L"):(dy>=0?"D":"U");
  }

  function buildGesture(){
    if(points.length<2)return "";
    const sens=Math.max(8,Number(settings.sensitivity||18));
    const minLen=gestureMinLen(sens);
    const p=[points[0]];
    for(let i=1;i<points.length;i++){
      const q=p[p.length-1];
      if(Math.hypot(points[i].x-q.x,points[i].y-q.y)>=2)p.push(points[i]);
    }
    if(p.length<2)return "";

    const out=[];
    let anchor=0,last="";
    for(let i=1;i<p.length;i++){
      const dx=p[i].x-p[anchor].x,dy=p[i].y-p[anchor].y;
      const len=Math.hypot(dx,dy);
      if(len<minLen)continue;
      const d=Math.abs(dx)>=Math.abs(dy)?(dx>=0?"R":"L"):(dy>=0?"D":"U");
      if(d!==last){out.push(d);last=d;anchor=i;}
    }
    const tail=p[p.length-1];
    if(anchor<p.length-1){
      const d=direction(p[anchor],tail);
      if(d&&d!==last&&Math.hypot(tail.x-p[anchor].x,tail.y-p[anchor].y)>=minLen*.35)out.push(d);
    }
    for(let i=1;i<out.length-1;i++){
      if(out[i-1]===out[i+1]){out.splice(i,1);i--;}
    }
    return out.join("");
  }

  // Choose the longest registered gesture that is a prefix of the
  // gesture actually drawn. Exact matches naturally win because they are longest.
  function resolveRegisteredGesture(rawGesture) {
    // Safety/intent rule: a gesture with more than 4 direction actions is cancelled.
    if (!rawGesture || rawGesture.length > 4) return null;
    const map = settings && settings.gestures ? settings.gestures : {};
    if (!map) return null;

    let best = null;
    for (const key of Object.keys(map)) {
      if (!key || map[key] === "none") continue;
      if (rawGesture.startsWith(key)) {
        if (!best || key.length > best.gesture.length) {
          best = {gesture:key, action:map[key]};
        }
      }
    }
    return best;
  }

  function start(e) {
    if (!settings.gestureEnabled || e.button !== buttonCode()) return;
    if (isEditable(e.target)) return;

    tracking = true;
    moved = false;
    points = [{x:e.clientX,y:e.clientY}];
    lastDirection = "";
    createTrail();
    addTrailPoint(e.clientX,e.clientY);

    // Do NOT cancel the event here. A normal click must continue to work.
  }

  function move(e) {
    if (!tracking) return;
    const p = {x:e.clientX,y:e.clientY};
    points.push(p);
    const sens = Math.max(8, Number(settings.sensitivity || 18));
    if (Math.hypot(e.clientX-points[0].x, e.clientY-points[0].y) >= gestureMinLen(sens)) moved = true;
    addTrailPoint(p.x,p.y);

    // Only suppress browser/site behavior after the pointer has actually moved.
    if (moved) e.preventDefault();
  }

  function end(e) {
    if (!tracking) return;
    tracking = false;

    const gesture = buildGesture();
    const resolved = resolveRegisteredGesture(gesture);
    const action = (gesture.length > 4) ? null : (resolved ? resolved.action : null);
    clearTrail();
    if (action && gesture.length) {
      // A recognized right-button gesture must never open the context menu.
      e.preventDefault();
      e.stopPropagation();
      chrome.runtime.sendMessage({
        type:["hardReload","scrollTop","scrollBottom","scrollUp","scrollDown","pageBack","pageForward"].includes(action)
          ?"gestureActionExtended":"gestureAction", action
      }).catch(()=>{});
    }
    // A plain right click (no movement) is deliberately left untouched.
  }

  function allowContextMenu(e) {
    if (settings.gestureButton === "right" && moved) { e.preventDefault(); e.stopImmediatePropagation(); return; }
    if (settings.restoreContextMenu) e.stopImmediatePropagation();
  }
  function allowSelection(e) { if (settings.restoreTextSelection) e.stopImmediatePropagation(); }
  function allowDrag(e) { if (settings.restoreDrag || settings.restoreImageDrag) e.stopImmediatePropagation(); }

  window.addEventListener("contextmenu", allowContextMenu, true);
  document.addEventListener("contextmenu", allowContextMenu, true);
  window.addEventListener("selectstart", allowSelection, true);
  document.addEventListener("selectstart", allowSelection, true);
  window.addEventListener("dragstart", allowDrag, true);
  document.addEventListener("dragstart", allowDrag, true);

  document.addEventListener("mousedown", start, true);
  document.addEventListener("mousemove", move, true);
  document.addEventListener("mouseup", end, true);


  function applyUnlockStyle() {
    if (!document.documentElement) return;

    let style = document.getElementById("__mg_unlock_style");
    if (!style) {
      style = document.createElement("style");
      style.id = "__mg_unlock_style";
      (document.head || document.documentElement).appendChild(style);
    }

    let css = "";
    if (settings.restoreTextSelection)
      css += 'html, body, body * { -webkit-user-select: text !important; user-select: text !important; }\\n';
    if (settings.restoreDrag)
      css += 'html, body, body * { -webkit-user-drag: auto !important; }\\n';
    if (settings.restoreImageDrag)
      css += 'img, a img { -webkit-user-drag: auto !important; }\\n';

    if (style.textContent !== css) style.textContent = css;
  }

  // Apply once. No MutationObserver: this avoids a style/observer infinite mutation loop.
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", applyUnlockStyle, {once:true});
  } else {
    applyUnlockStyle();
  }
})();
  chrome.runtime.onMessage.addListener(msg => {
    if (msg?.type === "scroll") {
      window.scrollTo({
        top: msg.action === "scrollTop" ? 0 : document.documentElement.scrollHeight,
        behavior: "smooth"
      });
    }
  });
