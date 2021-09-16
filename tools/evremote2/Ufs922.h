#ifndef _ufs922_
#define _ufs922_

#define VFDDISPLAYCHARS 0xc0425a00
#define VFDSETRCCODE    0xc0425af6
#define VFDSETLED       0xc0425afe
#define VFDSETMODE      0xc0425aff

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

/* this sets the mode temporarily (for one ioctl)
 * to the desired mode. currently the "normal" mode
 * is the compatible vfd mode
 */
struct set_mode_s
{
	int compat; /* 0 = compatibility mode to vfd driver; 1 = micom mode */
};

struct micom_ioctl_data
{
	union
	{
		struct set_icon_s icon;
		struct set_led_s led;
		struct set_brightness_s brightness;
		struct set_mode_s mode;
	} u;
};

struct vfd_ioctl_data
{
	unsigned char start_address;
	unsigned char data[64];
	unsigned char length;
};

#endif  // _ufs922_
// vim:ts=4
