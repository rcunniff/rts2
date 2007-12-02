/*
 * Script support.
 * Copyright (C) 2005-2007 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __RTS2_SCRIPT__
#define __RTS2_SCRIPT__

#include "rts2scriptelement.h"
#include <config.h>

#ifdef HAVE_PGSQL
#include "rts2scriptelementacquire.h"
#endif							 /* HAVE_PGSQL */

#include "../utils/rts2block.h"
#include "../utils/rts2command.h"
#include "../utils/rts2devclient.h"
#include "../utils/rts2target.h"
#include "../writers/rts2image.h"

#include <list>

#define NEXT_COMMAND_NEXT        -2
#define NEXT_COMMAND_END_SCRIPT        -1
#define NEXT_COMMAND_KEEP         1
#define NEXT_COMMAND_WAITING      2
#define NEXT_COMMAND_RESYNC   3
#define NEXT_COMMAND_CHECK_WAIT   4
#define NEXT_COMMAND_PRECISION_FAILED   5
// you should not return NEXT_COMMAND_PRECISION_OK on first nextCommand call
#define NEXT_COMMAND_PRECISION_OK 6
#define NEXT_COMMAND_WAIT_ACQUSITION    7
#define NEXT_COMMAND_ACQUSITION_IMAGE   8

#define NEXT_COMMAND_WAIT_SIGNAL  9
#define NEXT_COMMAND_WAIT_MIRROR  10

#define NEXT_COMMAND_WAIT_SEARCH  11

// when return value contains that mask, we will keep command,
// as we get it from block
#define NEXT_COMMAND_MASK_BLOCK   0x100000

#define EVENT_OK_ASTROMETRY RTS2_LOCAL_EVENT + 200
#define EVENT_NOT_ASTROMETRY  RTS2_LOCAL_EVENT + 201
#define EVENT_ALL_PROCESSED RTS2_LOCAL_EVENT + 202

class Rts2ScriptElement;
class Rts2SEBAcquired;

/**
 * Holds script to execute on given device.
 * Script might include commands to other devices; in such case device
 * name must be given in script, separated from command with .
 * (point).
 *
 * It should be extended to enable actions depending on results of
 * observations etc..now we have only small core to build on.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2Script:public Rts2Object
{
	private:
		char *cmdBuf;
		std::string wholeScript;
		char *cmdBufTop;
		char *commandStart;

		char defaultDevice[DEVICE_NAME_SIZE];

		Rts2ScriptElement *currScriptElement;

		// test whenewer next element is one that is given..
		bool isNext (const char *element);
		char *nextElement ();
		int getNextParamString (char **val);
		int getNextParamFloat (float *val);
		int getNextParamDouble (double *val);
		int getNextParamInteger (int *val);
		// we should not save reference to target, as it can be changed|deleted without our knowledge
		Rts2ScriptElement *parseBuf (Rts2Target * target,
			struct ln_equ_posn *target_pos);
		std::list < Rts2ScriptElement * >elements;
		std::list < Rts2ScriptElement * >::iterator el_iter;
		Rts2Block *master;
		// is >= 0 when script runs, will become -1 when script is deleted (in beging of script destructor
		int executedCount;
	public:
		Rts2Script (Rts2Block * in_master, const char *cam_name,
			Rts2Target * target);
		virtual ~ Rts2Script (void);
		virtual void postEvent (Rts2Event * event);
		template < typename T > int nextCommand (T & device,
			Rts2Command ** new_command,
			char
			new_device[DEVICE_NAME_SIZE]);
		// returns -1 when there wasn't any error, otherwise index of element that wasn't parsed
		int getFaultLocation ()
		{
			if (*cmdBufTop == '\0')
				return -1;
			return (cmdBufTop - cmdBuf);
		}
		int isLastCommand (void)
		{
			return (el_iter == elements.end ());
		}
		void getDefaultDevice (char new_device[DEVICE_NAME_SIZE])
		{
			strncpy (new_device, defaultDevice, DEVICE_NAME_SIZE);
		}
		char *getDefaultDevice ()
		{
			return defaultDevice;
		}
		int processImage (Rts2Image * image);
		Rts2Block *getMaster ()
		{
			return master;
		}
		int getExecutedCount ()
		{
			return executedCount;
		}
		const char *getScriptBuf ()
		{
			return cmdBuf;
		}
		const std::string getWholeScript ()
		{
			return wholeScript;
		}
		int getParsedStartPos ()
		{
			return commandStart - cmdBuf;
		}

		int idle ();
};

template < typename T > int
Rts2Script::nextCommand (T & device,
Rts2Command ** new_command,
char new_device[DEVICE_NAME_SIZE])
{
	int ret;

	*new_command = NULL;

	if (executedCount < 0)
	{
		return NEXT_COMMAND_END_SCRIPT;
	}

	while (1)
	{
		if (el_iter == elements.end ())
			// command not found, end of script,..
			return NEXT_COMMAND_END_SCRIPT;
		currScriptElement = *el_iter;
		ret = currScriptElement->nextCommand (&device, new_command, new_device);
		// send info about currently executed script element..
		device.queCommand (new
			Rts2CommandChangeValue (&device, "scriptPosition", '=',
			currScriptElement->getStartPos ()));
		device.queCommand (new
			Rts2CommandChangeValue (&device, "scriptLen", '=',
			currScriptElement->getLen ()));
		if (ret != NEXT_COMMAND_NEXT)
		{
			break;
		}
		// move to next command
		el_iter++;
	}
	if (ret & NEXT_COMMAND_MASK_BLOCK)
	{
		ret &= ~NEXT_COMMAND_MASK_BLOCK;
	}
	else
	{
		switch (ret)
		{
			case 0:
			case NEXT_COMMAND_CHECK_WAIT:
			case NEXT_COMMAND_PRECISION_FAILED:
			case NEXT_COMMAND_PRECISION_OK:
			case NEXT_COMMAND_WAIT_ACQUSITION:
				el_iter++;
				currScriptElement = NULL;
				break;
			case NEXT_COMMAND_WAITING:
				*new_command = NULL;
				break;
			case NEXT_COMMAND_KEEP:
			case NEXT_COMMAND_RESYNC:
			case NEXT_COMMAND_ACQUSITION_IMAGE:
			case NEXT_COMMAND_WAIT_SIGNAL:
			case NEXT_COMMAND_WAIT_MIRROR:
			case NEXT_COMMAND_WAIT_SEARCH:
				// keep us
				break;
		}
	}
	if (ret != NEXT_COMMAND_NEXT)
		executedCount++;
	return ret;
}
#endif							 /* ! __RTS2_SCRIPT__ */
