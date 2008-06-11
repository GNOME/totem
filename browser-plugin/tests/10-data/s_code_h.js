/* SiteCatalyst code version: H.8. Copyright 1997-2007 Omniture, Inc. More info available at http://www.omniture.com */
// 2007-01-26: Added Relative Link Patch
// 2007-01-30: Fixed Relative Link Patch
// 2007-07-24: ClickMap Image Map Patch
/************************ ADDITIONAL FEATURES ************************
     Dynamic Report Suite Selection
*/
/* Specify the Report Suite ID(s) to track here */
//var s_account="appleglobal"
var s=s_gi(s_account)
/************************** CONFIG SECTION **************************/
/* You may add or alter any code config here. */

/* E-commerce Config */
s.currencyCode="USD"
/* Link Tracking Config */
s.trackDownloadLinks=true
s.trackExternalLinks=true
s.trackInlineStats=true
s.linkDownloadFileTypes="zip,wav,mp3,mp4,doc,pdf,xls,rss,dmg,sit,pkg,exe,xml,mov"
s.linkInternalFilters="javascript:,apple.com"
s.linkLeaveQueryString=false
s.linkTrackVars="campaign"
s.linkTrackEvents="None"


function getZipCode() {
	var cookies = document.cookie.split(/;/g);
	
	for (var i = cookies.length - 1; i >= 0; i--){
		var cookie = cookies[i];
		zipCode = cookie.match(/PostalCode=(\d{5})/i);
		if(zipCode) {
			zipCode = zipCode[1];
			break;
		}
	}
	
	if(!zipCode) {
		zipCode = "No Zip";
	}
	
	return zipCode;
}

function QTCheck() {
	if (typeof(AC) != "undefined" && typeof(AC.Detector) != "undefined" && typeof(AC.Detector.getQTVersion) != "undefined") {
		
		if (AC.Detector.isMobile()) {
			return "Mobile"
		}
		
		var version = AC.Detector.getQTVersion();
		if ("0" == version) {
			return "No QuickTime"
		} else {
			return version;
		}
	} else {
		return "Not detected"
	}
}

var isIE  = (navigator.appVersion.indexOf("MSIE") != -1) ? true : false;
var isWin = (navigator.appVersion.toLowerCase().indexOf("win") != -1) ? true : false;
var isOpera = (navigator.userAgent.indexOf("Opera") != -1) ? true : false;


// Flash Player Version Detection - Rev 1.6
// Detect Client Browser type
// Copyright(c) 2005-2006 Adobe Macromedia Software, LLC. All rights reserved.
function ControlVersion()
{
	var version;
	var axo;
	var e;


	try {
		// version will be set for 7.X or greater players
		axo = new ActiveXObject("ShockwaveFlash.ShockwaveFlash.7");
		version = axo.GetVariable("$version");
	} catch (e) {
	}

	if (!version)
	{
		try {
			// version will be set for 6.X players only
			axo = new ActiveXObject("ShockwaveFlash.ShockwaveFlash.6");
			
			// installed player is some revision of 6.0
			// GetVariable("$version") crashes for versions 6.0.22 through 6.0.29,
			// so we have to be careful. 
			
			// default to the first public version
			version = "WIN 6,0,21,0";

			// throws if AllowScripAccess does not exist (introduced in 6.0r47)		
			axo.AllowScriptAccess = "always";

			// safe to call for 6.0r47 or greater
			version = axo.GetVariable("$version");

		} catch (e) {
		}
	}

	if (!version)
	{
		try {
			// version will be set for 4.X or 5.X player
			axo = new ActiveXObject("ShockwaveFlash.ShockwaveFlash.3");
			version = axo.GetVariable("$version");
		} catch (e) {
		}
	}

	if (!version)
	{
		try {
			// version will be set for 3.X player
			axo = new ActiveXObject("ShockwaveFlash.ShockwaveFlash.3");
			version = "WIN 3,0,18,0";
		} catch (e) {
		}
	}

	if (!version)
	{
		try {
			// version will be set for 2.X player
			axo = new ActiveXObject("ShockwaveFlash.ShockwaveFlash");
			version = "WIN 2,0,0,11";
		} catch (e) {
			version = "No Flash";
		}
	}
	
	return version;
}

// JavaScript helper required to detect Flash Player PlugIn version information
function GetSwfVer(){
	// NS/Opera version >= 3 check for Flash plugin in plugin array
	var flashVer = 'No Flash';
	
	if (navigator.plugins != null && navigator.plugins.length > 0) {
		if (navigator.plugins["Shockwave Flash 2.0"] || navigator.plugins["Shockwave Flash"]) {
			var swVer2 = navigator.plugins["Shockwave Flash 2.0"] ? " 2.0" : "";
			var flashDescription = navigator.plugins["Shockwave Flash" + swVer2].description;
			var descArray = flashDescription.split(" ");
			var tempArrayMajor = descArray[2].split(".");
			var versionMajor = tempArrayMajor[0];
			var versionMinor = tempArrayMajor[1];
			var versionRevision = descArray[3];
			if (versionRevision == "") {
				versionRevision = descArray[4];
			}
			if (versionRevision[0] == "d") {
				versionRevision = versionRevision.substring(1);
			} else if (versionRevision[0] == "r") {
				versionRevision = versionRevision.substring(1);
				if (versionRevision.indexOf("d") > 0) {
					versionRevision = versionRevision.substring(0, versionRevision.indexOf("d"));
				}
			}
			var flashVer = versionMajor + "." + versionMinor + "." + versionRevision;
		}
	}
	
	else if ( isIE && isWin && !isOpera ) {
		flashVer = ControlVersion();
	}
	
	return flashVer;
}

function FormatFlashVersion(reported) {
	
	var matched = reported.match(/^(?:win[\D0]*)?(\d)/i);
	if (matched && matched[1]) {
		reported = matched[1];
	}
	
	return reported;
}

/* Plugin Config */
s.usePlugins=true
function s_doPlugins(s) {
	/* Add calls to plugins here */
	
	// Processor Platform: 2007-06-08
	s.prop5=navigator.platform;
	
	// Redirect Referrer
	tempVar1=s.getQueryParam('ref');
	if(tempVar1)s.referrer=tempVar1;
	
	// Redirect Alias
	tempVar2=s.getQueryParam('alias');
	if(tempVar2)s.server=tempVar2;
	
	// External Campaigns
	if(!s.campaign){
		s.campaign=s.getQueryParam('cid');
		s.campaign=s.getValOnce(s.campaign,"s_campaign",0)
		
	// Email ClickMap
		s.setClickMapEmail('Email_PageName,Email_OID','Email_OT');
	}
	
	// Internal Campaigns - Session
	s.eVar7=s.getQueryParam('aid');
	s.eVar7=s.getValOnce(s.eVar7,'s_var_7',0)
	
	// Internal Campaigns - 30 Days
	s.eVar8=s.getValOnce(s.eVar8,'s_var_8',30)
	
	// Campaign Pathing
	s.prop6=s.getQueryParam('cp')+": "+s.pageName;
	
	// Campaign Channel
	s.prop11=s.getQueryParam('sr');
	
	// Zip code
	s.prop15=getZipCode();
	
	/* getNewRepeat 1.0 */
	s.prop16=s.getNewRepeat();
	
	// Mac OS X Versions: 2007-06-08 : Start
	// Added Leopard: 2007-11-06
	if (typeof(AC) == "undefined") {AC = {};}
	
		AC.AosCheck = function() {
			if (typeof(s) != "undefined") {
				
				if (navigator.userAgent.match(/windows/i)) {
					s.prop9 = "windows"
					return;
				}
				
				var mv = AC.getWebkitVersion().major;
				
				if (mv > 522) s.prop9 = "10.5.x";
				else if (mv > 400) s.prop9 = "10.4.x";
				else if (mv > 99) s.prop9 = "10.3.x";
				else if (mv > 80) s.prop9 = "10.2.x";
				else s.prop9 = "mac unknown or non-safari";
			}
		}
		
		AC.parseWebkitVersion = function(version) {
		 var bits = version.split(".");
		 var is_nightly = (version[version.length - 1] == "+");
		 if (is_nightly) {
			var minor = "+";
		 } else {
			var minor = parseInt(bits[1]);
			// If minor is Not a Number (NaN) return an empty string
			if (isNaN(minor)) {
			 minor = "";
			}
		 }
		 return {major: parseInt(bits[0]), minor: minor, is_nightly: is_nightly};
		}
		
		AC.getWebkitVersion = function() {
		 var regex = new RegExp("\\(.*\\) AppleWebKit/(.*) \\((.*)");
		 var matches = regex.exec(navigator.userAgent);
		 if (matches) {
			var webkit_version = AC.parseWebkitVersion(matches[1]);
		 }
		 if (webkit_version)
				return {major: webkit_version['major'], minor: webkit_version['minor'], is_nightly: webkit_version['is_nightly']};
			else // can't resolve version, we're not webkit
				return {major: 0, minor: 'not webkit', is_nightly: 0};
		}
		AC.AosCheck();
	// Mac OS X Versions: 2007-06-08 : En




	/* Plugin Example: getQueryParam v2.0
	s.campaign=s.getQueryParam('cid')
	*/

	/* Plugin Example: getValOnce v0.2
	s.campaign=s.getValOnce(s.campaign,"s_campaign",0)
	*/
	
	/* Plugin Example: 
		s.setClickMapEmail('Email_PageName,Email_OID,Email_OT');
	*/

	/* Plugin Example: getAndPersistValue 0.3 */
	/* param1 - variable to persist */
	/* param2 - cookie name */
	/* param1 - Number of days to expiration - 0 for session */
	/* returns value of persisted variable */
	/* s.prop1=s.getAndPersistValue(s.campaign,'s_cp_pers',0); */

	/* Plugin Example: getDaysSinceLastVisit 1.0 */
	/* s.prop1=s.getDaysSinceLastVisit(); */
	
	/* Plugin Example: getTimeToComplete 0.4 */
	/* if(s.events.indexOf('event1')>-1)
	/* 	s.prop1='start';
	/* if(s.events.indexOf('event2')>-1)
	/* 	s.prop1='stop';
	/* s.prop1=s.getTimeToComplete(s.prop1,'ttc',0); */

	/* Plugin Example: getNewRepeat 1.0 */
	/* s.prop5=s.getNewRepeat(); */
	
	/* Plugin Example: getVisitStart */
	/* param1 - cookie name */
	/* returns 1 on first page of visit, otherwise 0  */
	/* s.prop50=s.getVisitStart("s_visit"); */
	
	/* Plugin Example: formAnalysis 2.0	*/
	/* s.setupFormAnalysis();		*/

	/* Plugin Examples: getPreviousValue 
	// get previous value for s.pageName variable, set to prop5 on every page	
	/*	s.prop5=s.getPreviousValue(s.pageName,'gpv_p5','');
	// get previous value for s.prop1, set to prop6 only when event1 is in the events variable
	/*	s.prop6=s.getPreviousValue(s.prop1,'gpv_p6','event1');
	// get previous value for s.prop1, set to prop6 when either event1 or event10 is in the events variable
	/*	s.prop7=s.getPreviousValue(s.channel,'gpv_p4','event1,event10');*/

}
s.doPlugins=s_doPlugins

/************************** PLUGINS SECTION *************************/
/*
 * Plugin: getQueryParam 2.0 - return query string parameter(s)
 */
s.getQueryParam=new Function("p","d","u",""
+"var s=this,v='',i,t;d=d?d:'';u=u?u:(s.pageURL?s.pageURL:''+s.wd.loc"
+"ation);u=u=='f'?''+s.gtfs().location:u;while(p){i=p.indexOf(',');i="
+"i<0?p.length:i;t=s.p_gpv(p.substring(0,i),u);if(t)v+=v?d+t:t;p=p.su"
+"bstring(i==p.length?i:i+1)}return v");
s.p_gpv=new Function("k","u",""
+"var s=this,v='',i=u.indexOf('?'),q;if(k&&i>-1){q=u.substring(i+1);v"
+"=s.pt(q,'&','p_gvf',k)}return v");
s.p_gvf=new Function("t","k",""
+"if(t){var s=this,i=t.indexOf('='),p=i<0?t:t.substring(0,i),v=i<0?'T"
+"rue':t.substring(i+1);if(p.toLowerCase()==k.toLowerCase())return s."
+"epa(v)}return ''");
/*
 * Plugin: getValOnce 0.2 - get a value once per session or number of days
 */
s.getValOnce=new Function("v","c","e",""
+"var s=this,k=s.c_r(c),a=new Date;e=e?e:0;if(v){a.setTime(a.getTime("
+")+e*86400000);s.c_w(c,v,e?a:0);}return v==k?'':v");
/*
 * Plugin: setClickMapEmail v1.3 - sets ClickMap variables w/ q-string values
 */
s.setClickMapEmail=new Function("qp","ot",""
+"var s=this,v=s.getQueryParam(qp,'~'),d,pn,oid,ot=s.getQueryParam(ot)"
+",ot=ot?ot:'A',cv;d=v.indexOf('~');if(!v)return '';if(d>-1){pn=v.subs"
+"tring(0,d);oid=v.substring(d+1);}cv='&pid='+s.ape(s.fl(pn,255))+'&pi"
+"dt=1&oid='+s.ape(s.fl(oid,100))+'&oidt=1&ot='+ot+'&oi=1';s.sq(cv);");
/*
 * Plugin: Form Analysis 2.0 (Success, Error, Abandonment)
 */
s.setupFormAnalysis=new Function(""
+"var s=this;if(!s.fa){s.fa=new Object;var f=s.fa;f.ol=s.wd.onload;s."
+"wd.onload=s.faol;f.uc=s.useCommerce;f.vu=s.varUsed;f.vl=f.uc?s.even"
+"tList:'';f.tfl=s.trackFormList;f.fl=s.formList;f.va=new Array('',''"
+",'','')}");
s.sendFormEvent=new Function("t","pn","fn","en",""
+"var s=this,f=s.fa;t=t=='s'?t:'e';f.va[0]=pn;f.va[1]=fn;f.va[3]=t=='"
+"s'?'Success':en;s.fasl(t);f.va[1]='';f.va[3]='';");
s.faol=new Function("e",""
+"var s=s_c_il["+s._in+"],f=s.fa,r=true,fo,fn,i,en,t,tf;if(!e)e=s.wd."
+"event;f.os=new Array;if(f.ol)r=f.ol(e);if(s.d.forms&&s.d.forms.leng"
+"th>0){for(i=s.d.forms.length-1;i>=0;i--){fo=s.d.forms[i];fn=fo.name"
+";tf=f.tfl&&s.pt(f.fl,',','ee',fn)||!f.tfl&&!s.pt(f.fl,',','ee',fn);"
+"if(tf){f.os[fn]=fo.onsubmit;fo.onsubmit=s.faos;f.va[1]=fn;f.va[3]='"
+"No Data Entered';for(en=0;en<fo.elements.length;en++){el=fo.element"
+"s[en];t=el.type;if(t&&t.toUpperCase){t=t.toUpperCase();var md=el.on"
+"mousedown,kd=el.onkeydown,omd=md?md.toString():'',okd=kd?kd.toStrin"
+"g():'';if(omd.indexOf('.fam(')<0&&okd.indexOf('.fam(')<0){el.s_famd"
+"=md;el.s_fakd=kd;el.onmousedown=s.fam;el.onkeydown=s.fam}}}}}f.ul=s"
+".wd.onunload;s.wd.onunload=s.fasl;}return r;");
s.faos=new Function("e",""
+"var s=s_c_il["+s._in+"],f=s.fa,su;if(!e)e=s.wd.event;if(f.vu){s[f.v"
+"u]='';f.va[1]='';f.va[3]='';}su=f.os[this.name];return su?su(e):tru"
+"e;");
s.fasl=new Function("e",""
+"var s=s_c_il["+s._in+"],f=s.fa,a=f.va,l=s.wd.location,ip=s.trackPag"
+"eName,p=s.pageName;if(a[1]!=''&&a[3]!=''){a[0]=!p&&ip?l.host+l.path"
+"name:a[0]?a[0]:p;if(!f.uc&&a[3]!='No Data Entered'){if(e=='e')a[2]="
+"'Error';else if(e=='s')a[2]='Success';else a[2]='Abandon'}else a[2]"
+"='';var tp=ip?a[0]+':':'',t3=e!='s'?':('+a[3]+')':'',ym=!f.uc&&a[3]"
+"!='No Data Entered'?tp+a[1]+':'+a[2]+t3:tp+a[1]+t3,ltv=s.linkTrackV"
+"ars,lte=s.linkTrackEvents,up=s.usePlugins;if(f.uc){s.linkTrackVars="
+"ltv=='None'?f.vu+',events':ltv+',events,'+f.vu;s.linkTrackEvents=lt"
+"e=='None'?f.vl:lte+','+f.vl;f.cnt=-1;if(e=='e')s.events=s.pt(f.vl,'"
+",','fage',2);else if(e=='s')s.events=s.pt(f.vl,',','fage',1);else s"
+".events=s.pt(f.vl,',','fage',0)}else{s.linkTrackVars=ltv=='None'?f."
+"vu:ltv+','+f.vu}s[f.vu]=ym;s.usePlugins=false;s.tl(true,'o','Form A"
+"nalysis');s[f.vu]='';s.usePlugins=up}return f.ul&&e!='e'&&e!='s'?f."
+"ul(e):true;");
s.fam=new Function("e",""
+"var s=s_c_il["+s._in+"],f=s.fa;if(!e) e=s.wd.event;var o=s.trackLas"
+"tChanged,et=e.type.toUpperCase(),t=this.type.toUpperCase(),fn=this."
+"form.name,en=this.name,sc=false;if(document.layers){kp=e.which;b=e."
+"which}else{kp=e.keyCode;b=e.button}et=et=='MOUSEDOWN'?1:et=='KEYDOW"
+"N'?2:et;if(f.ce!=en||f.cf!=fn){if(et==1&&b!=2&&'BUTTONSUBMITRESETIM"
+"AGERADIOCHECKBOXSELECT-ONEFILE'.indexOf(t)>-1){f.va[1]=fn;f.va[3]=e"
+"n;sc=true}else if(et==1&&b==2&&'TEXTAREAPASSWORDFILE'.indexOf(t)>-1"
+"){f.va[1]=fn;f.va[3]=en;sc=true}else if(et==2&&kp!=9&&kp!=13){f.va["
+"1]=fn;f.va[3]=en;sc=true}if(sc){nface=en;nfacf=fn}}if(et==1&&this.s"
+"_famd)return this.s_famd(e);if(et==2&&this.s_fakd)return this.s_fak"
+"d(e);");
s.ee=new Function("e","n",""
+"return n&&n.toLowerCase?e.toLowerCase()==n.toLowerCase():false;");
s.fage=new Function("e","a",""
+"var s=this,f=s.fa,x=f.cnt;x=x?x+1:1;f.cnt=x;return x==a?e:'';");
/*
 * Plugin: getAndPersistValue 0.3 - get a value on every page
 */
s.getAndPersistValue=new Function("v","c","e",""
+"var s=this,a=new Date;e=e?e:0;a.setTime(a.getTime()+e*86400000);if("
+"v)s.c_w(c,v,e?a:0);return s.c_r(c);");
/*
 * Plugin: Days since last Visit 1.0.H - capture time from last visit
 */
s.getDaysSinceLastVisit=new Function(""
+"var s=this,e=new Date(),cval,ct=e.getTime(),c='s_lastvisit',day=24*"
+"60*60*1000;e.setTime(ct+3*365*day);cval=s.c_r(c);if(!cval){s.c_w(c,"
+"ct,e);return 'First page view or cookies not supported';}else{var d"
+"=ct-cval;if(d>30*60*1000){if(d>30*day){s.c_w(c,ct,e);return 'More t"
+"han 30 days';}if(d<30*day+1 && d>7*day){s.c_w(c,ct,e);return 'More "
+"than 7 days';}if(d<7*day+1 && d>day){s.c_w(c,ct,e);return 'Less tha"
+"n 7 days';}if(d<day+1){s.c_w(c,ct,e);return 'Less than 1 day';}}els"
+"e return '';}");
/*
 * Plugin: getTimeToComplete 0.4 - return the time from start to stop
 */
s.getTimeToComplete=new Function("v","cn","e",""
+"var s=this,d=new Date,x=d,k;if(!s.ttcr){e=e?e:0;if(v=='start'||v=='"
+"stop')s.ttcr=1;x.setTime(x.getTime()+e*86400000);if(v=='start'){s.c"
+"_w(cn,d.getTime(),e?x:0);return '';}if(v=='stop'){k=s.c_r(cn);if(!s"
+".c_w(cn,'',d)||!k)return '';v=(d.getTime()-k)/1000;var td=86400,th="
+"3600,tm=60,r=5,u,un;if(v>td){u=td;un='days';}else if(v>th){u=th;un="
+"'hours';}else if(v>tm){r=2;u=tm;un='minutes';}else{r=.2;u=1;un='sec"
+"onds';}v=v*r/u;return (Math.round(v)/r)+' '+un;}}return '';");
/*
 * Plugin: getNewRepeat 1.0 - Return whether user is new or repeat
 */
s.getNewRepeat=new Function(""
+"var s=this,e=new Date(),cval,ct=e.getTime(),y=e.getYear();e.setTime"
+"(ct+30*24*60*60*1000);cval=s.c_r('s_nr');if(cval.length==0){s.c_w("
+"'s_nr',ct,e);return 'New';}if(cval.length!=0&&ct-cval<30*60*1000){s"
+".c_w('s_nr',ct,e);return 'New';}if(cval<1123916400001){e.setTime(cv"
+"al+30*24*60*60*1000);s.c_w('s_nr',ct,e);return 'Repeat';}else retur"
+"n 'Repeat';");
/*
 * Plugin: getVisitStart v2.0
 */
s.getVisitStart=new Function("c",""
+"var s=this,v=1,t=new Date;t.setTime(t.getTime()+1800000);if(s.c_r(c"
+")){v=0}if(!s.c_w(c,1,t)){s.c_w(c,1,0)}if(!s.c_r(c)){v=0}return v;"); 
/*
 * Plugin: getPreviousValue_v1.0 - return previous value of designated
 *   variable (requires split utility)
 */
s.getPreviousValue=new Function("v","c","el",""
+"var s=this,t=new Date,i,j,r='';t.setTime(t.getTime()+1800000);if(el"
+"){if(s.events){i=s.split(el,',');j=s.split(s.events,',');for(x in i"
+"){for(y in j){if(i[x]==j[y]){if(s.c_r(c)) r=s.c_r(c);v?s.c_w(c,v,t)"
+":s.c_w(c,'no value',t);return r}}}}}else{if(s.c_r(c)) r=s.c_r(c);v?"
+"s.c_w(c,v,t):s.c_w(c,'no value',t);return r}");
/*
 * Utility Function: split v1.5 - split a string (JS 1.0 compatible)
 */
s.split=new Function("l","d",""
+"var i,x=0,a=new Array;while(l){i=l.indexOf(d);i=i>-1?i:l.length;a[x"
+"++]=l.substring(0,i);l=l.substring(i+d.length);}return a");
/************* DO NOT ALTER ANYTHING BELOW THIS LINE ! **************/
var s_objectID;function s_c2fe(f){var x='',s=0,e,a,b,c;while(1){e=
f.indexOf('"',s);b=f.indexOf('\\',s);c=f.indexOf("\n",s);if(e<0||(b>=
0&&b<e))e=b;if(e<0||(c>=0&&c<e))e=c;if(e>=0){x+=(e>s?f.substring(s,e):
'')+(e==c?'\\n':'\\'+f.substring(e,e+1));s=e+1}else return x
+f.substring(s)}return f}function s_c2fa(f){var s=f.indexOf('(')+1,e=
f.indexOf(')'),a='',c;while(s>=0&&s<e){c=f.substring(s,s+1);if(c==',')
a+='","';else if(("\n\r\t ").indexOf(c)<0)a+=c;s++}return a?'"'+a+'"':
a}function s_c2f(cc){cc=''+cc;var fc='var f=new Function(',s=
cc.indexOf(';',cc.indexOf('{')),e=cc.lastIndexOf('}'),o,a,d,q,c,f,h,x
fc+=s_c2fa(cc)+',"var s=new Object;';c=cc.substring(s+1,e);s=
c.indexOf('function');while(s>=0){d=1;q='';x=0;f=c.substring(s);a=
s_c2fa(f);e=o=c.indexOf('{',s);e++;while(d>0){h=c.substring(e,e+1);if(
q){if(h==q&&!x)q='';if(h=='\\')x=x?0:1;else x=0}else{if(h=='"'||h=="'"
)q=h;if(h=='{')d++;if(h=='}')d--}if(d>0)e++}c=c.substring(0,s)
+'new Function('+(a?a+',':'')+'"'+s_c2fe(c.substring(o+1,e))+'")'
+c.substring(e+1);s=c.indexOf('function')}fc+=s_c2fe(c)+';return s");'
eval(fc);return f}function s_gi(un,pg,ss){var c="function s_c(un,pg,s"
+"s){var s=this;s.wd=window;if(!s.wd.s_c_in){s.wd.s_c_il=new Array;s."
+"wd.s_c_in=0;}s._il=s.wd.s_c_il;s._in=s.wd.s_c_in;s._il[s._in]=s;s.w"
+"d.s_c_in++;s.m=function(m){return (''+m).indexOf('{')<0};s.fl=funct"
+"ion(x,l){return x?(''+x).substring(0,l):x};s.co=function(o){if(!o)r"
+"eturn o;var n=new Object,x;for(x in o)if(x.indexOf('select')<0&&x.i"
+"ndexOf('filter')<0)n[x]=o[x];return n};s.num=function(x){x=''+x;for"
+"(var p=0;p<x.length;p++)if(('0123456789').indexOf(x.substring(p,p+1"
+"))<0)return 0;return 1};s.rep=function(x,o,n){var i=x.indexOf(o),l="
+"n.length>0?n.length:1;while(x&&i>=0){x=x.substring(0,i)+n+x.substri"
+"ng(i+o.length);i=x.indexOf(o,i+l)}return x};s.ape=function(x){var s"
+"=this,i;x=x?s.rep(escape(''+x),'+','%2B'):x;if(x&&s.charSet&&s.em=="
+"1&&x.indexOf('%u')<0&&x.indexOf('%U')<0){i=x.indexOf('%');while(i>="
+"0){i++;if(('89ABCDEFabcdef').indexOf(x.substring(i,i+1))>=0)return "
+"x.substring(0,i)+'u00'+x.substring(i);i=x.indexOf('%',i)}}return x}"
+";s.epa=function(x){var s=this;return x?unescape(s.rep(''+x,'+',' ')"
+"):x};s.pt=function(x,d,f,a){var s=this,t=x,z=0,y,r;while(t){y=t.ind"
+"exOf(d);y=y<0?t.length:y;t=t.substring(0,y);r=s.m(f)?s[f](t,a):f(t,"
+"a);if(r)return r;z+=y+d.length;t=x.substring(z,x.length);t=z<x.leng"
+"th?t:''}return ''};s.isf=function(t,a){var c=a.indexOf(':');if(c>=0"
+")a=a.substring(0,c);if(t.substring(0,2)=='s_')t=t.substring(2);retu"
+"rn (t!=''&&t==a)};s.fsf=function(t,a){var s=this;if(s.pt(a,',','isf"
+"',t))s.fsg+=(s.fsg!=''?',':'')+t;return 0};s.fs=function(x,f){var s"
+"=this;s.fsg='';s.pt(x,',','fsf',f);return s.fsg};s.c_d='';s.c_gdf=f"
+"unction(t,a){var s=this;if(!s.num(t))return 1;return 0};s.c_gd=func"
+"tion(){var s=this,d=s.wd.location.hostname,n=s.fpCookieDomainPeriod"
+"s,p;if(!n)n=s.cookieDomainPeriods;if(d&&!s.c_d){n=n?parseInt(n):2;n"
+"=n>2?n:2;p=d.lastIndexOf('.');if(p>=0){while(p>=0&&n>1){p=d.lastInd"
+"exOf('.',p-1);n--}s.c_d=p>0&&s.pt(d,'.','c_gdf',0)?d.substring(p):d"
+"}}return s.c_d};s.c_r=function(k){var s=this;k=s.ape(k);var c=' '+s"
+".d.cookie,i=c.indexOf(' '+k+'='),e=i<0?i:c.indexOf(';',i),v=i<0?'':"
+"s.epa(c.substring(i+2+k.length,e<0?c.length:e));return v!='[[B]]'?v"
+":''};s.c_w=function(k,v,e){var s=this,d=s.c_gd(),l=s.cookieLifetime"
+",t;v=''+v;l=l?(''+l).toUpperCase():'';if(e&&l!='SESSION'&&l!='NONE'"
+"){t=(v!=''?parseInt(l?l:0):-60);if(t){e=new Date;e.setTime(e.getTim"
+"e()+(t*1000))}}if(k&&l!='NONE'){s.d.cookie=k+'='+s.ape(v!=''?v:'[[B"
+"]]')+'; path=/;'+(e&&l!='SESSION'?' expires='+e.toGMTString()+';':'"
+"')+(d?' domain='+d+';':'');return s.c_r(k)==v}return 0};s.eh=functi"
+"on(o,e,r,f){var s=this,b='s_'+e+'_'+s._in,n=-1,l,i,x;if(!s.ehl)s.eh"
+"l=new Array;l=s.ehl;for(i=0;i<l.length&&n<0;i++){if(l[i].o==o&&l[i]"
+".e==e)n=i}if(n<0){n=i;l[n]=new Object}x=l[n];x.o=o;x.e=e;f=r?x.b:f;"
+"if(r||f){x.b=r?0:o[e];x.o[e]=f}if(x.b){x.o[b]=x.b;return b}return 0"
+"};s.cet=function(f,a,t,o,b){var s=this,r;if(s.apv>=5&&(!s.isopera||"
+"s.apv>=7))eval('try{r=s.m(f)?s[f](a):f(a)}catch(e){r=s.m(t)?s[t](e)"
+":t(e)}');else{if(s.ismac&&s.u.indexOf('MSIE 4')>=0)r=s.m(b)?s[b](a)"
+":b(a);else{s.eh(s.wd,'onerror',0,o);r=s.m(f)?s[f](a):f(a);s.eh(s.wd"
+",'onerror',1)}}return r};s.gtfset=function(e){var s=this;return s.t"
+"fs};s.gtfsoe=new Function('e','var s=s_c_il['+s._in+'];s.eh(window,"
+"\"onerror\",1);s.etfs=1;var c=s.t();if(c)s.d.write(c);s.etfs=0;retu"
+"rn true');s.gtfsfb=function(a){return window};s.gtfsf=function(w){v"
+"ar s=this,p=w.parent,l=w.location;s.tfs=w;if(p&&p.location!=l&&p.lo"
+"cation.host==l.host){s.tfs=p;return s.gtfsf(s.tfs)}return s.tfs};s."
+"gtfs=function(){var s=this;if(!s.tfs){s.tfs=s.wd;if(!s.etfs)s.tfs=s"
+".cet('gtfsf',s.tfs,'gtfset',s.gtfsoe,'gtfsfb')}return s.tfs};s.ca=f"
+"unction(){var s=this,imn='s_i_'+s.fun;if(s.d.images&&s.apv>=3&&(!s."
+"isopera||s.apv>=7)&&(s.ns6<0||s.apv>=6.1)){s.ios=1;if(!s.d.images[i"
+"mn]&&(!s.isns||(s.apv<4||s.apv>=5))){s.d.write('<im'+'g name=\"'+im"
+"n+'\" height=1 width=1 border=0 alt=\"\">');if(!s.d.images[imn])s.i"
+"os=0}}};s.mr=function(sess,q,ta){var s=this,ns=s.visitorNamespace,u"
+"nc=s.rep(s.fun,'_','-'),imn='s_i_'+s.fun,im,b,e,rs='http'+(s.ssl?'s"
+"':'')+'://'+(s.ssl?'securemetrics':'metrics')+'.apple.com/b/ss/'+s.un+'/1"
+"/H.8-pdvu-2/'+sess+'?[AQB]&ndh=1'+(q?q:'')+(s.q?s.q:'')+'&[AQE]';if"
+"(s.isie&&!s.ismac){if(s.apv>5.5)rs=s.fl(rs,4095);else rs=s.fl(rs,20"
+"47)}if(s.ios||s.ss){if (!s.ss)s.ca();im=s.wd[imn]?s.wd[imn]:s.d.ima"
+"ges[imn];if(!im)im=s.wd[imn]=new Image;im.src=rs;if(rs.indexOf('&pe"
+"=')>=0&&(!ta||ta=='_self'||ta=='_top'||(s.wd.name&&ta==s.wd.name)))"
+"{b=e=new Date;while(e.getTime()-b.getTime()<500)e=new Date}return '"
+"'}return '<im'+'g sr'+'c=\"'+rs+'\" width=1 height=1 border=0 alt="
+"\"\">'};s.gg=function(v){var s=this;return s.wd['s_'+v]};s.glf=func"
+"tion(t,a){if(t.substring(0,2)=='s_')t=t.substring(2);var s=this,v=s"
+".gg(t);if(v)s[t]=v};s.gl=function(v){var s=this;s.pt(v,',','glf',0)"
+"};s.gv=function(v){var s=this;return s['vpm_'+v]?s['vpv_'+v]:(s[v]?"
+"s[v]:'')};s.havf=function(t,a){var s=this,b=t.substring(0,4),x=t.su"
+"bstring(4),n=parseInt(x),k='g_'+t,m='vpm_'+t,q=t,v=s.linkTrackVars,"
+"e=s.linkTrackEvents;s[k]=s.gv(t);if(s.lnk||s.eo){v=v?v+','+s.vl_l:'"
+"';if(v&&!s.pt(v,',','isf',t))s[k]='';if(t=='events'&&e)s[k]=s.fs(s["
+"k],e)}s[m]=0;if(t=='pageURL')q='g';else if(t=='referrer')q='r';else"
+" if(t=='vmk')q='vmt';else if(t=='charSet'){q='ce';if(s[k]&&s.em==2)"
+"s[k]='UTF-8'}else if(t=='visitorNamespace')q='ns';else if(t=='cooki"
+"eDomainPeriods')q='cdp';else if(t=='cookieLifetime')q='cl';else if("
+"t=='variableProvider')q='vvp';else if(t=='currencyCode')q='cc';else"
+" if(t=='channel')q='ch';else if(t=='campaign')q='v0';else if(s.num("
+"x)) {if(b=='prop')q='c'+n;else if(b=='eVar')q='v'+n;else if(b=='hie"
+"r'){q='h'+n;s[k]=s.fl(s[k],255)}}if(s[k]&&t!='linkName'&&t!='linkTy"
+"pe')s.qav+='&'+q+'='+s.ape(s[k]);return ''};s.hav=function(){var s="
+"this;s.qav='';s.pt(s.vl_t,',','havf',0);return s.qav};s.lnf=functio"
+"n(t,h){t=t?t.toLowerCase():'';h=h?h.toLowerCase():'';var te=t.index"
+"Of('=');if(t&&te>0&&h.indexOf(t.substring(te+1))>=0)return t.substr"
+"ing(0,te);return ''};s.ln=function(h){var s=this,n=s.linkNames;if(n"
+")return s.pt(n,',','lnf',h);return ''};s.ltdf=function(t,h){t=t?t.t"
+"oLowerCase():'';h=h?h.toLowerCase():'';var qi=h.indexOf('?');h=qi>="
+"0?h.substring(0,qi):h;if(t&&h.substring(h.length-(t.length+1))=='.'"
+"+t)return 1;return 0};s.ltef=function(t,h){t=t?t.toLowerCase():'';h"
+"=h?h.toLowerCase():'';if(t&&h.indexOf(t)>=0)return 1;return 0};s.lt"
+"=function(h){var s=this,lft=s.linkDownloadFileTypes,lef=s.linkExter"
+"nalFilters,lif=s.linkInternalFilters;lif=lif?lif:s.wd.location.host"
+"name;h=h.toLowerCase();if(s.trackDownloadLinks&&lft&&s.pt(lft,',','"
+"ltdf',h))return 'd';if(s.trackExternalLinks&&(lef||lif)&&(!lef||s.p"
+"t(lef,',','ltef',h))&&(!lif||!s.pt(lif,',','ltef',h)))return 'e';re"
+"turn ''};s.lc=new Function('e','var s=s_c_il['+s._in+'],b=s.eh(this"
+",\"onclick\");s.lnk=s.co(this);s.t();s.lnk=0;if(b)return this[b](e)"
+";return true');s.bc=new Function('e','var s=s_c_il['+s._in+'],f;if("
+"s.d&&s.d.all&&s.d.all.cppXYctnr)return;s.eo=e.srcElement?e.srcEleme"
+"nt:e.target;eval(\"try{if(s.eo&&(s.eo.tagName||s.eo.parentElement||"
+"s.eo.parentNode))s.t()}catch(f){}\");s.eo=0');s.ot=function(o){var "
+"a=o.type,b=o.tagName;return (a&&a.toUpperCase?a:b&&b.toUpperCase?b:"
+"o.href?'A':'').toUpperCase()};s.oid=function(o){var s=this,t=s.ot(o"
+"),p=o.protocol,c=o.onclick,n='',x=0;if(!o.s_oid){if(o.href&&(t=='A'"
+"||t=='AREA')&&(!c||!p||p.toLowerCase().indexOf('javascript')<0))n=o"
+".href;else if(c){n=s.rep(s.rep(s.rep(s.rep(''+c,\"\\r\",''),\"\\n\""
+",''),\"\\t\",''),' ','');x=2}else if(o.value&&(t=='INPUT'||t=='SUBM"
+"IT')){n=o.value;x=3}else if(o.src&&t=='IMAGE')n=o.src;if(n){o.s_oid"
+"=s.fl(n,100);o.s_oidt=x}}return o.s_oid};s.rqf=function(t,un){var s"
+"=this,e=t.indexOf('='),u=e>=0?','+t.substring(0,e)+',':'';return u&"
+"&u.indexOf(','+un+',')>=0?s.epa(t.substring(e+1)):''};s.rq=function"
+"(un){var s=this,c=un.indexOf(','),v=s.c_r('s_sq'),q='';if(c<0)retur"
+"n s.pt(v,'&','rqf',un);return s.pt(un,',','rq',0)};s.sqp=function(t"
+",a){var s=this,e=t.indexOf('='),q=e<0?'':s.epa(t.substring(e+1));s."
+"sqq[q]='';if(e>=0)s.pt(t.substring(0,e),',','sqs',q);return 0};s.sq"
+"s=function(un,q){var s=this;s.squ[un]=q;return 0};s.sq=function(q){"
+"var s=this,k='s_sq',v=s.c_r(k),x,c=0;s.sqq=new Object;s.squ=new Obj"
+"ect;s.sqq[q]='';s.pt(v,'&','sqp',0);s.pt(s.un,',','sqs',q);v='';for"
+"(x in s.squ)s.sqq[s.squ[x]]+=(s.sqq[s.squ[x]]?',':'')+x;for(x in s."
+"sqq)if(x&&s.sqq[x]&&(x==q||c<2)){v+=(v?'&':'')+s.sqq[x]+'='+s.ape(x"
+");c++}return s.c_w(k,v,0)};s.wdl=new Function('e','var s=s_c_il['+s"
+"._in+'],r=true,b=s.eh(s.wd,\"onload\"),i,o,oc;if(b)r=this[b](e);for"
+"(i=0;i<s.d.links.length;i++){o=s.d.links[i];oc=o.onclick?\"\"+o.onc"
+"lick:\"\";if((oc.indexOf(\"s_gs(\")<0||oc.indexOf(\".s_oc(\")>=0)&&"
+"oc.indexOf(\".tl(\")<0)s.eh(o,\"onclick\",0,s.lc);}return r');s.wds"
+"=function(){var s=this;if(s.apv>3&&(!s.isie||!s.ismac||s.apv>=5)){i"
+"f(s.b&&s.b.attachEvent)s.b.attachEvent('onclick',s.bc);else if(s.b&"
+"&s.b.addEventListener)s.b.addEventListener('click',s.bc,false);else"
+" s.eh(s.wd,'onload',0,s.wdl)}};s.vs=function(x){var s=this,v=s.visi"
+"torSampling,g=s.visitorSamplingGroup,k='s_vsn_'+s.un+(g?'_'+g:''),n"
+"=s.c_r(k),e=new Date,y=e.getYear();e.setYear(y+10+(y<1900?1900:0));"
+"if(v){v*=100;if(!n){if(!s.c_w(k,x,e))return 0;n=x}if(n%10000>v)retu"
+"rn 0}return 1};s.dyasmf=function(t,m){if(t&&m&&m.indexOf(t)>=0)retu"
+"rn 1;return 0};s.dyasf=function(t,m){var s=this,i=t?t.indexOf('='):"
+"-1,n,x;if(i>=0&&m){var n=t.substring(0,i),x=t.substring(i+1);if(s.p"
+"t(x,',','dyasmf',m))return n}return 0};s.uns=function(){var s=this,"
+"x=s.dynamicAccountSelection,l=s.dynamicAccountList,m=s.dynamicAccou"
+"ntMatch,n,i;s.un.toLowerCase();if(x&&l){if(!m)m=s.wd.location.host;"
+"if(!m.toLowerCase)m=''+m;l=l.toLowerCase();m=m.toLowerCase();n=s.pt"
+"(l,';','dyasf',m);if(n)s.un=n}i=s.un.indexOf(',');s.fun=i<0?s.un:s."
+"un.substring(0,i)};s.t=function(){var s=this,trk=1,tm=new Date,sed="
+"Math&&Math.random?Math.floor(Math.random()*10000000000000):tm.getTi"
+"me(),sess='s'+Math.floor(tm.getTime()/10800000)%10+sed,yr=tm.getYea"
+"r(),vt=tm.getDate()+'/'+tm.getMonth()+'/'+(yr<1900?yr+1900:yr)+' '+"
+"tm.getHours()+':'+tm.getMinutes()+':'+tm.getSeconds()+' '+tm.getDay"
+"()+' '+tm.getTimezoneOffset(),tfs=s.gtfs(),ta='',q='',qs='';s.uns()"
+";if(!s.q){var tl=tfs.location,x='',c='',v='',p='',bw='',bh='',j='1."
+"0',k=s.c_w('s_cc','true',0)?'Y':'N',hp='',ct='',pn=0,ps;if(s.apv>=4"
+")x=screen.width+'x'+screen.height;if(s.isns||s.isopera){if(s.apv>=3"
+"){j='1.1';v=s.n.javaEnabled()?'Y':'N';if(s.apv>=4){j='1.2';c=screen"
+".pixelDepth;bw=s.wd.innerWidth;bh=s.wd.innerHeight;if(s.apv>=4.06)j"
+"='1.3'}}s.pl=s.n.plugins}else if(s.isie){if(s.apv>=4){v=s.n.javaEna"
+"bled()?'Y':'N';j='1.2';c=screen.colorDepth;if(s.apv>=5){bw=s.d.docu"
+"mentElement.offsetWidth;bh=s.d.documentElement.offsetHeight;j='1.3'"
+";if(!s.ismac&&s.b){s.b.addBehavior('#default#homePage');hp=s.b.isHo"
+"mePage(tl)?\"Y\":\"N\";s.b.addBehavior('#default#clientCaps');ct=s."
+"b.connectionType}}}else r=''}if(s.pl)while(pn<s.pl.length&&pn<30){p"
+"s=s.fl(s.pl[pn].name,100)+';';if(p.indexOf(ps)<0)p+=ps;pn++}s.q=(x?"
+"'&s='+s.ape(x):'')+(c?'&c='+s.ape(c):'')+(j?'&j='+j:'')+(v?'&v='+v:"
+"'')+(k?'&k='+k:'')+(bw?'&bw='+bw:'')+(bh?'&bh='+bh:'')+(ct?'&ct='+s"
+".ape(ct):'')+(hp?'&hp='+hp:'')+(p?'&p='+s.ape(p):'')}if(s.usePlugin"
+"s)s.doPlugins(s);var l=s.wd.location,r=tfs.document.referrer;if(!s."
+"pageURL)s.pageURL=s.fl(l?l:'',255);if(!s.referrer)s.referrer=s.fl(r"
+"?r:'',255);if(s.lnk||s.eo){var o=s.eo?s.eo:s.lnk;if(!o)return '';va"
+"r p=s.gv('pageName'),w=1,t=s.ot(o),n=s.oid(o),x=o.s_oidt,h,l,i,oc;i"
+"f(s.eo&&o==s.eo){while(o&&!n&&t!='BODY'){o=o.parentElement?o.parent"
+"Element:o.parentNode;if(!o)return '';t=s.ot(o);n=s.oid(o);x=o.s_oid"
+"t}oc=o.onclick?''+o.onclick:'';if((oc.indexOf(\"s_gs(\")>=0&&oc.ind"
+"exOf(\".s_oc(\")<0)||oc.indexOf(\".tl(\")>=0)return ''}ta=n?o.targe"
+"t:1;"

/* Begin Modified Code for Relative Exit Links */
/*h=o.href?o.href:'';*/
+"var a_l=s.wd.location;"
+"h=o.href?o.href.indexOf('http')==-1&&o.href.indexOf('file')==-1?((o.protocol?o.protocol:a_l.protocol)+'//'+(o.hostname?o.hostname:"
+"a_l.hostname)+(o.pathname?o.pathname:'')+(o.search?(o.search.substring("
+"0,1)!='?'?'?':'')+o.search:'')):o.href:'';"
/* End Modified Code for Relative Exit Links */

+"i=h.indexOf('?');h=s.linkLeaveQueryString||i"
+"<0?h:h.substring(0,i);l=s.linkName?s.linkName:s.ln(h);t=s.linkType?"
+"s.linkType.toLowerCase():s.lt(h);if(t&&(h||l))q+='&pe=lnk_'+(t=='d'"
+"||t=='e'?s.ape(t):'o')+(h?'&pev1='+s.ape(h):'')+(l?'&pev2='+s.ape(l"
+"):'');else trk=0;if(s.trackInlineStats){if(!p){p=s.gv('pageURL');w="
+"0}t=s.ot(o);i=o.sourceIndex;if(s.gg('objectID')){n=s.gg('objectID')"
+";x=1;i=1}if(p&&n&&t)qs='&pid='+s.ape(s.fl(p,255))+(w?'&pidt='+w:'')"
+"+'&oid='+s.ape(s.fl(n,100))+(x?'&oidt='+x:'')+'&ot='+s.ape(t)+(i?'&"
+"oi='+i:'')}}if(!trk&&!qs)return '';if(s.p_r)s.p_r();var code='';if("
+"trk&&s.vs(sed))code=s.mr(sess,(vt?'&t='+s.ape(vt):'')+s.hav()+q+(qs"
+"?qs:s.rq(s.un)),ta);s.sq(trk?'':qs);s.lnk=s.eo=s.linkName=s.linkTyp"
+"e=s.wd.s_objectID=s.ppu='';return code};s.tl=function(o,t,n){var s="
+"this;s.lnk=s.co(o);s.linkType=t;s.linkName=n;s.t()};s.ssl=(s.wd.loc"
+"ation.protocol.toLowerCase().indexOf('https')>=0);s.d=document;s.b="
+"s.d.body;s.n=navigator;s.u=s.n.userAgent;s.ns6=s.u.indexOf('Netscap"
+"e6/');var apn=s.n.appName,v=s.n.appVersion,ie=v.indexOf('MSIE '),o="
+"s.u.indexOf('Opera '),i;if(v.indexOf('Opera')>=0||o>0)apn='Opera';s"
+".isie=(apn=='Microsoft Internet Explorer');s.isns=(apn=='Netscape')"
+";s.isopera=(apn=='Opera');s.ismac=(s.u.indexOf('Mac')>=0);if(o>0)s."
+"apv=parseFloat(s.u.substring(o+6));else if(ie>0){s.apv=parseInt(i=v"
+".substring(ie+5));if(s.apv>3)s.apv=parseFloat(i)}else if(s.ns6>0)s."
+"apv=parseFloat(s.u.substring(s.ns6+10));else s.apv=parseFloat(v);s."
+"em=0;if(String.fromCharCode){i=escape(String.fromCharCode(256)).toU"
+"pperCase();s.em=(i=='%C4%80'?2:(i=='%U0100'?1:0))}s.un=un;s.uns();s"
+".vl_l='vmk,ppu,charSet,visitorNamespace,cookieDomainPeriods,cookieL"
+"ifetime,pageName,pageURL,referrer,currencyCode,purchaseID';s.vl_t=s"
+".vl_l+',variableProvider,channel,server,pageType,campaign,state,zip"
+",events,products,linkName,linkType';for(var n=1;n<51;n++)s.vl_t+=',"
+"prop'+n+',eVar'+n+',hier'+n;s.vl_g=s.vl_t+',trackDownloadLinks,trac"
+"kExternalLinks,trackInlineStats,linkLeaveQueryString,linkDownloadFi"
+"leTypes,linkExternalFilters,linkInternalFilters,linkNames';if(pg)s."
+"gl(s.vl_g);s.ss=ss;if(!ss){s.wds();s.ca()}}",
l=window.s_c_il,n=navigator,u=n.userAgent,v=n.appVersion,e=v.indexOf(
'MSIE '),m=u.indexOf('Netscape6/'),a,i,s;if(l)for(i=0;i<l.length;i++){
s=l[i];s.uns();if(s.un==un)return s;else if(s.pt(s.un,',','isf',un)){
s=s.co(s);s.un=un;s.uns();return s}}if(e>0){a=parseInt(i=v.substring(e
+5));if(a>3)a=parseFloat(i)}else if(m>0)a=parseFloat(u.substring(m+10)
);else a=parseFloat(v);if(a>=5&&v.indexOf('Opera')<0&&u.indexOf(
'Opera')<0){eval(c);return new s_c(un,pg,ss)}else s=s_c2f(c);return s(
un,pg,ss)}

