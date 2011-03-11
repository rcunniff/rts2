/**
 * Copyright (C) 2005 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "filterd.h"
#include "libfli.h"
#include "libfli-filter-focuser.h"

namespace rts2filterd
{

class Fli:public Filterd
{
	public:
		Fli (int in_argc, char **in_argv);
		virtual ~ Fli (void);
	protected:
		virtual int processOption (int in_opt);
		virtual int init (void);

		virtual int changeMasterState (int new_state);

		virtual int getFilterNum (void);
		virtual int setFilterNum (int new_filter);

		virtual int homeFilter ();

	private:
		flidev_t dev;
		flidomain_t deviceDomain;

		long filter_count;

		int fliDebug;
};

}

using namespace rts2filterd;

Fli::Fli (int in_argc, char **in_argv):Filterd (in_argc, in_argv)
{
	deviceDomain = FLIDEVICE_FILTERWHEEL | FLIDOMAIN_USB;
	fliDebug = FLIDEBUG_NONE;
	addOption ('D', "domain", 1, "CCD Domain (default to USB; possible values: USB|LPT|SERIAL|INET)");
	addOption ('b', "fli_debug", 1, "FLI debug level (1, 2 or 3; 3 will print most error message to stdout)");
}

Fli::~Fli (void)
{
	FLIClose (dev);
}

int Fli::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'D':
			deviceDomain = FLIDEVICE_FILTERWHEEL;
			if (!strcasecmp ("USB", optarg))
				deviceDomain |= FLIDOMAIN_USB;
			else if (!strcasecmp ("LPT", optarg))
				deviceDomain |= FLIDOMAIN_PARALLEL_PORT;
			else if (!strcasecmp ("SERIAL", optarg))
				deviceDomain |= FLIDOMAIN_SERIAL;
			else if (!strcasecmp ("INET", optarg))
				deviceDomain |= FLIDOMAIN_INET;
			else
				return -1;
			break;
		case 'b':
			switch (atoi (optarg))
			{
				case 1:
					fliDebug = FLIDEBUG_FAIL;
					break;
				case 2:
					fliDebug = FLIDEBUG_FAIL | FLIDEBUG_WARN;
					break;
				case 3:
					fliDebug = FLIDEBUG_ALL;
					break;
				default:
					return -1;
			}
			break;
		default:
			return Filterd::processOption (in_opt);
	}
	return 0;
}

int Fli::init (void)
{
	LIBFLIAPI ret;
	char **names;
	char *nam_sep;

	ret = Filterd::init ();
	if (ret)
		return ret;

	if (fliDebug)
		FLISetDebugLevel (NULL, fliDebug);

	ret = FLIList (deviceDomain, &names);
	if (ret)
		return -1;

	if (names[0] == NULL)
	{
		logStream (MESSAGE_ERROR) << "Fli::init No device found!"
			<< sendLog;
		return -1;
	}

	nam_sep = strchr (names[0], ';');
	if (nam_sep)
		*nam_sep = '\0';

	ret = FLIOpen (&dev, names[0], deviceDomain);
	FLIFreeList (names);
	if (ret)
		return -1;

	ret = FLIGetFilterCount (dev, &filter_count);
	if (ret)
		return -1;

	return 0;
}

int Fli::changeMasterState (int new_state)
{
	if ((new_state & SERVERD_STATUS_MASK) == SERVERD_DAY || (new_state & SERVERD_STATUS_MASK) == SERVERD_STANDBY_MASK || new_state == SERVERD_SOFT_OFF || new_state == SERVERD_HARD_OFF)
		homeFilter ();
	return Filterd::changeMasterState (new_state);
}

int Fli::getFilterNum (void)
{
	long ret_f;
	LIBFLIAPI ret;
	ret = FLIGetFilterPos (dev, &ret_f);
	if (ret)
		return -1;
	return (int) ret_f;
}

int Fli::setFilterNum (int new_filter)
{
	LIBFLIAPI ret;
	if (new_filter < -1 || new_filter >= filter_count)
		return -1;

	ret = FLISetFilterPos (dev, new_filter);
	if (ret)
		return -1;
	return Filterd::setFilterNum (new_filter);
}

int Fli::homeFilter ()
{
	return setFilterNum (FLI_FILTERPOSITION_HOME);
}

int main (int argc, char **argv)
{
	Fli device = Fli (argc, argv);
	return device.run ();
}
