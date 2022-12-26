//#region utility functions
export function genUUID() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
      let r = Math.random() * 16 | 0,
          v = c === 'x' ? r : ((r & 0x3) | 0x8);
      return v.toString(16);
  });
}

export function dirname(path) {
  return (path.match(/.*\//) + "").replace(/(.+)\/$/g, "$1");
}

export function isFloat(n) {
  return Number(n) === n && n % 1 !== 0;
}

export function IsDatetimeValue(fld) {
  return fld.match(".*ime_.s$") || fld.match(".*ime_sec$");
}

export function IsBooleanValue(val) {
  return (val === "true") ||
      (val === "yes") ||
      (val === true) ||
      (val === "false") ||
      (val === "no") ||
      (val === false);
}

export function IsNumberValue(val) {
  return ((val !== true) && (val !== false) && (typeof (val) != "string") && !isNaN(val) && (val !== ""));
}

export function degToRad(degree) {
  let factor = Math.PI / 180;
  return degree * factor;
}

export function fromVersionedToPlain(obj, level = "") {
  let ret = {};
  let arr = Object.keys(obj);
  let fldidx;
  for (fldidx in arr) {
      let fld = arr[fldidx];
      if ((typeof obj[fld] === 'object') &&
          (Object.keys(obj[fld]).filter(cfld =>  cfld !== "version" && cfld !== "value").length === 0)) {
          ret[fld] = obj[fld]["value"];
      } else if (Array.isArray(obj[fld])) {
          ret[fld] = [];
          obj[fld].forEach((item, idx) => {
              if (obj[fld][idx].boper) {
                  ret[fld][ret[fld].length - 1]["boper"] = obj[fld][idx].boper;
              } else {
                  ret[fld].push(fromVersionedToPlain(item, `${level}/${fld}[${idx}]`));
              }
          });
      } else if (typeof obj[fld] === 'object') {
          ret[fld] = fromVersionedToPlain(obj[fld], `${level}/${fld}`);
      } else {
          ret[fld] = obj[fld];
      }
  }
  return ret;
}

export function fromPlainToVersionned(obj, vobj, level = "") {
  return Object.entries(obj).reduce((ret,entry) => {
    let fld = entry[0];
    let pvobj = vobj ? vobj[fld] : undefined;
    if (Array.isArray(obj[fld])) {
      ret[fld]=obj[fld].map((child,idx) => fromPlainToVersionned(child, pvobj ? pvobj[idx] : null, `${level}/${fld}[${idx}]`));
    } else if (obj[fld] && (obj[fld].constructor === Object)) {
      ret[fld]=fromPlainToVersionned(obj[fld], pvobj, `${level}/${fld}`);
    } else {
      ret[fld]= {
        version: pvobj === undefined ? 0 : obj[fld] === pvobj.value ? pvobj.version : pvobj.version + 1,
        value: obj[fld]
      }
    }
    return ret;
  },{});
}

const getCellValue = (tr, idx) => tr.children[idx]?.innerText || tr.children[idx]?.textContent;

export function comparer (idx, asc) {
  return  (a, b) => ((v1, v2) =>
    v1 !== '' && v2 !== '' && !isNaN(v1) && !isNaN(v2) ? v1 - v2 : v1?.toString()?.localeCompare(v2)
  )(getCellValue(asc ? a : b, idx), getCellValue(asc ? b : a, idx));
}

export function getInSpot(anims, origin) {
  return anims
        .filter(anim => anim.from === origin)
        .find(anim => (anim.x === undefined) || (origin === "browser" ? anim.x <= (36 + anim.weight) : anim.x >= (66 - anim.weight)));
}

export function getAnims() {
  if (!window.anims){
    window.anims=[];
  }
  return window.anims
}

export function isStandalone() {
  return window.location.protocol !== "https:" && window.location.port < 10000;
}

let selectedDevice = undefined;

export function setSelectedDevice(device) {
  selectedDevice = device;
}

export function chipRequest(requestInfo, params) {
    return new Promise((resolve,reject) => {
      let browserPostAnims = getAnims().filter(anim => anim.type === "post" && anim.from === "browser");
      let inSpot = getInSpot(browserPostAnims, "browser");
      let reqAnim = inSpot;
  
      if (inSpot) {
        inSpot.weight++;
      } else {
        reqAnim={
          type:"post",
          from: "browser",
          weight: 1,
          lineColor: '#00ffff',
          textColor: '#00ffff',
          shadowColor: '#000000',
          fillColor: '#004444',
          startY: 5
        };
        getAnims().push(reqAnim);
      }
  
      let httpPrefix = selectedDevice?.ip ? `${process.env.REACT_APP_API_URI}${selectedDevice.config.devName}` : "";
      fetch(`${params?.skipHttpPrefix ? '' : httpPrefix}${requestInfo}`,params).then(resp => {
        let chipResponseAnim = getAnims().filter(anim => anim.type === "post" && anim.from === "chip");
        let inSpot = getInSpot(chipResponseAnim, "chip");
  
        if (inSpot) {
          inSpot.weight++;
        } else {
          getAnims().push({
              type:"post",
              from: "chip",
              weight: 1,
              lineColor: '#00ffff',
              textColor: '#00ffff',
              shadowColor: '#000000',
              fillColor: '#004444',
              startY: 25
          });
        }
        resolve(resp);
      })
      .catch(err => {
        reqAnim.color="red";
        reqAnim.lineColor="red";
        reject(err);
      });

      if (window.animRenderer) {
        window.requestAnimationFrame(window.animRenderer);
      }
    })
  }
  