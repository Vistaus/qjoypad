#include "quickset.h"

QuickSet::QuickSet( JoyPad* jp)
	: QDialog(){
	setting = false;
	joypad = jp;
	setCaption("Quick set " + joypad->getName());
	QVBoxLayout* LMain = new QVBoxLayout(this,5,5);
	LMain->setAutoAdd(true);
	
	new QLabel("Press any button or axis and\nyou will be prompted for a key.",this);
	QPushButton* button = new QPushButton("Done",this);
	connect( button, SIGNAL(clicked()), this, SLOT(accept()));
}

void QuickSet::jsevent( js_event msg ) {
	if (setting) return;
	if (msg.type == JS_EVENT_BUTTON) {
		Button* button = joypad->Buttons[msg.number];
		
		setting = true;
		int code = GetKey(button->getName(), true).exec();
		setting = false;
		
		if (code > 200) 
			button->setKey(true, code - 200);
		else
			button->setKey(false, code);
	}
	else {
		if (abs(msg.value) < 5000) return;
		Axis* axis = joypad->Axes[msg.number];
		
		setting = true;
		int code = GetKey(axis->getName() + ", " + QString((msg.value > 0)?"positive":"negative"), false).exec();
		setting = false;
		axis->setKey((msg.value > 0),code);
	}
}