'use strict';

const e = React.createElement;
var httpPrefix = "";//"http://localhost:81";

function wfetch(requestInfo, params) {
    return new Promise((resolve,reject) => {
      var anims = app.anims.filter(anim => anim.type == "post" && anim.from == "browser");
      var inSpot = getInSpot(anims, "browser");
      var reqAnim = inSpot;
  
      if (inSpot) {
        inSpot.weight++;
      } else {
        app.anims.push((reqAnim={
            type:"post",
            from: "browser",
            weight: 1,
            lineColor: '#00ffff',
            textColor: '#00ffff',
            shadowColor: '#000000',
            fillColor: '#004444',
            startY: 5,
            renderer: app.drawSprite
        }));
      }
  
      try{
        fetch(requestInfo,params).then(resp => {
          var anims = app.anims.filter(anim => anim.type == "post" && anim.from == "chip");
          var inSpot = getInSpot(anims, "chip");
    
          if (inSpot) {
            inSpot.weight++;
          } else {
            app.anims.push({
                type:"post",
                from: "chip",
                weight: 1,
                lineColor: '#00ffff',
                textColor: '#00ffff',
                shadowColor: '#000000',
                fillColor: '#004444',
                startY: 25,
                renderer: app.drawSprite
            });
          }
          resolve(resp);
        })
        .catch(err => {
          reqAnim.color="red";
          reqAnim.lineColor="red";
          reject(err);
        });
      } catch(e) {
        reqAnim.color="red";
        reqAnim.lineColor="red";
        reject(err);
      }
    })
  }
  