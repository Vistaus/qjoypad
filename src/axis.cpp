#include "axis.h"

#define sqr(a) ((a)*(a))


Axis::Axis( int i ) {
	index = i;
	isOn = false;
	//to keep toDefault from calling tossTimer without first calling takeTimer
	gradient = false;
	toDefault();
}


Axis::~Axis() {
	release();
}

bool Axis::read( QTextStream* stream ) {
//	At this point, toDefault has just been called.

	//read in a line from the stream, and split it up into individual words
	QString input = stream->readLine().lower();
	QRegExp regex("[\\s,]+");
	QStringList words = QStringList::split(regex,input);
	
	//used to assure QString->int conversions worked
	bool ok;
	//int to store values derived from strings
	int val;

	//step through each word, check if it's a token we recognize
    for ( QStringList::Iterator it = words.begin(); it != words.end(); ++it ) {
        if (*it == "maxspeed") {
			++it; //move to the next word, which should be the maximum speed.
			//if no value was given, there's an error in the file, stop reading.
			if (it == words.end()) return false;
			//try to convert the value.
			val = (*it).toInt(&ok);
			//if that worked and the maximum speed is in range, set it.
			if (ok && val >= 0 && val <= MAXMOUSESPEED) maxSpeed = val;
			//otherwise, faulty input, give up.
			else return false;
		}
		//pretty much the same process for getting the dead zone
		else if (*it == "dzone") {
			++it;
			if (it == words.end()) return false;
			val = (*it).toInt(&ok);
			if (ok && val >= 0 && val <= JOYMAX) dZone = val;
			else return false;
		}
		//and again for the extreme zone,
		else if (*it == "xzone") {
			++it;
			if (it == words.end()) return false;
			val = (*it).toInt(&ok);
			if (ok && val >= 0 && val <= JOYMAX) xZone = val;
			else return false;
		}
		//and for the positive keycode,
		else if (*it == "+key") {
			++it;
			if (it == words.end()) return false;
			val = (*it).toInt(&ok);
			if (ok && val >= 0 && val <= MAXKEY) pkeycode = val;
			else return false;
		}
		//and finally for the negative keycode.
		else if (*it == "-key") {
			++it;
			if (it == words.end()) return false;
			val = (*it).toInt(&ok);
			if (ok && val >= 0 && val <= MAXKEY) nkeycode = val;
			else return false;
		}
		//the rest of the options are keywords without integers
		else if (*it == "gradient") {
			if (!gradient) takeTimer( this );
			gradient = true;
		}
		else if (*it == "throttle+") {
			throttle = 1;
		}
		else if (*it == "throttle-") {
			throttle = -1;
		}
		else if (*it == "mouse+v") {
			mode = mousepv;
		}
		else if (*it == "mouse-v") {
			mode = mousenv;
		}
		else if (*it == "mouse+h") {
			mode = mouseph;
		}
		else if (*it == "mouse-h") {
			mode = mousenh;
		}
		//we ignore unrecognized words to be friendly and allow for additions to
		//the format in later versions. Note, this means that typos will not get
		//the desired effect OR produce an error message.
    }

	//assume that xZone, dZone, or maxSpeed has changed, for simplicity.
	//do a few floating point calculations.
	adjustGradient();
	
	//if we parsed through all of the words, yay! All done.
	return true;
}

void Axis::write( QTextStream* stream ) {
	*stream << "\t" << getName() << ": ";
	if (gradient)  *stream << "gradient, ";
	if (throttle > 0) *stream << "throttle+, ";
	else if (throttle < 0) *stream << "throttle-, ";
	if (dZone != DZONE) *stream << "dZone " << dZone << ", ";
	if (xZone != XZONE) *stream << "xZone " << xZone << ", ";
	if (mode == keybd) {
		*stream << "+key " << pkeycode << ", "
		        << "-key " << nkeycode << "\n";
	}
	else {
		if (gradient) *stream << "maxSpeed " << maxSpeed << ", ";
		*stream << "mouse";
		if (mode == mousepv)
			*stream << "+v\n";
		else if (mode == mousenv)
			*stream << "-v\n";
		else if (mode == mouseph)
			*stream << "+h\n";
		else if (mode == mousenh)
			*stream << "-h\n";
	}
	
}

void Axis::release() {
	//if we're pressing a key, let it go.
	if (isDown) {
		move(false);
		isDown = false;
	}
}

void Axis::jsevent( int value ) {
	//adjust real value to throttle value
	if (throttle == 0)
		state = value;
	else if (throttle == -1)
		state = (value + JOYMIN) / 2;
	else
		state = (value + JOYMAX) / 2;
		
	//set isOn, deal with state changing.
	//if was on but now should be off:	
	if (isOn && abs(state) <= dZone) {
		isOn = false;
		if (gradient) release();
	}
	//if was off but now should be on:
	else if (!isOn && abs(state) >= dZone) {
		isOn = true;
		if (gradient) duration = (abs(state) * FREQ) / JOYMAX;
	}
	//otherwise, state doesn't change! Don't touch it.
	else return;
	
	//gradient will trigger movement on its own via timer().
	//non-gradient needs to be told to move.
	if (!gradient) {
		move(isOn);
	}
}

void Axis::toDefault() {
	release();
	
	if (gradient) tossTimer( this );
	gradient = false;
	throttle = 0;
	maxSpeed = 100;
	dZone = DZONE;
	xZone = XZONE;
	mode = keybd;
	pkeycode = 0;
	nkeycode = 0;
	downkey = 0;
	state = 0;
	adjustGradient();
}

bool Axis::isDefault() {
    return (gradient == false) &&
           (throttle == 0) &&
           (maxSpeed == 100) &&
           (dZone == DZONE) &&
           (xZone == XZONE) &&
           (mode == keybd) &&
           (pkeycode == 0) &&
           (nkeycode == 0);
}

bool Axis::inDeadZone( int val ) {
	int value;
	if (throttle == 0)
		value = val;
	else if (throttle == -1)
		value = (val + JOYMIN) / 2;
	else
		value = (val + JOYMAX) / 2;
	return (abs(value) < dZone);
}

QString Axis::status() {
	QString result = getName() + " : [";
	if (mode == keybd) {
		if (throttle == 0)
			result += "KEYBOARD";
		else result += "THROTTLE";
	}
	else result += "MOUSE";
	return result + "]";
}

void Axis::setKey(bool positive, int value) {
	if (positive)
		pkeycode = value;
	else
		nkeycode = value;
}

void Axis::timer( int tick ) {
	if (isOn) {
		if (mode == keybd) {
			if (tick % FREQ == 0) {
				if (duration == FREQ) {
					if (!isDown) move(true);
					duration = (abs(state) * FREQ) / JOYMAX;
					return;
				}
				move(true);
			}
			if (tick % FREQ == duration) {
				move(false);
				duration = (abs(state) * FREQ) / JOYMAX;
			}
		}
		else {
			move(true);
		}
	}
}

void Axis::adjustGradient() {
	//create a nice quadratic curve fitting it to the points
	//(dZone,0) and (xZone,MaxSpeed)
	a = (double) (maxSpeed) / sqr(xZone - dZone);
	b = -2 * a * dZone;
	c = a * sqr(dZone);
	//actual equation for curve is: y = ax^2 + b
	//where x is the state of the axis and y is the distance the mouse should move.
}

void Axis::move( bool press ) {
	xevent e;
	if (mode == keybd) {
		//prevent KeyPress-KeyPress and KeyRelease-KeyRelease pairs.
		//this would only happen in odd circumstances involving the setup
		//dialog being open and blocking events from happening.
		if (isDown == press) return;
		isDown = press;
		if (press) {
			e.type = KPRESS;
			downkey = (state > 0)?pkeycode:nkeycode;
		}
		else {
			e.type = KREL;
		}
		e.value1 = downkey;
		e.value2 = 0;
	}
	//if using the mouse
	else if (press){
		int dist;
		if (gradient) {
			//calculate our mouse speed curve based on calculations made in
			//adjustGradient()
			int absState = abs(state);
			if (absState >= xZone) dist = maxSpeed;
			else if (absState <= dZone) dist = 0;
			else dist = (int) (a*sqr(absState) + b*absState + c);
		}
		//if not gradient, always go full speed.
		else dist = maxSpeed;

		//if we're on the negative side	of the axis, must compensate for
		//squaring and make distance negative.
		if (state < 0) dist = -dist;
		e.type = WARP;
		if (mode == mousepv) {
			e.value1 = 0;
			e.value2 = dist;
		}
		else if (mode == mousenv) {
			e.value1 = 0;
			e.value2 = -dist;
		}
		else if (mode == mouseph) {
			e.value1 = dist;
			e.value2 = 0;
		}
		else if (mode == mousenh) {
			e.value1 = -dist;
			e.value2 = 0;
		}
	}
	//actually create the event
	sendevent(e);
}
