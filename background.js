const DEFAULTS = {
  gestureEnabled:true, showTrail:true, sensitivity:18, gestureButton:"right",
  restoreContextMenu:true, restoreTextSelection:true, restoreDrag:true,
  restoreImageDrag:true,
  gestures:{}
};

chrome.runtime.onInstalled.addListener(async () => {
  const current = await chrome.storage.local.get(null);
  const next = {...current};
  const defaults = {gestureEnabled:true,showTrail:true,sensitivity:18,gestureButton:"right",
    restoreContextMenu:true,restoreTextSelection:true,restoreDrag:true,restoreImageDrag:true,gestures:{}};
  for (const [k,v] of Object.entries(defaults)) if (!(k in next)) next[k]=v;
  // Remove legacy built-in mappings from earlier versions.
  const legacy = {L:"back",R:"forward",U:"newTab",D:"closeTab",LU:"reload",LD:"scrollTop",RU:"scrollBottom"};
  if (next.gestures && Object.keys(next.gestures).some(k=>legacy[k]===next.gestures[k])) {
    const cleaned={...next.gestures};
    for (const [k,v] of Object.entries(legacy)) if (cleaned[k]===v) delete cleaned[k];
    next.gestures=cleaned;
  }
  const schema = Number(next.gestureSchemaVersion || 0);
  if (schema < 3) { next.gestures = {}; next.gestureSchemaVersion = 3; }
  await chrome.storage.local.set(next);
});

chrome.commands.onCommand.addListener(async command => {
  if (command === "open-options") await chrome.runtime.openOptionsPage();
});

chrome.runtime.onMessage.addListener((msg, sender) => {
  if (msg?.type !== "gestureAction") return;
  const tabId = sender.tab?.id;
  if (!tabId) return;

  (async () => {
    try {
      switch (msg.action) {
        case "back": await chrome.tabs.goBack(tabId); break;
        case "forward": await chrome.tabs.goForward(tabId); break;
        case "hardReload": await chrome.tabs.reload(tabId,{bypassCache:true}); break;
        case "reload": await chrome.tabs.reload(tabId); break;
        case "newTab": await chrome.tabs.create({active:true}); break;
        case "closeTab": await chrome.tabs.remove(tabId); break;
        case "scrollTop":
        case "scrollBottom":
          await chrome.tabs.sendMessage(
            tabId,
            {type:"scroll", action:msg.action},
            {frameId:sender.frameId ?? 0}
          ).catch(()=>{});
          break;
        case "duplicateTab": await chrome.tabs.duplicate(tabId); break;
        case "reopenClosedTab": await chrome.sessions.restore(); break;
        case "pinTab": await chrome.tabs.update(tabId,{pinned:true}); break;
        case "unpinTab": await chrome.tabs.update(tabId,{pinned:false}); break;
        case "muteTab": await chrome.tabs.update(tabId,{muted:true}); break;
        case "unmuteTab": await chrome.tabs.update(tabId,{muted:false}); break;
        case "zoomIn": {
          const z=await chrome.tabs.getZoom(tabId);
          await chrome.tabs.setZoom(tabId,Math.min(z+0.1,5));
          break;
        }
        case "zoomOut": {
          const z=await chrome.tabs.getZoom(tabId);
          await chrome.tabs.setZoom(tabId,Math.max(z-0.1,0.25));
          break;
        }
        case "zoomReset": await chrome.tabs.setZoom(tabId,0); break;
        case "openOptions": await chrome.runtime.openOptionsPage(); break;
        case "nextTab":
        case "previousTab":
        case "firstTab":
        case "lastTab": {
          const t=await chrome.tabs.get(tabId);
          const tabs=await chrome.tabs.query({windowId:t.windowId});
          if (!tabs.length) break;
          const i=tabs.findIndex(x=>x.id===tabId);
          let n=i;
          if(msg.action==="nextTab") n=(i+1)%tabs.length;
          if(msg.action==="previousTab") n=(i-1+tabs.length)%tabs.length;
          if(msg.action==="firstTab") n=0;
          if(msg.action==="lastTab") n=tabs.length-1;
          await chrome.tabs.update(tabs[n].id,{active:true});
          break;
        }
        case "newWindow": await chrome.windows.create({}); break;
        // Requires the user to enable "Allow in Incognito" for this
        // extension in chrome://extensions; otherwise this rejects and the
        // gesture silently does nothing (caught by the try/catch below).
        case "incognitoWindow": await chrome.windows.create({incognito:true}); break;
        case "closeWindow": await chrome.windows.remove(sender.tab.windowId); break;
        case "nextWindow":
        case "previousWindow": {
          const wins=await chrome.windows.getAll({windowTypes:["normal"]});
          if (wins.length<2) break;
          const wi=wins.findIndex(w=>w.id===sender.tab.windowId);
          if (wi===-1) break;
          const wn=msg.action==="nextWindow" ? (wi+1)%wins.length : (wi-1+wins.length)%wins.length;
          await chrome.windows.update(wins[wn].id,{focused:true});
          break;
        }
      }
    } catch (_) {}
  })();
});

chrome.runtime.onMessage.addListener((msg,sender,sendResponse)=>{
  if(!msg||msg.type!=="gestureActionExtended")return;
  chrome.tabs.query({active:true,currentWindow:true}).then(async tabs=>{
    const tab=tabs[0]; if(!tab){sendResponse({ok:false});return;}
    try{
      if(msg.action==="hardReload"){await chrome.tabs.reload(tab.id,{bypassCache:true});sendResponse({ok:true});return;}
      if(["scrollTop","scrollBottom","scrollUp","scrollDown","pageBack","pageForward"].includes(msg.action)){
        await chrome.scripting.executeScript({target:{tabId:tab.id},func:(a)=>{
          const h=Math.max(document.documentElement.scrollHeight,document.body?.scrollHeight||0);
          const amount=Math.max(innerHeight*.85,420);
          if(a==="scrollTop")scrollTo({top:0,behavior:"smooth"});
          else if(a==="scrollBottom")scrollTo({top:h,behavior:"smooth"});
          else if(a==="scrollUp")scrollBy({top:-Math.max(160,amount*.35),behavior:"smooth"});
          else if(a==="scrollDown")scrollBy({top:Math.max(160,amount*.35),behavior:"smooth"});
          else if(a==="pageBack")scrollBy({top:-amount,behavior:"smooth"});
          else if(a==="pageForward")scrollBy({top:amount,behavior:"smooth"});
        },args:[msg.action]});
        sendResponse({ok:true}); return;
      }
      sendResponse({ok:false});
    }catch(e){sendResponse({ok:false});}
  }).catch(()=>sendResponse({ok:false}));
  return true;
});
