#include "event.h"

Display* display;

void sendevent( xevent e ) {
	if (e.value1 == 0 && e.value2 == 0) return;
	if (e.type == WARP) {
		XTestFakeRelativeMotionEvent(display, e.value1, e.value2, 0);
	}
	else {
		if (e.type == KREL || e.type == KPRESS) {
			XTestFakeKeyEvent(display, e.value1, (e.type == KPRESS), 0);
		}
		else if (e.type == BREL | e.type == BPRESS) {
			XTestFakeButtonEvent(display, e.value1, (e.type == BPRESS), 0);
		}
	}
	XFlush(display);
}
