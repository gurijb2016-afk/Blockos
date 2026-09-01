#include "usb_hid.hpp"
namespace usb::hid {
bool parse_boot_keyboard(const uint8_t*d,size_t n,KeyboardReport*o){if(!d||!o||n<8)return false;o->modifiers=d[0];o->reserved=d[1];for(int i=0;i<6;i++)o->keys[i]=d[2+i];return true;}
bool parse_boot_mouse(const uint8_t*d,size_t n,MouseReport*o){if(!d||!o||n<3)return false;o->buttons=d[0];o->x=(int8_t)d[1];o->y=(int8_t)d[2];o->wheel=n>3?(int8_t)d[3]:0;return true;}
const char* key_name(uint8_t u){static const char* t[0x3D]={}; if(u>=0x04&&u<=0x1D){static const char* a[]={"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"};return a[u-4];} if(u>=0x1E&&u<=0x27){static const char* n[]={"1","2","3","4","5","6","7","8","9","0"};return n[u-0x1E];} switch(u){case 0x28:return "Enter";case 0x29:return "Escape";case 0x2A:return "Backspace";case 0x2B:return "Tab";case 0x2C:return "Space";default:return "Unknown";}}
}
