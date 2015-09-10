/* Copyright © 2014 cat <cat@wolfgirl.org>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
 */

#ifndef UTIL_DEBUG_H
#define UTIL_DEBUG_H

#if defined(__GNUC__)
	#define ___PREFUNCTION__ __PRETTY_FUNCTION__
#else
	#define ___PREFUNCTION__ __FUNCTION__
#endif

#define __PFUNC_SEP__ '\t'

#define pdbg qDebug() << ___PREFUNCTION__ << __PFUNC_SEP__
#define pwarn qWarning() << ___PREFUNCTION__ << __PFUNC_SEP__


#endif // UTIL_DEBUG_H
