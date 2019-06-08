var xoff = 0 ;
var yoff = 0 ;
var nameof = new Array() ;
var botid = new Array() ;
var botstyle = new Array() ;
var namediv = new Array() ;
var scorediv = new Array() ;
var moneydiv = new Array() ;
var packdiv = new Array() ;
var movediv = new Array() ;
var idof = new Array() ;
var nbots = 0 ;
var mult = 0 ;
var time = 0 ;
var waitTime = 15 ;
var height = 0 ;
var width = 0 ;
var textob = 0 ;
var stopped = 1 ;
var fontheight = 20 ;
var fontwidth = 12 ;
var boardname = "" ;
function getStyleObject(objectId) {
   if(document.getElementById && document.getElementById(objectId)) {
      return document.getElementById(objectId).style;
   } else if (document.all && document.all(objectId)) {
      return document.all(objectId).style;
   } else if (document.layers && document.layers[objectId]) {
      return document.layers[objectId];
   } else {
      return false;
   }
}
function init(mlt, h, w, bn) {
   var i ;
   mult = mlt ;
   height = h ;
   width = w ;
   boardname = bn ;
   for (i=0; bots[i] != ""; i++) {
      var j = bots[i].indexOf(" ") ;
      var id = bots[i].substring(0, j) ;
      idof[i] = id ;
      nameof[id] = bots[i].substring(j+1) ;
      botid[nbots] = id ;
      botstyle[id] = getStyleObject(id) ;
      namediv[id] = document.getElementById(id + "name") ;
      scorediv[id] = document.getElementById(id + "score") ;
      moneydiv[id] = document.getElementById(id + "money") ;
      movediv[id] = document.getElementById(id + "move") ;
      packdiv[id] = document.getElementById(id + "pack") ;
      namediv[id].innerHTML = bots[i] ;
      nbots++ ;
   }
   xoff = parseInt(getStyleObject("board").left) - mult ;
   yoff = parseInt(getStyleObject("board").top) - mult ;
   xoff += Math.floor((mult - fontwidth + 1) / 2) ;
   yoff += Math.floor((mult - fontheight + 1) / 2) ;
   if (document.layers) document.captureEvents(Event.KEYPRESS);
   document.onkeypress=keyhandler;
}
function dostep() {
   var i ;
   var s ;
   if (data[time] == '') {
      time-- ;
      stopped = 1 ;
      return ;
   }
   var botlines = data[time].split(";") ;
   var ii ;
   for (ii=0; ii<botlines.length; ii++) {
      var lin = botlines[ii] ;
      var id = idof[ii] ;
      if (lin == '') {
         lin = lastline[ii] ;
      }
      var f = lin.split(" ") ;
      var x = mult * f[0] ;
      var y = mult * (height - f[1] + 1) ;
      if (x > 0 && y > 0) {
         botstyle[id].left = (xoff + x) + "px" ;
         botstyle[id].top = (yoff + y) + "px" ;
      } else {
         botstyle[id].top = botstyle[id].left = 0 ;
      }
      scorediv[id].innerHTML = f[2] ;
      moneydiv[id].innerHTML = f[3] ;
      packdiv[id].innerHTML = f[4] ;
      s = f[5] ;
      for (i=6; f.length>i; i++)
         s = s + " " + f[i] ;
      movediv[id].innerHTML = s ;
   }
   document.f.step.value = time ;
   if (!stopped)
      window.setTimeout("time++;dostep();", waitTime);
}
function findtime(t) {
   if (t < 0) t = 0 ;
   else if (t >= data.length || data[t] == '') t = data.length-2 ;
   time = t ;
}
function relative(inc) {
   findtime(time + inc) ;
   stopped = 1 ;
   dostep() ;
}
function stopit() {
   stopped = 1 ;
}
function absolute() {
   findtime(parseInt(document.f.step.value)) ;
   stopped = 1 ;
   dostep() ;
}
function startit() {
   stopped = 0 ;
   dostep() ;
}
function go(mlt, h, w, bn) {
   init(mlt, h, w, bn) ;
   newsize() ;
}
function faster() {
   waitTime = (waitTime >> 1) | 1 ;
}
function slower() {
   waitTime = waitTime + waitTime + 1 ;
}
function newsize() {
   document.getElementById("board").innerHTML = 
      '<img name="myimg" src="' + boardname + '.gif" width="' + (mult * width) +
      '" height="' + (mult * height) + '">' ;
   xoff = parseInt(getStyleObject("board").left) - mult ;
   yoff = parseInt(getStyleObject("board").top) - mult ;
   xoff += Math.floor((mult - fontwidth + 1) / 2) ;
   yoff += Math.floor((mult - fontheight + 1) / 2) ;
   getStyleObject("table").top =
                        (yoff + (height + 1) * mult + 10 + fontheight) + "px" ;
   relative(0) ;
}
function larger() {
   mult++ ;
   newsize() ;
}
function smaller() {
   if (1 >= mult) return ;
   mult-- ;
   newsize() ;
}
function keyhandler(e) {
   var key ;
   if (document.all) {
      e = window.event ;
      key = e.keyCode ;
   } else {
      key = e.which ;
   }
   key &= 127 ;
   if (key == 32 || key == 110 || key == 78) {
      relative(1) ;
   } else if (key == 80 || key == 112) {
      relative(-1) ;
   } else if (key == 43 || key == 61) {
      larger() ;
   } else if (key == 45) {
      smaller() ;
   } else {
      return true ;
   }
   if (document.layers)
      return false ;
   else if (document.all)
      e.returnValue = false ;
}
function writebody() {
   var i, id ;
   document.write(
       '<div id="board" style="position:absolute;top:10;left:10;"></div>\n') ;
   for (i=0; bots[i] != ''; i++) {
      var f = bots[i].split(" ") ;
      id = f[0] ;
      document.write('<div id="' + id +
         '" style="position:absolute;top:10;left:10;font-size:' + fontheight +
         'px;">' + id + '</div>\n') ;
   }
   document.write(
  '<div id="table" style="position:absolute;top:10;left:10;">\n' +
  '<form name="f" action="" onSubmit="return false;">\n' +
  '<input type="button" value="<<" onClick="relative(-100000000);">\n' +
  '<input type="button" value="<" onClick="relative(-1);">\n' +
  '<input type="button" value="stop" onClick="stopit();">\n' +
  '<input type="button" value="play" onClick="startit();">\n' +
  '<input type="button" value=">" onClick="relative(1);">\n' +
  '<input type="button" value=">>" onClick="relative(1000000000);">\n' +
  'Step: <input name="step" type="text" value="0" size="6">\n' +
  '<input type="button" value="go" onClick="absolute();">\n' +
  '<input type="button" value="faster" onClick="faster();">\n' +
  '<input type="button" value="slower" onClick="slower();">\n' +
  '<input type="button" value="smaller" onClick="smaller();">\n' + 
  '<input type="button" value="larger" onClick="larger();"><br>\n' +
  'Keys:  space/n: forward; p: backward; +/=: larger;  -: smaller <br>\n' +
  '</form>\n' +
  '<table><tr><td>Robot</td><td>Money</td><td>Score</td><td>#Packages</td>\n' +
  '<td>Bid/Move</td></tr>\n') ;
   for (i=0; bots[i] != ''; i++) {
      var f = bots[i].split(" ") ;
      id = f[0] ;
      document.write('<tr>' +
         '<td><div id="' + id + 'name"></div></td>' +
         '<td align=right><div id="' + id + 'money"></div></td>' +
         '<td align=right><div id="' + id + 'score"></div></td>' +
         '<td align=right><div id="' + id + 'pack"></div></td>' +
         '<td><div id="' + id + 'move"></div></td>' +
         '</tr>\n') ;
   }
   document.write('</table></div>\n') ;
}
