#ifndef _CTRL_H_
#define _CTRL_H_

#include "config.h"

#define APPCTRL_DEFAULT_PORT        "18276"

#define APPCTL_TERMINATOR    "\r\n\r\n"

#define APPCTL_UNDEF         0
#define APPCTL_STATUS        1
#define APPCTL_LOG           2

int
ctrl_init(struct app_config *cfg);

#endif  /* _CTRL_H_ */
