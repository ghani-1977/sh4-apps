#ifndef __ufs922__
#define __ufs922__

#define VFDSETFAN        0xc0425af6
#define VFDGETVERSION    0xc0425af7
#if defined VFDGETWAKEUPTIME
#undef VFDGETWAKEUPTIME
#endif
#define VFDGETWAKEUPTIME 0xc0425b00
#define VFDSETWAKEUPTIME 0xc0425b04

/* this sets up the mode temporarily (for one ioctl)
 * to the desired mode. currently the "normal" mode
 * is the compatible vfd mode
 */
struct set_mode_s
{
	int compat; /* 0 = compatibility mode to vfd driver; 1 = micom mode */
};

struct set_brightness_s
{
	int level;
};

struct set_icon_s
{
	int icon_nr;
	int on;
};

struct set_led_s
{
	int led_nr;
	int on;
};

struct set_light_s
{
	int onoff;
};

/* time must be given as follows:
 * time[0] & time[1] = mjd ???
 * time[2] = hour
 * time[3] = min
 * time[4] = sec
 */
struct set_standby_s
{
	char time[5];
};

struct set_time_s
{
	char time[5];
};

struct set_fan_s
{
	int speed;
};

struct micom_ioctl_data
{
	union
	{
		struct set_icon_s icon;
		struct set_led_s led;
		struct set_brightness_s brightness;
		struct set_light_s light;
		struct set_mode_s mode;
		struct set_standby_s standby;
		struct set_time_s time;
		struct set_fan_s fan;
	} u;
};
#endif  // __ufs922__
// vim:ts=4
