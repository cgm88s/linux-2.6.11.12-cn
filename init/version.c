/*
 *  linux/init/version.c
 *
 *  Copyright (C) 1992  Theodore Ts'o
 *
 *  May be freely distributed as part of Linux.
 */

#include <linux/compile.h>
#include <linux/module.h>
#include <linux/uts.h>
#include <linux/utsname.h>
#include <linux/version.h>

#define version(a) Version_ ## a
#define version_string(a) version(a)

int version_string(LINUX_VERSION_CODE);
// uname -a 显示的信息
struct new_utsname system_utsname = {
	.sysname	= UTS_SYSNAME,	//系统名称，固定为 linux
	.nodename	= UTS_NODENAME, //主机名，由 sethostname 设置
	.release	= UTS_RELEASE,  //发行版本号
	.version	= UTS_VERSION,  //发行日期
	.machine	= UTS_MACHINE,  // 机器类型
	.domainname	= UTS_DOMAINNAME, // 域名， 由 setdomainname 设置
};

EXPORT_SYMBOL(system_utsname);

const char linux_banner[] =
	"Linux version " UTS_RELEASE " (" LINUX_COMPILE_BY "@"
	LINUX_COMPILE_HOST ") (" LINUX_COMPILER ") " UTS_VERSION "\n";
