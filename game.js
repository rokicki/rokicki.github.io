m=528349151;n=1000;a=b=0;l="";
function c(n){return(n^(n<<1)^(n<<6)^(n>>1)^(n>>6))&m;}
function d(i){n++;a^=i;b^=c(i);}
function j(i){var r=0;while(i){r++;i&=i-1;}if(r<n)n=r;}
function f(i){j(i);j(i^240497998);}
function w(){b=s=c(m&(m*Math.random()));d(b>>6);d(b>>6);d(b>>6);d(b>>6);
if(b&4)d(103125900);if(b&2)d(120176668);if(b&1)d(51434840);n=1000;f(a);
f(a^357827925);q=n;n=0;b=s;l="Clear the board in "+q+" moves.";}
function r(){var i;for(i=0;i<25;i++){document.images[1+i].src=(b&(1<<(i+i/5)))?
"black.gif":"white.gif";document.images[1+i].onmousedown=new Function("z("+i+")"
);}document.forms[0].elements[0].value=l;}
function z(i){if(b==0){w();}else{d(1<<(i+i/5));if(b==0){l="You did it in "+n
+"/"+q+" moves.";(new Image).src="white.gif?"+s+","+n+","+q}else{l=""+n+"/"+q;
}}r();}w();
