if (typeof(AC) == "undefined") {AC = {};}
AC.AosCheck = function() {	
	if (document.location.search && s_account) {
		var dls = document.location.search;
		if (dls.indexOf("?cid=AOS-") > -1 || dls.indexOf("&cid=AOS-") > -1)
			s_account += ",applestoreWW";
	}
}
AC.AosCheck();
