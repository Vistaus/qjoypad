#define MAIN

//to create a qapplication
#include <qapplication.h>
//for basic file i/o and argument parsing
#include <qstring.h>
#include <qdir.h>
#include <qfile.h>
#include <qstringlist.h>
#include <qregexp.h>

//for ouput when there is no GUI going
#include <stdio.h>

//to create and handle signals for various events
#include <signal.h>

//for our custom-made event loop
#include "loop.h"
//to load layouts
#include "layout.h"
//to give event.h the current X11 display
#include "event.h"
//to update the joystick device list
#include "device.h"
//to produce errors!
#include "error.h"

//for making universally available variables
extern Display* display;			//from event.h
QIntDict<JoyPad> available;			//to device.h
QIntDict<JoyPad> joypads;           //to device.h
QPtrList<Component> timerList;		//to timer.h

//to be made available in timer.h
//they simply pass on the message to JoyLoop
void takeTimer( Component* c ) {
	((JoyLoop*) qApp->eventLoop())->takeTimer( c );
}
void tossTimer( Component* c ) {
	((JoyLoop*) qApp->eventLoop())->tossTimer( c );
}

//variables needed in various functions in this file
LayoutManager* lm;
QString devdir = DEVDIR;



//update the joystick devices!
void buildJoyDevices() {
	//reset all joydevs to sentinal value (-1)
    QIntDictIterator<JoyPad> it( joypads );
    for ( ; it.current(); ++it ) {
		it.current()->unsetDev();
	}

	//clear out the list of previously available joysticks
	available.clear();

	//set all joydevs anew (create new JoyPad's if necesary)
	QDir DeviceDir(devdir);
	QStringList devices = DeviceDir.entryList("js*", QDir::System );
	QRegExp devicename(".*\\js(\\d+)");
	int joydev;
	int index;
	//for every joystick device in the directory listing...
	//(note, with devfs, only available devices are listed)
	for (QStringList::Iterator it = devices.begin(); it != devices.end(); ++it) {
		//try opening the device.
		joydev = open( devdir + "/" + (*it), O_RDONLY | O_NONBLOCK);
		//if it worked, then we have a live joystick! Make sure it's properly
		//setup.
		if (joydev > 0) {
			devicename.match(*it);
			index = QString(devicename.cap(1)).toInt();
			JoyPad* joypad;
			//if we've never seen this device before, make a new one!
			if (joypads[index] == 0) {
				joypad = new JoyPad( index, joydev );
				joypads.insert(index,joypad);
			}
			else {
				joypad = joypads[index];
				joypad->resetToDev(joydev);
			}
			//make this joystick device available.
			available.insert(index,joypad);
		}
	}

	//when it's all done, rebuild the popup menu so it displays the correct
	//information.
	lm->fillPopup();
}



//signal handler for SIGIO
//SIGIO means that a new layout should be loaded. It is saved in
// ~/.qjoypad/layout, where the last used layout is put.
void catchSIGIO( int sig )
{
	lm->load();
	//remember to catch this signal again next time.
	signal( sig, catchSIGIO );
}



//signal handler for SIGUSR1
//SIGUSR1 means that we should update the available joystick device list.
void catchSIGUSR1( int sig ){
	buildJoyDevices();
	//remember to catch this signal again next time.
	signal( sig, catchSIGUSR1 );
}


/*  A new feature to add? We'll see.
void catchSIGUSR2( int sig ) {
	lm->trayClick();
	signal( sig, catchSIGUSR2 );
}
*/



int main( int argc, char **argv )
{
	//create a new event loop. This will be captured by the QApplication
	//when it gets created
	JoyLoop loop;
    QApplication a( argc, argv );


	//where to look for settings. If it does not exist, it will be created
	QDir dir(settingsDir);

	//if there is no new directory and we can't make it, complain
	if (!dir.exists() && !dir.mkdir(settingsDir)) {
		printf("Couldn't create the QJoyPad save directory (" + settingsDir + ")!");
		return 1;
	}


	//start out with no special layout.
	QString layout = "";
	//by default, we use a tray icon
	bool useTrayIcon = true;
	//this execution wasn't made to update the joystick device list.
	bool update = false;
	
	
	//parse command-line options
	for (int i = 1; i < a.argc(); i++) {
		//if a device directory was specified,
		if (QRegExp("-{1,2}device").exactMatch(a.argv()[i])) {
			++i;
			if (i < a.argc()) {
				if (QFile::exists(a.argv()[i])) {
					devdir = a.argv()[i];
				}
				else {
					error("Command Line Argument Problems", "No such directory: " + QString(a.argv()[i]));
					continue;
				}
			}
		}
		//if no-tray mode was requested,
		else if (QRegExp("-{1,2}notray").exactMatch(a.argv()[i])) {
			useTrayIcon = false;
		}
		//if this execution is just meant to update the joystick devices,
		else if (QRegExp("-{1,2}update").exactMatch(a.argv()[i])) {
			update = true;
		}
		//if help was requested,
		else if (QRegExp("-{1,2}h(elp)?").exactMatch(a.argv()[i])) {
		 	printf(NAME"\nUsage: qjoypad [--device \"/device/path\"] [--notray] [\"layout name\"]\n\nOptions:\n  --device path     Look for joystick devices in \"path\". This should\n                    be something like \"/dev/input\" if your game\n                    devices are in /dev/input/js0, /dev/input/js1, etc.\n  --notray          Do not use a system tray icon. This is useful for\n                    window managers that don't support this feature.\n  --update          Force a running instance of QJoyPad to update its\n                    list of devices and layouts.\n  \"layout name\"     Load the given layout in an already running\n                    instance of QJoyPad, or start QJoyPad using the\n                    given layout.\n");
			return 1;
		}
		//in all other cases, an argument is assumed to be a layout name.
		//note: only the last layout name given will be used.
		else layout = a.argv()[i];
	}

	//if the user specified a layout to use,
	if (layout != "")
	{
		//then we try to store that layout in the last-used layout spot, to be
		//loaded by default.
		QFile file( settingsDir + "layout");
		if (file.open(IO_WriteOnly))
		{
			QTextStream stream( &file );
			stream << layout;
			file.close();
		}
	}

	
	
	
	
	//create a pid lock file.
	QFile pidFile( "/tmp/qjoypad.pid" );
	//if that file already exists, then qjoypad is already running!
	if (pidFile.exists())
	{
  		int pid;
  		if (pidFile.open( IO_ReadOnly ));
		{
			//try to get that pid...
			QTextStream( &pidFile ) >> pid;
			pidFile.close();
			//if we can signal the pid (ie, if the process is active)
			if (kill(pid,0) == 0)
			{
				//then prevent two instances from running at once.
				//however, if we are setting the layout or updating the device
				//list, this is not an error and we shouldn't make one!
				if (layout == "" && update == false) error("Instance Error","There is already a running instance of QJoyPad; please close\nthe old instance before starting a new one.");
				else  {
					//if one of these is the case, send the approrpriate signal!
					if (update == true)
						kill(pid,SIGUSR1);
					if (layout != "")
						kill(pid,SIGIO);
				}
				//and quit. We don't need two instances.
				return 0;
			}
		}
	}
	//now we can try to create and write our pid to the lock file.
	if (pidFile.open( IO_WriteOnly ))
	{
		QTextStream( &pidFile ) << getpid();
		pidFile.close();
	}

	
	
	
	//prepare the signal handlers
	signal( SIGIO,   catchSIGIO );
	signal( SIGUSR1, catchSIGUSR1 );
//	signal( SIGUSR2, catchSIGUSR2 );
	
	
	
	
	//capture the current display for event.h
	display = QPaintDevice::x11AppDisplay();

	//create a new LayoutManager with a tray icon / floating icon, depending
	//on the user's request
	lm = new LayoutManager(useTrayIcon);

	//build the joystick device list for the first time,
	buildJoyDevices();

	//load the last used layout (Or the one given as a command-line argument)	
	lm->load();
	
	//and run the program!
    int result = a.exec();
	
	//when everything is done, save the current layout for next time...
	lm->saveDefault();
	
	//remove the lock file...
	pidFile.remove();
	
	//and terminate!
	return result;
}
