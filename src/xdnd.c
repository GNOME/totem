/* Took WindowMaker implementation and adopted for MPlayer */
/* Took the MPlayer implementation and adapted it for Totem ;)
 * 			Bastien Nocera <hadess@hadess.net> */

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include "xdnd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>


#define XDND_VERSION 3L

Atom _XA_XdndAware;
Atom _XA_XdndEnter;
Atom _XA_XdndLeave;
Atom _XA_XdndDrop;
Atom _XA_XdndPosition;
Atom _XA_XdndStatus;
Atom _XA_XdndActionCopy;
Atom _XA_XdndSelection;
Atom _XA_XdndFinished;
Atom _XA_XdndTypeList;

Atom atom_support;

void wXDNDInitialize()
{

    _XA_XdndAware = XInternAtom(gdk_display, "XdndAware", False);
    _XA_XdndEnter = XInternAtom(gdk_display, "XdndEnter", False);
    _XA_XdndLeave = XInternAtom(gdk_display, "XdndLeave", False);
    _XA_XdndDrop = XInternAtom(gdk_display, "XdndDrop", False);
    _XA_XdndPosition = XInternAtom(gdk_display, "XdndPosition", False);
    _XA_XdndStatus = XInternAtom(gdk_display, "XdndStatus", False);
    _XA_XdndActionCopy = XInternAtom(gdk_display, "XdndActionCopy", False);
    _XA_XdndSelection = XInternAtom(gdk_display, "XdndSelection", False);
    _XA_XdndFinished = XInternAtom(gdk_display, "XdndFinished", False);
    _XA_XdndTypeList = XInternAtom(gdk_display, "XdndTypeList", False);
}

void wXDNDMakeAwareness(Window window) {
    long int xdnd_version = XDND_VERSION;
    XChangeProperty (gdk_display, window, _XA_XdndAware, XA_ATOM,
            32, PropModeAppend, (char *)&xdnd_version, 1);
}

void wXDNDClearAwareness(Window window) {
    XDeleteProperty (gdk_display, window, _XA_XdndAware);
}

GList *
wXDNDProcessSelection(Window window, XEvent *event)
{
    Atom ret_type;
    int ret_format, num;
    unsigned long ret_items;
    unsigned long remain_byte;
    char *delme, *retain, *file;
    XEvent xevent;
    GList *files = NULL;

    Window selowner = XGetSelectionOwner(gdk_display,_XA_XdndSelection);

    XGetWindowProperty(gdk_display, event->xselection.requestor,
            event->xselection.property,
            0, 65536, True, atom_support, &ret_type, &ret_format,
            &ret_items, &remain_byte, (unsigned char **)&delme);

    /*send finished*/
    memset (&xevent, 0, sizeof(xevent));
    xevent.xany.type = ClientMessage;
    xevent.xany.display = gdk_display;
    xevent.xclient.window = selowner;
    xevent.xclient.message_type = _XA_XdndFinished;
    xevent.xclient.format = 32;
    XDND_FINISHED_TARGET_WIN(&xevent) = window;
    XSendEvent(gdk_display, selowner, 0, 0, &xevent);

    if (!delme)
      return NULL;

    /* Handle dropped files */
    num = 0;

    retain = delme;

    while(retain < delme + ret_items) {
      file = retain;

    /* now check for special characters */
    {
      int newone = 0;
      while(retain < (delme + ret_items)){
        if(*retain == '\r' || *retain == '\n'){
          *retain=0;
	  newone = 1;
      } else {
        if (newone)
          break;
      }
      retain++;
      }
    }

    /* add the "retain" to the list */
    files = g_list_prepend (files, g_strdup (file));

    }

    free (delme);
    return files;
}

Bool
wXDNDProcessClientMessage(XClientMessageEvent *event)
{
#if 0
  {
    char * name = XGetAtomName(gdk_display, event->message_type);
    printf("Got %s\n",name);
    XFree(name);
  }
#endif

  if (event->message_type == _XA_XdndEnter) {
    Atom ok = XInternAtom(gdk_display, "text/uri-list", False);
    atom_support = None;
    if ((event->data.l[1] & 1) == 0){
      int index;
      for(index = 0; index <= 2 ; index++){
	if (event->data.l[2+index] == ok) {
	  atom_support = ok;
	}
      }
      if (atom_support == None) {
	printf("This doesn't seem as a file...\n");
      }
    } else {
      /* need to check the whole list here */
      unsigned long ret_left = 1;
      int offset = 0;
      Atom* ret_buff;
      Atom ret_type;
      int ret_format;
      unsigned long ret_items;

      /* while there is data left...*/
      while(ret_left && atom_support == None){
	XGetWindowProperty(gdk_display,event->data.l[0],_XA_XdndTypeList,
			   offset,256,False,XA_ATOM,&ret_type,
			   &ret_format,&ret_items,&ret_left,
			   (unsigned char**)&ret_buff);
	
	/* sanity checks...*/
	if(ret_buff == NULL || ret_type != XA_ATOM || ret_format != 8*sizeof(Atom)){
	  XFree(ret_buff);
	  break;
	}
	/* now chek what we've got */
	{
	  int i;
	  for(i=0; i<ret_items; i++){
	    if(ret_buff[i] == ok){
	      atom_support = ok;
	      break;
	    }
	  }
	}
	/* maybe next time ... */
	XFree(ret_buff);
	offset += 256;
      }
    }
    return True;
  }
  
  if (event->message_type == _XA_XdndLeave) {
    return True;
  }
  
  if (event->message_type == _XA_XdndDrop) {
    if (event->data.l[0] != XGetSelectionOwner(gdk_display, _XA_XdndSelection)){
      puts("Wierd selection owner... QT?");
    }
    if (atom_support != None) {
      XConvertSelection(gdk_display, _XA_XdndSelection, atom_support,
			_XA_XdndSelection, event->window,
			CurrentTime);
    }
    return True;
  }
  
  if (event->message_type == _XA_XdndPosition) {
    Window srcwin = event->data.l[0];
    if (atom_support == None){
      return True;
    }

    /* send response */
    {
      XEvent xevent;
      memset (&xevent, 0, sizeof(xevent));
      xevent.xany.type = ClientMessage;
      xevent.xany.display = gdk_display;
      xevent.xclient.window = srcwin;
      xevent.xclient.message_type = _XA_XdndStatus;
      xevent.xclient.format = 32; 
      
      XDND_STATUS_TARGET_WIN (&xevent) = event->window;
      XDND_STATUS_WILL_ACCEPT_SET (&xevent, True);
      XDND_STATUS_WANT_POSITION_SET(&xevent, True);
      /* actually need smth real here */
      XDND_STATUS_RECT_SET(&xevent, 0, 0, 1024,768);
      XDND_STATUS_ACTION(&xevent) = _XA_XdndActionCopy;
      
      XSendEvent(gdk_display, srcwin, 0, 0, &xevent);
    }
    return True;
  }
  
  return False;
}
